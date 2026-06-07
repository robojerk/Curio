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
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QTimer>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>

#include "backend/FlatpakBackend.h"
#include "backend/FlatpakRemoteCatalog.h"
#include "backend/TrackedBuildClassifier.h"
#include "models/Operation.h"

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
    m_installButton->setFixedHeight(36);
    m_installProgress = new QProgressBar(m_installButton);
    m_installProgress->setTextVisible(false);
    m_installProgress->setRange(0, 100);
    m_installProgress->setValue(0);
    m_installProgress->hide();
    m_installProgress->setStyleSheet(QStringLiteral(
        "QProgressBar {"
        "  border: 1px solid rgba(255,255,255,0.12);"
        "  border-radius: 8px;"
        "  background-color: rgba(80,80,88,0.55);"
        "}"
        "QProgressBar::chunk {"
        "  border-radius: 7px;"
        "  background-color: rgba(90,160,220,0.95);"
        "}"
    ));
    m_installProgressTimer = new QTimer(this);
    m_installProgressTimer->setInterval(45);
    connect(m_installProgressTimer, &QTimer::timeout, this, &AppDetailsWidget::tickInstallProgress);

    m_launchButton = new QPushButton(tr("Launch"), this);
    m_launchButton->setFixedHeight(36);
    m_removeButton = new QPushButton(tr("Remove"), this);
    m_removeButton->setFixedHeight(36);
    m_updateButton = new QPushButton(tr("Update"), this);
    m_updateButton->setFixedHeight(36);
    m_manageButton = new QPushButton(tr("Manage"), this);
    m_manageButton->setFixedHeight(36);
    m_manageButton->setEnabled(false);

    auto *buttonsLayout = new QVBoxLayout;
    buttonsLayout->setSpacing(6);
    buttonsLayout->addWidget(m_installButton);
    buttonsLayout->addWidget(m_launchButton);
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

    m_releaseHeading = new QLabel(this);
    m_releaseHeading->setFont(sectionFont);
    m_releaseHeading->setVisible(false);
    layout->addWidget(m_releaseHeading);

    m_releaseLabel = new QLabel(this);
    m_releaseLabel->setWordWrap(true);
    m_releaseLabel->setTextFormat(Qt::PlainText);
    m_releaseLabel->setAlignment(Qt::AlignTop);
    m_releaseLabel->setStyleSheet(QStringLiteral("padding: 8px 0;"));
    m_releaseLabel->setVisible(false);
    layout->addWidget(m_releaseLabel);
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
    connect(m_launchButton, &QPushButton::clicked, this, &AppDetailsWidget::onLaunchClicked);
    connect(m_backend, &FlatpakBackend::operationStarted, this, [this](const Operation &op) {
        if (op.appId != m_info.id)
            return;
        if (op.type == OperationType::Install)
            setInstallInProgress(true, op.progress);
        else if (op.type == OperationType::Uninstall && op.status == OperationStatus::Running)
            m_removeButton->setEnabled(false);
    });
    connect(m_backend, &FlatpakBackend::operationProgress, this, [this](const Operation &op) {
        if (op.appId != m_info.id || op.type != OperationType::Install)
            return;
        setInstallInProgress(true, op.progress);
    });
    connect(m_backend, &FlatpakBackend::operationFinished, this, [this](const Operation &op) {
        if (op.appId != m_info.id)
            return;
        if (op.type == OperationType::Install)
            setInstallInProgress(false);
        if (op.type == OperationType::Install && op.status == OperationStatus::Succeeded)
            m_info.installed = true;
        if (op.type == OperationType::Uninstall && op.status == OperationStatus::Succeeded)
            setInstalled(false);
        else
            updateButtonStates();
    });
    connect(m_removeButton, &QPushButton::clicked, this, &AppDetailsWidget::onRemoveClicked);
    connect(m_updateButton, &QPushButton::clicked, this, &AppDetailsWidget::onUpdateClicked);
    connect(m_manageButton, &QPushButton::clicked, this, &AppDetailsWidget::onManageClicked);
}

