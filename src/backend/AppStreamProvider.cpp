#include "AppStreamProvider.h"

#include <QDebug>
#include <QDir>
#include <QSet>
#include <QLocale>
#include <QTextDocumentFragment>
#include <QFileInfo>
#include <QByteArray>
#include <QUrl>
#include <limits>
#include <memory>

#ifdef HAVE_APPSTREAM_QT
#include <AppStreamQt/pool.h>
#include <AppStreamQt/component.h>
#include <AppStreamQt/component-box.h>
#include <AppStreamQt/developer.h>
#include <AppStreamQt/icon.h>
#include <AppStreamQt/metadata.h>
#include <AppStreamQt/screenshot.h>
#include <AppStreamQt/image.h>
#endif

struct AppStreamProvider::PoolHolder {
#ifdef HAVE_APPSTREAM_QT
    AppStream::Pool *pool = nullptr;
#endif
};

namespace {
QString proxyFlathubScreenshotUrl(const QUrl &url)
{
    const QString raw = url.toString();
    QString source = raw;
    if (raw.startsWith(QStringLiteral("https://dl.flathub.org/repo/screenshots/"))) {
        source = QStringLiteral("https://dl.flathub.org/media/")
                 + raw.mid(QStringLiteral("https://dl.flathub.org/repo/screenshots/").size());
    } else if (!raw.startsWith(QStringLiteral("https://dl.flathub.org/"))) {
        return raw;
    }

    QByteArray encoded = source.toUtf8().toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    return QStringLiteral("https://imgproxy.flathub.org/insecure/q:90/f:avif/%1")
            .arg(QString::fromUtf8(encoded));
}

int scoreIconCandidate(const AppStream::Icon &icon)
{
    int score = 0;
    const int pixels = static_cast<int>(icon.width() * icon.height());
    if (icon.kind() == AppStream::Icon::KindLocal || icon.kind() == AppStream::Icon::KindCached) {
        score += 100000;
    } else if (icon.kind() == AppStream::Icon::KindStock) {
        score += 50000;
    } else if (icon.kind() == AppStream::Icon::KindRemote) {
        score += 25000;
    }
    score += pixels;
    return score;
}
}

AppStreamProvider::AppStreamProvider(QObject *parent)
    : QObject(parent)
    , m_pool(std::make_unique<PoolHolder>())
{
}

AppStreamProvider::~AppStreamProvider()
{
#ifdef HAVE_APPSTREAM_QT
    if (m_pool && m_pool->pool) {
        delete m_pool->pool;
        m_pool->pool = nullptr;
    }
#endif
    // m_pool (unique_ptr<PoolHolder>) is destroyed here
}

bool AppStreamProvider::isAvailable() const
{
    ensurePoolLoaded();
    return m_available;
}

