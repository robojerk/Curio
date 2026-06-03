#include "AppDetailsWidget.h"

#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPalette>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>

#include "backend/FlatpakBackend.h"
#include "backend/FlatpakRemoteCatalog.h"
#include "backend/TrackedBuildClassifier.h"

namespace {

bool isGithubHost(const QUrl &url)
{
    return url.isValid() && url.host().compare(QStringLiteral("github.com"), Qt::CaseInsensitive) == 0;
}

QString githubUrlForApp(const AppInfo &info)
{
    if (!info.vcsUrl.isEmpty()) {
        const QUrl vcs(info.vcsUrl);
        if (isGithubHost(vcs))
            return info.vcsUrl;
    }
    if (!info.homepageUrl.isEmpty()) {
        const QUrl home(info.homepageUrl);
        if (isGithubHost(home))
            return info.homepageUrl;
    }
    return QString();
}

bool isVersionOnlySummary(const AppInfo &info)
{
    if (info.summary.isEmpty() || info.version.isEmpty())
        return false;
    return info.summary == QObject::tr("Version %1").arg(info.version)
            || info.summary == QStringLiteral("Version ") + info.version;
}

bool isReasonableIconShape(const QPixmap &pm)
{
    if (pm.isNull() || pm.width() <= 0 || pm.height() <= 0)
        return false;
    const double ratio = static_cast<double>(pm.width()) / static_cast<double>(pm.height());
    return ratio >= 0.5 && ratio <= 2.0;
}

QPixmap normalizedIconPixmap(const QPixmap &pm, int size)
{
    QPixmap scaled = pm.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    scaled.setDevicePixelRatio(1.0);
    return scaled;
}
}