void AppDetailsWidget::cancelPendingLoads()
{
    ++m_loadGeneration;
    if (m_network)
        m_network->clearAccessCache();
}

void AppDetailsWidget::setInstalled(bool installed)
{
    if (m_info.installed == installed)
        return;
    m_info.installed = installed;
    if (!installed) {
        m_originLabel->clear();
        m_originLabel->setVisible(false);
    }
    updateButtonStates();
}

void AppDetailsWidget::applyMetadataPatch(const AppInfo &patch)
{
    if (patch.id != m_info.id)
        return;

    const auto fill = [](QString &dst, const QString &src) {
        if (!src.isEmpty())
            dst = src;
    };
    fill(m_info.name, patch.name);
    fill(m_info.summary, patch.summary);
    fill(m_info.longDescription, patch.longDescription);
    fill(m_info.version, patch.version);
    fill(m_info.iconUrl, patch.iconUrl);
    fill(m_info.iconName, patch.iconName);
    fill(m_info.developerName, patch.developerName);
    fill(m_info.projectLicense, patch.projectLicense);
    fill(m_info.repoId, patch.repoId);
    fill(m_info.homepageUrl, patch.homepageUrl);
    fill(m_info.vcsUrl, patch.vcsUrl);
    fill(m_info.bugtrackerUrl, patch.bugtrackerUrl);
    fill(m_info.helpUrl, patch.helpUrl);
    fill(m_info.donateUrl, patch.donateUrl);
    fill(m_info.translateUrl, patch.translateUrl);
    fill(m_info.latestReleaseVersion, patch.latestReleaseVersion);
    fill(m_info.latestReleaseNotes, patch.latestReleaseNotes);
    if (!patch.categories.isEmpty())
        m_info.categories = patch.categories;
    if (!patch.screenshotUrls.isEmpty())
        m_info.screenshotUrls = patch.screenshotUrls;

    setApp(m_info);
}