void AppStreamProvider::enrich(AppInfo &info) const
{
#ifdef HAVE_APPSTREAM_QT
    ensurePoolLoaded();
    if (!m_available || !m_pool->pool)
        return;

    AppStream::ComponentBox box = m_pool->pool->componentsById(info.id);
    if (box.toList().isEmpty()) {
        box = m_pool->pool->componentsById(info.id + QStringLiteral(".desktop"));
    }
    if (box.toList().isEmpty() && info.id.endsWith(QStringLiteral(".desktop"))) {
        box = m_pool->pool->componentsById(info.id.left(info.id.size() - 8));
    }

    const QList<AppStream::Component> list = box.toList();
    if (list.isEmpty())
    {
        if (debugEnabled() && !m_loggedMisses.contains(info.id)) {
            qDebug() << "AppStreamProvider: no component for id" << info.id;
            m_loggedMisses.insert(info.id);
        }
        return;
    }

    if (debugEnabled()) {
        qDebug() << "AppStreamProvider: enriched component for id" << info.id
                 << "with" << list.first().screenshotsAll().size() << "screenshots";
    }

    const AppStream::Component &comp = list.first();
    if (info.longDescription.isEmpty()) {
        const QString desc = comp.description();
        if (!desc.isEmpty()) {
            const QString plain = QTextDocumentFragment::fromHtml(desc).toPlainText().trimmed();
            info.longDescription = plain.isEmpty() ? desc : plain;
        }
    }
    if (info.screenshotUrls.isEmpty()) {
        QSet<QString> seenUrls;
        const auto screenshots = comp.screenshotsAll();
        for (const AppStream::Screenshot &shot : screenshots) {
            if (shot.mediaKind() != AppStream::Screenshot::MediaKindImage)
                continue;
            // Prefer one representative image per screenshot (avoids many size variants).
            QUrl candidate;
            const std::optional<AppStream::Image> picked = shot.image(1280, 720, 1);
            if (picked.has_value() && picked->kind() == AppStream::Image::KindSource && !picked->url().isEmpty()) {
                candidate = picked->url();
            } else {
                const auto images = shot.imagesAll();
                int bestPixels = -1;
                for (const AppStream::Image &img : images) {
                    if (img.url().isEmpty())
                        continue;
                    const int pixels = static_cast<int>(img.width() * img.height());
                    // Prefer source images, otherwise largest available.
                    if (img.kind() == AppStream::Image::KindSource) {
                        candidate = img.url();
                        break;
                    }
                    if (pixels > bestPixels) {
                        bestPixels = pixels;
                        candidate = img.url();
                    }
                }
            }

            if (!candidate.isEmpty()) {
                const QString normalizedUrl = proxyFlathubScreenshotUrl(candidate);
                const QString key = QUrl(normalizedUrl).toString(QUrl::RemoveQuery | QUrl::RemoveFragment);
                if (!seenUrls.contains(key)) {
                    seenUrls.insert(key);
                    info.screenshotUrls.append(normalizedUrl);
                }
            }
        }
    }
    if (info.developerName.isEmpty()) {
        const QString devName = comp.developer().name().trimmed();
        if (!devName.isEmpty()) {
            info.developerName = devName;
        } else {
            const QString projectGroup = comp.projectGroup().trimmed();
            if (!projectGroup.isEmpty())
                info.developerName = projectGroup;
        }
    }
    if (info.projectLicense.isEmpty()) {
        const QString license = comp.projectLicense().trimmed();
        if (!license.isEmpty())
            info.projectLicense = license;
    }
    if (info.homepageUrl.isEmpty()) {
        const QUrl homepage = comp.url(AppStream::Component::UrlKindHomepage);
        if (!homepage.isEmpty())
            info.homepageUrl = homepage.toString();
    }
    if (info.bugtrackerUrl.isEmpty()) {
        const QUrl bugtracker = comp.url(AppStream::Component::UrlKindBugtracker);
        if (!bugtracker.isEmpty())
            info.bugtrackerUrl = bugtracker.toString();
    }
    if (info.donateUrl.isEmpty()) {
        const QUrl donate = comp.url(AppStream::Component::UrlKindDonation);
        if (!donate.isEmpty())
            info.donateUrl = donate.toString();
    }
    if (info.helpUrl.isEmpty()) {
        const QUrl help = comp.url(AppStream::Component::UrlKindHelp);
        if (!help.isEmpty())
            info.helpUrl = help.toString();
    }
    if (info.translateUrl.isEmpty()) {
        const QUrl translate = comp.url(AppStream::Component::UrlKindTranslate);
        if (!translate.isEmpty())
            info.translateUrl = translate.toString();
    }
    if (info.categories.isEmpty()) {
        info.categories = comp.categories();
    }
    AppStream::Icon selectedIcon;
    int bestScore = std::numeric_limits<int>::min();
    const auto icons = comp.icons();
    for (const AppStream::Icon &candidate : icons) {
        if (candidate.isEmpty())
            continue;
        const int score = scoreIconCandidate(candidate);
        if (score > bestScore) {
            bestScore = score;
            selectedIcon = candidate;
        }
    }
    if (selectedIcon.isEmpty()) {
        selectedIcon = comp.icon(QSize(128, 128));
    }

    if (!selectedIcon.isEmpty()) {
        if (!selectedIcon.name().isEmpty())
            info.iconName = selectedIcon.name();
        if (info.iconUrl.isEmpty() && !selectedIcon.url().isEmpty())
            info.iconUrl = selectedIcon.url().toString();
    }

    if (info.iconUrl.isEmpty() && !info.iconName.isEmpty()) {
        // AppStream local/cached icons can be stored under icons/flatpak/<resolution>/<filename>.
        const QStringList iconBaseDirs = {
            QDir::homePath() + QStringLiteral("/.local/share/flatpak/exports/share/app-info"),
            QStringLiteral("/var/lib/flatpak/exports/share/app-info")
        };
        const QList<QSize> sizes = {
            QSize(128, 128), QSize(64, 64), QSize(256, 256), QSize(96, 96), QSize(48, 48)
        };
        for (const QString &base : iconBaseDirs) {
            for (const QSize &s : sizes) {
                const QString path = QStringLiteral("%1/icons/flatpak/%2x%3/%4")
                        .arg(base)
                        .arg(s.width())
                        .arg(s.height())
                        .arg(info.iconName);
                if (QFileInfo::exists(path)) {
                    info.iconUrl = QUrl::fromLocalFile(path).toString();
                    break;
                }
            }
            if (!info.iconUrl.isEmpty())
                break;
        }
    }

    if (!info.iconUrl.isEmpty()) {
        // Normalize plain absolute paths so UI can load them as local files.
        const QFileInfo fi(info.iconUrl);
        if (fi.isAbsolute() && fi.exists()) {
            info.iconUrl = QUrl::fromLocalFile(fi.absoluteFilePath()).toString();
        }
    }
    if (debugEnabled()) {
        qDebug() << "AppStreamProvider: icon for" << info.id
                 << "name=" << info.iconName
                 << "url=" << info.iconUrl
                 << "screenshots=" << info.screenshotUrls.size();
    }
#else
    Q_UNUSED(info);
#endif
}