AppDetailsWidget::AppDetailsWidget(FlatpakBackend *backend, QWidget *parent)
    : QWidget(parent)
    , m_backend(backend)
{
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    // Top bar: Back only
    auto *topRow = new QHBoxLayout;
    auto *backButton = new QPushButton(tr("← Back"), this);
    connect(backButton, &QPushButton::clicked, this, &AppDetailsWidget::backClicked);
    topRow->addWidget(backButton);
    topRow->addStretch();
    rootLayout->addLayout(topRow);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setBackgroundRole(QPalette::Window);

    auto *content = new QWidget(this);
    auto *layout = new QVBoxLayout(content);
    layout->setAlignment(Qt::AlignTop);

    // Header: icon + title + id/version + action buttons
    auto *headerLayout = new QHBoxLayout;
    m_iconLabel = new QLabel(this);
    const int iconSize = 96;
    m_iconLabel->setFixedSize(iconSize, iconSize);
    headerLayout->addWidget(m_iconLabel);

    auto *titlesLayout = new QVBoxLayout;
    m_titleLabel = new QLabel(this);
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() + 3);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    m_idLabel = new QLabel(this);
    m_idLabel->setStyleSheet(QStringLiteral("color: gray;"));
    m_developerLabel = new QLabel(this);
    m_developerLabel->setStyleSheet(QStringLiteral("color: gray;"));
    m_licenseLabel = new QLabel(this);
    m_licenseLabel->setStyleSheet(QStringLiteral("color: gray;"));
    m_originLabel = new QLabel(this);
    m_originLabel->setStyleSheet(QStringLiteral("color: gray;"));
    titlesLayout->addWidget(m_titleLabel);
    titlesLayout->addWidget(m_idLabel);
    titlesLayout->addWidget(m_originLabel);
    titlesLayout->addWidget(m_developerLabel);
    titlesLayout->addWidget(m_licenseLabel);
    titlesLayout->setSpacing(2);
    headerLayout->addLayout(titlesLayout);
    headerLayout->addStretch();

    m_installButton = new QPushButton(tr("Install"), this);
    m_removeButton = new QPushButton(tr("Remove"), this);
    m_updateButton = new QPushButton(tr("Update"), this);
    m_manageButton = new QPushButton(tr("Manage"), this);
    m_manageButton->setEnabled(false);

    auto *buttonsLayout = new QVBoxLayout;
    buttonsLayout->setSpacing(6);
    buttonsLayout->addWidget(m_installButton);
    buttonsLayout->addWidget(m_removeButton);
    buttonsLayout->addWidget(m_updateButton);
    buttonsLayout->addWidget(m_manageButton);
    headerLayout->addLayout(buttonsLayout);

    layout->addLayout(headerLayout);
    layout->addSpacing(20);

    QFont sectionFont = m_titleLabel->font();
    sectionFont.setPointSizeF(sectionFont.pointSizeF() + 1);
    sectionFont.setBold(true);
    auto *linksHeading = new QLabel(tr("Links"), this);
    linksHeading->setFont(sectionFont);
    m_linksView = new QTextBrowser(this);
    m_linksView->setOpenExternalLinks(true);
    m_linksView->setFrameShape(QFrame::NoFrame);
    m_linksView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_linksView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_linksView->setMaximumHeight(120);
    m_linksView->setVisible(false);

    auto *tagsHeading = new QLabel(tr("Tags"), this);
    tagsHeading->setFont(sectionFont);
    m_tagsLabel = new QLabel(this);
    m_tagsLabel->setWordWrap(true);
    m_tagsLabel->setStyleSheet(QStringLiteral("color: gray;"));
    m_tagsLabel->setVisible(false);

    // Screenshots
    auto *shotsHeading = new QLabel(tr("Screenshots"), this);
    shotsHeading->setFont(sectionFont);
    layout->addWidget(shotsHeading);
    m_screenshotsPlaceholder = new QLabel(tr("No screenshots available for this app."), this);
    m_screenshotsPlaceholder->setStyleSheet(
        QStringLiteral("color: gray; padding: 24px; border: 1px dashed gray; border-radius: 6px;"));
    m_screenshotsPlaceholder->setAlignment(Qt::AlignCenter);
    m_screenshotsPlaceholder->setMinimumHeight(120);
    layout->addWidget(m_screenshotsPlaceholder);

    m_screenshotsScroll = new QScrollArea(this);
    m_screenshotsScroll->setWidgetResizable(true);
    m_screenshotsScroll->setFrameShape(QFrame::NoFrame);
    m_screenshotsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_screenshotsScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_screenshotsScroll->setMinimumHeight(140);
    m_screenshotsContainer = new QWidget(this);
    m_screenshotsContainer->setLayout(new QHBoxLayout(m_screenshotsContainer));
    m_screenshotsScroll->setWidget(m_screenshotsContainer);
    m_screenshotsScroll->setVisible(false);
    layout->addWidget(m_screenshotsScroll);
    layout->addSpacing(16);

    // Description section
    auto *descHeading = new QLabel(tr("Description"), this);
    descHeading->setFont(sectionFont);
    layout->addWidget(descHeading);

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setWordWrap(true);
    m_summaryLabel->setTextFormat(Qt::PlainText);
    m_summaryLabel->setAlignment(Qt::AlignTop);
    m_summaryLabel->setStyleSheet(QStringLiteral("padding: 8px 0;"));
    layout->addWidget(m_summaryLabel);
    layout->addSpacing(16);

    layout->addWidget(linksHeading);
    layout->addWidget(m_linksView);
    layout->addSpacing(16);

    // Keep tags as the final section.
    layout->addWidget(tagsHeading);
    layout->addWidget(m_tagsLabel);

    m_network = new QNetworkAccessManager(this);

    layout->addStretch(1);
    scroll->setWidget(content);
    rootLayout->addWidget(scroll);

    connect(m_installButton, &QPushButton::clicked, this, &AppDetailsWidget::onInstallClicked);
    connect(m_removeButton, &QPushButton::clicked, this, &AppDetailsWidget::onRemoveClicked);
    connect(m_updateButton, &QPushButton::clicked, this, &AppDetailsWidget::onUpdateClicked);
    connect(m_manageButton, &QPushButton::clicked, this, &AppDetailsWidget::onManageClicked);
}