void AppDetailsWidget::setApp(const AppInfo &info)
{
    cancelPendingLoads();
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

    if (!info.latestReleaseVersion.isEmpty() || !info.latestReleaseNotes.isEmpty()) {
        if (!info.latestReleaseVersion.isEmpty()) {
            m_releaseHeading->setText(tr("Changes in version %1").arg(info.latestReleaseVersion));
        } else {
            m_releaseHeading->setText(tr("Release notes"));
        }
        m_releaseHeading->setVisible(true);
        m_releaseLabel->setText(info.latestReleaseNotes);
        m_releaseLabel->setVisible(!info.latestReleaseNotes.isEmpty());
    } else {
        m_releaseHeading->clear();
        m_releaseHeading->setVisible(false);
        m_releaseLabel->clear();
        m_releaseLabel->setVisible(false);
    }

    QStringList links;
    const QString githubUrl = githubUrlForApp(info);
    if (!githubUrl.isEmpty()) {
        links << QStringLiteral("<a href=\"%1\">GitHub</a>").arg(githubUrl.toHtmlEscaped());
    }
    if (!info.bugtrackerUrl.isEmpty())
        links << QStringLiteral("<a href=\"%1\">Report an Issue</a>").arg(info.bugtrackerUrl.toHtmlEscaped());
    if (!info.vcsUrl.isEmpty() && info.vcsUrl != githubUrl && info.vcsUrl != info.homepageUrl) {
        links << QStringLiteral("<a href=\"%1\">Browse Source Code</a>")
                       .arg(info.vcsUrl.toHtmlEscaped());
    }
    if (!info.homepageUrl.isEmpty() && info.homepageUrl != githubUrl) {
        links << QStringLiteral("<a href=\"%1\">Project Website</a>")
                       .arg(info.homepageUrl.toHtmlEscaped());
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
            const quint64 generation = m_loadGeneration;
            QNetworkRequest iconReq(url);
            iconReq.setTransferTimeout(15'000);
            QNetworkReply *reply = m_network->get(iconReq);
            connect(reply, &QNetworkReply::finished, this, [this, reply, expectedUrl, generation]() {
                if (generation != m_loadGeneration) {
                    reply->deleteLater();
                    return;
                }
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
            const QString shotUrl = info.screenshotUrls.at(i);
            const quint64 generation = m_loadGeneration;
            QNetworkRequest req = QNetworkRequest(QUrl(shotUrl));
            req.setTransferTimeout(15'000);
            QNetworkReply *reply = m_network->get(req);
            QPointer<QLabel> labelGuard(label);
            connect(reply, &QNetworkReply::finished, this, [reply, labelGuard, generation, shotUrl, this]() {
                if (generation != m_loadGeneration || !labelGuard) {
                    reply->deleteLater();
                    return;
                }
                if (reply->error() == QNetworkReply::NoError && m_info.screenshotUrls.contains(shotUrl)) {
                    QPixmap pm;
                    if (pm.loadFromData(reply->readAll()))
                        labelGuard->setPixmap(pm.scaled(220, 140, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                }
                reply->deleteLater();
            });
        }
    }

    updateButtonStates();
}

void AppDetailsWidget::onInstallClicked()
{
    if (m_info.id.isEmpty() || m_info.installed)
        return;
    emit installRequested(m_info);
}

void AppDetailsWidget::onLaunchClicked()
{
    if (m_backend && m_info.installed)
        m_backend->launchApp(m_info.id);
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

void AppDetailsWidget::setInstallInProgress(bool inProgress, int progress)
{
    m_installInProgress = inProgress;
    if (!m_installButton || !m_installProgress)
        return;

    if (inProgress) {
        m_installButton->setVisible(true);
        m_installButton->setEnabled(false);
        m_installButton->setText(QString());
        if (progress >= 0) {
            m_installProgressTimer->stop();
            m_installProgress->setValue(qBound(0, progress, 100));
        } else {
            m_installProgressValue = 0;
            m_installProgress->setValue(0);
            m_installProgressTimer->start();
        }
        m_installProgress->show();
        layoutInstallProgress();
        return;
    }

    m_installProgressTimer->stop();
    m_installProgress->hide();
    updateButtonStates();
}

void AppDetailsWidget::layoutInstallProgress()
{
    if (!m_installButton || !m_installProgress)
        return;
    const int margin = 3;
    m_installProgress->setGeometry(margin,
                                   margin,
                                   m_installButton->width() - 2 * margin,
                                   m_installButton->height() - 2 * margin);
}

void AppDetailsWidget::tickInstallProgress()
{
    m_installProgressValue += 3;
    if (m_installProgressValue > 100)
        m_installProgressValue = 0;
    m_installProgress->setValue(m_installProgressValue);
}

void AppDetailsWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    layoutInstallProgress();
}

void AppDetailsWidget::updateButtonStates()
{
    if (m_installInProgress)
        return;

    const bool installed = m_info.installed;
    m_installButton->setVisible(!installed);
    m_launchButton->setVisible(installed);
    m_removeButton->setVisible(installed);
    m_removeButton->setEnabled(installed);

    const bool trackedUpdate = installed
            && !m_info.trackedStableAssetUrl.isEmpty()
            && TrackedBuildClassifier::isReleaseNewerThanInstalled(m_info.version,
                                                                   m_info.trackedStableVersion);
    const bool showUpdate = installed && trackedUpdate;
    m_updateButton->setVisible(showUpdate);
    m_updateButton->setEnabled(showUpdate);
    if (trackedUpdate && !m_info.trackedStableVersion.isEmpty()) {
        m_updateButton->setText(tr("Update to %1").arg(m_info.trackedStableVersion));
        m_updateButton->setToolTip(m_info.trackedStableAssetUrl);
    } else {
        m_updateButton->setText(tr("Update"));
        m_updateButton->setToolTip(QString());
    }
}