void AppStreamProvider::ensurePoolLoaded() const
{
    if (m_poolTried)
        return;
    m_poolTried = true;

#ifdef HAVE_APPSTREAM_QT
    m_pool->pool = new AppStream::Pool();
    m_pool->pool->setLocale(QLocale().name());
    AppStream::Pool::Flags flags = AppStream::Pool::FlagNone;
    flags |= AppStream::Pool::FlagLoadFlatpak;
    flags |= AppStream::Pool::FlagLoadOsMetainfo;
    flags |= AppStream::Pool::FlagLoadOsCatalog;
    m_pool->pool->setFlags(flags);

    const QString userFlatpakExport = QDir::homePath() + QStringLiteral("/.local/share/flatpak/exports/share");
    const QString systemFlatpakExport = QStringLiteral("/var/lib/flatpak/exports/share");
    if (QDir(userFlatpakExport).exists()) {
        m_pool->pool->addExtraDataLocation(userFlatpakExport, AppStream::Metadata::FormatStyleUnknown);
    }
    if (QDir(systemFlatpakExport).exists()) {
        m_pool->pool->addExtraDataLocation(systemFlatpakExport, AppStream::Metadata::FormatStyleUnknown);
    }

    m_available = m_pool->pool->load();
    if (!m_available) {
        qWarning() << "AppStreamProvider: pool load failed:" << m_pool->pool->lastError();
        delete m_pool->pool;
        m_pool->pool = nullptr;
    } else if (debugEnabled()) {
        qDebug() << "AppStreamProvider: pool loaded successfully";
    }
#endif
}

bool AppStreamProvider::debugEnabled() const
{
    return qEnvironmentVariableIsSet("CURIO_APPSTREAM_DEBUG");
}