void AppDetailsWidget::setApp(const AppInfo &info)
{
    m_info = info;
    m_titleLabel->setText(info.name);
    m_idLabel->setText(info.version.isEmpty()
                           ? info.id
                           : QStringLiteral("%1 • %2").arg(info.id, info.version));
    const QString originName = FlatpakRemoteCatalog::displayNameForOrigin(info.repoId);
    if (!originName.isEmpty()) {
        m_originLabel->setText(tr("Installed from: %1").arg(originName));
        m_originLabel->setVisible(true);
    } else {
        m_originLabel->clear();
        m_originLabel->setVisible(false);
    }
    m_developerLabel->setText(info.developerName.isEmpty()
                                  ? QString()
                                  : QStringLiteral("By %1").arg(info.developerName));
    m_developerLabel->setVisible(!info.developerName.isEmpty());
    m_licenseLabel->setText(info.projectLicense.isEmpty()
                                ? QString()
                                : QStringLiteral("License: %1").arg(info.projectLicense));
    m_licenseLabel->setVisible(!info.projectLicense.isEmpty());

    QString description = info.longDescription;
    if (description.isEmpty() && !isVersionOnlySummary(info))
        description = info.summary;
    m_summaryLabel->setText(description);

    QStringList links;
    const QString githubUrl = githubUrlForApp(info);
    if (!githubUrl.isEmpty()) {
        links << QStringLiteral("<a href=\"%1\">GitHub</a>").arg(githubUrl.toHtmlEscaped());
    }
    if (!info.homepageUrl.isEmpty() && info.homepageUrl != githubUrl) {
        links << QStringLiteral("<a href=\"%1\">Project Website</a>")
                       .arg(info.homepageUrl.toHtmlEscaped());
    }
    if (!info.vcsUrl.isEmpty() && info.vcsUrl != githubUrl && info.vcsUrl != info.homepageUrl) {
        links << QStringLiteral("<a href=\"%1\">Source Repository</a>")
                       .arg(info.vcsUrl.toHtmlEscaped());
    }
    const QString catalogUrl = FlatpakRemoteCatalog::catalogPageUrlForApp(info.repoId, info.id);
    if (!catalogUrl.isEmpty()) {
        const QString catalogLabel = FlatpakRemoteCatalog::catalogPageLabelForOrigin(info.repoId);
        links << QStringLiteral("<a href=\"%1\">%2</a>")
                     .arg(catalogUrl.toHtmlEscaped(), catalogLabel.toHtmlEscaped());
    } else if (!info.remoteRepoUrl.isEmpty()) {
        links << QStringLiteral("<a href=\"%1\">Repository</a>")
                     .arg(info.remoteRepoUrl.toHtmlEscaped());
    }
    if (!info.bugtrackerUrl.isEmpty())
        links << QStringLiteral("<a href=\"%1\">Issue Tracker</a>").arg(info.bugtrackerUrl.toHtmlEscaped());
    if (!info.helpUrl.isEmpty())
        links << QStringLiteral("<a href=\"%1\">Help</a>").arg(info.helpUrl.toHtmlEscaped());
    if (!info.donateUrl.isEmpty())
        links << QStringLiteral("<a href=\"%1\">Donate</a>").arg(info.donateUrl.toHtmlEscaped());
    if (!info.translateUrl.isEmpty())
        links << QStringLiteral("<a href=\"%1\">Translate</a>").arg(info.translateUrl.toHtmlEscaped());
    if (!info.trackedStableAssetUrl.isEmpty()) {
        links << QStringLiteral("<a href=\"%1\">Tracked Release Build</a>")
                     .arg(info.trackedStableAssetUrl.toHtmlEscaped());
    }
    if (!info.trackedNightlyAssetUrl.isEmpty()) {
        links << QStringLiteral("<a href=\"%1\">Tracked Pre-release Build</a>")
                     .arg(info.trackedNightlyAssetUrl.toHtmlEscaped());
    }

    if (links.isEmpty()) {
        m_linksView->setVisible(false);
    } else {
        m_linksView->setHtml(links.join(QStringLiteral("<br/>")));
        m_linksView->setVisible(true);
    }

    if (info.categories.isEmpty()) {
        if (!info.trackedStableVersion.isEmpty() || !info.trackedNightlyVersion.isEmpty()) {
            QStringList trackedLines;
            if (!info.trackedStableVersion.isEmpty()) {
                trackedLines << tr("Release: %1 (%2)")
                                .arg(info.trackedStableVersion,
                                     info.trackedStablePublishedAtIso.isEmpty() ? tr("date unknown")
                                                                                 : info.trackedStablePublishedAtIso);
            }
            if (!info.trackedNightlyVersion.isEmpty()) {
                trackedLines << tr("Pre-release: %1 (%2)")
                                .arg(info.trackedNightlyVersion,
                                     info.trackedNightlyPublishedAtIso.isEmpty() ? tr("date unknown")
                                                                                  : info.trackedNightlyPublishedAtIso);
            }
            if (!info.trackedBuildLastError.isEmpty())
                trackedLines << tr("Tracked feed error: %1").arg(info.trackedBuildLastError);
            m_tagsLabel->setText(trackedLines.join(QStringLiteral("\n")));
            m_tagsLabel->setVisible(true);
        } else {
            m_tagsLabel->setVisible(false);
        }
    } else {
        QString tagsText = info.categories.join(QStringLiteral("   "));
        QStringList trackedLines;
        if (!info.trackedStableVersion.isEmpty()) {
            trackedLines << tr("Release: %1 (%2)")
                            .arg(info.trackedStableVersion,
                                 info.trackedStablePublishedAtIso.isEmpty() ? tr("date unknown")
                                                                             : info.trackedStablePublishedAtIso);
        }
        if (!info.trackedNightlyVersion.isEmpty()) {
            trackedLines << tr("Pre-release: %1 (%2)")
                            .arg(info.trackedNightlyVersion,
                                 info.trackedNightlyPublishedAtIso.isEmpty() ? tr("date unknown")
                                                                              : info.trackedNightlyPublishedAtIso);
        }
        if (!trackedLines.isEmpty())
            tagsText += QStringLiteral("\n") + trackedLines.join(QStringLiteral("\n"));
        m_tagsLabel->setVisible(true);
        m_tagsLabel->setText(tagsText);
    }

    QIcon fallback = QIcon::fromTheme(QStringLiteral("application-x-executable"));
    if (fallback.isNull())
        fallback = QIcon::fromTheme(QStringLiteral("applications-internet"));
    if (!fallback.isNull())
        m_iconLabel->setPixmap(fallback.pixmap(96, 96));
    else
        m_iconLabel->clear();

    QIcon icon = QIcon::fromTheme(info.iconName.isEmpty() ? info.id : info.iconName);
    if (!icon.isNull()) {
        m_iconLabel->setPixmap(icon.pixmap(96, 96));
    } else if (!info.iconUrl.isEmpty()) {
        QUrl url(info.iconUrl);
        QPixmap pm;
        if (url.isLocalFile())
            pm.load(url.toLocalFile());
        else if (url.scheme().isEmpty() && info.iconUrl.startsWith(QLatin1Char('/')))
            pm.load(info.iconUrl);
        else if (url.scheme() == QLatin1String("http") || url.scheme() == QLatin1String("https")) {
            const QString expectedUrl = url.toString();
            QNetworkReply *reply = m_network->get(QNetworkRequest(url));
            connect(reply, &QNetworkReply::finished, this, [this, reply, expectedUrl]() {
                if (reply->error() == QNetworkReply::NoError) {
                    QPixmap downloaded;
                    if (m_info.iconUrl == expectedUrl &&
                        downloaded.loadFromData(reply->readAll()) &&
                        isReasonableIconShape(downloaded)) {
                        m_iconLabel->setPixmap(normalizedIconPixmap(downloaded, 96));
                    }
                }
                reply->deleteLater();
            });
        }
        else
            pm.load(info.iconUrl);
        if (!pm.isNull() && isReasonableIconShape(pm))
            m_iconLabel->setPixmap(normalizedIconPixmap(pm, 96));
    }

    // Screenshots: show placeholder or load images from URLs
    auto *box = qobject_cast<QHBoxLayout *>(m_screenshotsContainer->layout());
    while (box && box->count()) {
        QLayoutItem *item = box->takeAt(0);
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
    if (info.screenshotUrls.isEmpty()) {
        m_screenshotsPlaceholder->setVisible(true);
        m_screenshotsScroll->setVisible(false);
    } else {
        m_screenshotsPlaceholder->setVisible(false);
        m_screenshotsScroll->setVisible(true);
        const int maxShots = 8;
        for (int i = 0; i < qMin(info.screenshotUrls.size(), maxShots); ++i) {
            auto *label = new QLabel(m_screenshotsContainer);
            label->setFixedSize(220, 140);
            label->setAlignment(Qt::AlignCenter);
            label->setStyleSheet(QStringLiteral("border: 1px solid gray; border-radius: 4px; background: palette(base);"));
            label->setScaledContents(false);
            box->addWidget(label);
            QNetworkRequest req(QUrl(info.screenshotUrls.at(i)));
            QNetworkReply *reply = m_network->get(req);
            connect(reply, &QNetworkReply::finished, this, [reply, label]() {
                if (reply->error() == QNetworkReply::NoError) {
                    QPixmap pm;
                    if (pm.loadFromData(reply->readAll()))
                        label->setPixmap(pm.scaled(220, 140, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                }
                reply->deleteLater();
            });
        }
    }

    updateButtonStates();
}

void AppDetailsWidget::onInstallClicked()
{
    if (m_backend)
        m_backend->install(m_info.id, m_info.repoId);
}

void AppDetailsWidget::onRemoveClicked()
{
    if (m_backend)
        m_backend->uninstall(m_info.id);
}

void AppDetailsWidget::onUpdateClicked()
{
    if (m_backend)
        m_backend->update(m_info.id, m_info.version);
}

void AppDetailsWidget::onManageClicked()
{
    // Placeholder: future permissions / halt updates
}

void AppDetailsWidget::updateButtonStates()
{
    m_installButton->setEnabled(!m_info.installed);
    m_removeButton->setEnabled(m_info.installed);
    const bool trackedUpdate = m_info.installed
            && !m_info.trackedStableAssetUrl.isEmpty()
            && TrackedBuildClassifier::isReleaseNewerThanInstalled(m_info.version,
                                                                   m_info.trackedStableVersion);
    m_updateButton->setEnabled(m_info.installed);
    if (trackedUpdate && !m_info.trackedStableVersion.isEmpty()) {
        m_updateButton->setText(tr("Update to %1").arg(m_info.trackedStableVersion));
        m_updateButton->setToolTip(m_info.trackedStableAssetUrl);
    } else {
        m_updateButton->setText(tr("Update"));
        m_updateButton->setToolTip(QString());
    }
}
