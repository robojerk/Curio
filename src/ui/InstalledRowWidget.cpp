#include "InstalledRowWidget.h"

#include "backend/AppDisplayNames.h"

#include <QHash>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMouseEvent>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QResizeEvent>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace {

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

QString formatInstallSource(const AppInfo &info)
{
    if (!info.trackedBuildProviderId.trimmed().isEmpty()) {
        const QString provider = info.trackedBuildProviderId.trimmed();
        if (provider.compare(QStringLiteral("github"), Qt::CaseInsensitive) == 0)
            return QStringLiteral("GitHub");
        return provider.at(0).toUpper() + provider.mid(1).toLower();
    }

    const QString origin = info.repoId.trimmed().toLower();
    if (origin.isEmpty())
        return QStringLiteral("—");

    static const QHash<QString, QString> knownRemotes = {
        {QStringLiteral("flathub"), QStringLiteral("Flathub")},
        {QStringLiteral("flathub-beta"), QStringLiteral("Flathub Beta")},
        {QStringLiteral("fedora"), QStringLiteral("Fedora")},
    };
    const auto it = knownRemotes.constFind(origin);
    if (it != knownRemotes.cend())
        return it.value();

    QString label = info.repoId.trimmed();
    label.replace(QLatin1Char('_'), QLatin1Char(' '));
    label.replace(QLatin1Char('-'), QLatin1Char(' '));
    const QStringList words = label.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    QStringList titled;
    titled.reserve(words.size());
    for (QString word : words) {
        if (word.isEmpty())
            continue;
        word[0] = word.at(0).toUpper();
        titled.append(word);
    }
    return titled.join(QLatin1Char(' '));
}

} // namespace

InstalledRowWidget::InstalledRowWidget(QWidget *parent)
    : QWidget(parent)
{
    setCursor(Qt::PointingHandCursor);
    setFixedHeight(56);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(12);

    m_iconLabel = new QLabel(this);
    m_iconLabel->setFixedSize(40, 40);
    layout->addWidget(m_iconLabel);

    auto *textLayout = new QVBoxLayout;
    textLayout->setSpacing(0);
    m_nameLabel = new QLabel(this);
    QFont f = m_nameLabel->font();
    f.setBold(true);
    m_nameLabel->setFont(f);
    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setMaximumHeight(20);
    m_summaryLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    textLayout->addWidget(m_nameLabel);
    textLayout->addWidget(m_summaryLabel);
    layout->addLayout(textLayout, 1);

    m_sourceLabel = new QLabel(this);
    m_sourceLabel->setAlignment(Qt::AlignCenter);
    m_sourceLabel->setMinimumWidth(120);
    m_sourceLabel->setMaximumWidth(160);
    m_sourceLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_sourceLabel->setStyleSheet(QStringLiteral("color: rgba(200,200,210,0.92);"));
    layout->addWidget(m_sourceLabel, 0, Qt::AlignCenter);

    m_updateButton = new QPushButton(tr("Update"), this);
    m_updateButton->setCursor(Qt::PointingHandCursor);
    m_updateButton->setFixedWidth(88);
    m_updateButton->setFixedHeight(32);
    m_updateButton->hide();
    connect(m_updateButton, &QPushButton::clicked, this, [this]() {
        if (!m_updateInProgress)
            emit updateRequested();
    });
    layout->addWidget(m_updateButton);

    m_updateProgress = new QProgressBar(m_updateButton);
    m_updateProgress->setTextVisible(false);
    m_updateProgress->setRange(0, 100);
    m_updateProgress->setValue(0);
    m_updateProgress->hide();
    m_updateProgress->setStyleSheet(QStringLiteral(
        "QProgressBar {"
        "  border: 1px solid rgba(255,255,255,0.12);"
        "  border-radius: 14px;"
        "  background-color: rgba(80,80,88,0.55);"
        "}"
        "QProgressBar::chunk {"
        "  border-radius: 13px;"
        "  background-color: rgba(90,160,220,0.95);"
        "}"
    ));

    m_updateProgressTimer = new QTimer(this);
    m_updateProgressTimer->setInterval(45);
    connect(m_updateProgressTimer, &QTimer::timeout, this, &InstalledRowWidget::tickUpdateProgress);

    m_uninstallButton = new QPushButton(tr("Uninstall"), this);
    m_uninstallButton->setCursor(Qt::PointingHandCursor);
    m_uninstallButton->setFixedWidth(108);
    m_uninstallButton->setFixedHeight(32);
    connect(m_uninstallButton, &QPushButton::clicked, this, [this]() {
        if (!m_uninstallProgress || !m_uninstallProgress->isVisible())
            emit uninstallRequested();
    });
    layout->addWidget(m_uninstallButton);

    m_uninstallProgress = new QProgressBar(m_uninstallButton);
    m_uninstallProgress->setTextVisible(false);
    m_uninstallProgress->setRange(0, 100);
    m_uninstallProgress->setValue(0);
    m_uninstallProgress->setLayoutDirection(Qt::RightToLeft);
    m_uninstallProgress->hide();
    m_uninstallProgress->setStyleSheet(QStringLiteral(
        "QProgressBar {"
        "  border: 1px solid rgba(255,255,255,0.12);"
        "  border-radius: 14px;"
        "  background-color: rgba(80,80,88,0.55);"
        "}"
        "QProgressBar::chunk {"
        "  border-radius: 13px;"
        "  background-color: rgba(140,145,165,0.95);"
        "}"
    ));

    m_uninstallProgressTimer = new QTimer(this);
    m_uninstallProgressTimer->setInterval(45);
    connect(m_uninstallProgressTimer, &QTimer::timeout, this, &InstalledRowWidget::tickUninstallProgress);
}

void InstalledRowWidget::setApp(const AppInfo &info)
{
    m_info = info;
    m_trackGitLinkEnabled = false;
    m_trackLinkLabel.clear();
    m_trackedUpdateAvailable = false;
    m_trackedUpdateTag.clear();
    m_remoteUpdateAvailable = false;
    m_nameLabel->setText(AppDisplayNames::displayName(info.name, info.id));
    updateSummaryLabel();
    updateSourceLabel();
    syncUpdateButtonVisibility();

    QIcon fallback = QIcon::fromTheme(QStringLiteral("application-x-executable"));
    if (fallback.isNull())
        fallback = QIcon::fromTheme(QStringLiteral("applications-internet"));
    if (!fallback.isNull())
        m_iconLabel->setPixmap(fallback.pixmap(40, 40));
    else
        m_iconLabel->clear();

    QIcon icon = QIcon::fromTheme(info.iconName.isEmpty() ? info.id : info.iconName);
    if (!icon.isNull()) {
        m_iconLabel->setPixmap(icon.pixmap(40, 40));
        return;
    }
    if (!info.iconUrl.isEmpty()) {
        QUrl url(info.iconUrl);
        QPixmap pm;
        if (url.isLocalFile())
            pm.load(url.toLocalFile());
        else if (url.scheme().isEmpty() && info.iconUrl.startsWith(QLatin1Char('/')))
            pm.load(info.iconUrl);
        else
            pm.load(info.iconUrl);
        if (!pm.isNull() && isReasonableIconShape(pm)) {
            m_iconLabel->setPixmap(normalizedIconPixmap(pm, 40));
            return;
        }
    }
}

void InstalledRowWidget::setTrackGitRepoLink(bool enabled, const QString &label)
{
    m_trackGitLinkEnabled = enabled;
    m_trackLinkLabel = label;
    updateSourceLabel();
    if (m_sourceLabel) {
        m_sourceLabel->setCursor(enabled ? Qt::PointingHandCursor : Qt::ArrowCursor);
    }
}

void InstalledRowWidget::setTrackedUpdateAvailable(bool available, const QString &releaseTag)
{
    m_trackedUpdateAvailable = available;
    m_trackedUpdateTag = releaseTag.trimmed();
    if (m_updateButton && !m_updateInProgress) {
        syncUpdateButtonVisibility();
        if (available) {
            m_updateButton->setToolTip(tr("Install release %1").arg(m_trackedUpdateTag));
        } else if (m_remoteUpdateAvailable) {
            m_updateButton->setToolTip(tr("Install update from %1").arg(formatInstallSource(m_info)));
        } else {
            m_updateButton->setToolTip(QString());
        }
    }
    updateSummaryLabel();
}

void InstalledRowWidget::setRemoteUpdateAvailable(bool available)
{
    m_remoteUpdateAvailable = available;
    if (m_updateButton && !m_updateInProgress) {
        syncUpdateButtonVisibility();
        if (m_trackedUpdateAvailable) {
            m_updateButton->setToolTip(tr("Install release %1").arg(m_trackedUpdateTag));
        } else if (available) {
            m_updateButton->setToolTip(tr("Install update from %1").arg(formatInstallSource(m_info)));
        } else {
            m_updateButton->setToolTip(QString());
        }
    }
    updateSummaryLabel();
}

void InstalledRowWidget::syncUpdateButtonVisibility()
{
    if (!m_updateButton || m_updateInProgress)
        return;
    m_updateButton->setVisible(m_trackedUpdateAvailable || m_remoteUpdateAvailable);
}

void InstalledRowWidget::setUpdateInProgress(bool inProgress, const QString &statusText, int progress)
{
    m_updateInProgress = inProgress;
    if (!m_updateButton || !m_updateProgress)
        return;

    if (inProgress) {
        m_updateButton->setVisible(true);
        m_updateButton->setEnabled(false);
        m_updateButton->setText(QString());
        if (progress >= 0) {
            m_updateProgressTimer->stop();
            m_updateProgress->setValue(qBound(0, progress, 100));
        } else {
            m_updateProgressValue = 0;
            m_updateProgress->setValue(0);
            m_updateProgressTimer->start();
        }
        m_updateProgress->show();
        layoutUpdateProgress();
        if (!statusText.isEmpty() && m_summaryLabel) {
            QFontMetrics fm(m_summaryLabel->font());
            m_summaryLabel->setText(fm.elidedText(statusText, Qt::ElideRight, 400));
        }
        return;
    }

    m_updateProgressTimer->stop();
    m_updateProgress->hide();
    m_updateButton->setEnabled(true);
    m_updateButton->setText(tr("Update"));
    syncUpdateButtonVisibility();
    updateSummaryLabel();
}

void InstalledRowWidget::updateSummaryLabel()
{
    if (!m_summaryLabel || m_updateInProgress)
        return;
    QFontMetrics fm(m_summaryLabel->font());
    QString summary;
    if (m_trackedUpdateAvailable && !m_trackedUpdateTag.isEmpty()) {
        summary = tr("Version %1 — update to %2 available")
                          .arg(m_info.version, m_trackedUpdateTag);
    } else if (m_remoteUpdateAvailable) {
        summary = m_info.version.isEmpty()
                ? tr("Flatpak update available")
                : tr("Version %1 — Flatpak update available").arg(m_info.version);
    } else if (m_info.summary.isEmpty() && !m_info.version.isEmpty()) {
        summary = tr("Version %1").arg(m_info.version);
    } else {
        summary = m_info.summary;
    }
    m_summaryLabel->setText(fm.elidedText(summary, Qt::ElideRight, 400));
}

void InstalledRowWidget::updateSourceLabel()
{
    if (m_trackGitLinkEnabled && !m_trackLinkLabel.isEmpty()) {
        m_sourceLabel->setText(m_trackLinkLabel);
        m_sourceLabel->setStyleSheet(QStringLiteral("color: palette(link); text-decoration: underline;"));
        m_sourceLabel->setToolTip(tr("Add or link this app to Tracked Builds"));
        return;
    }
    m_sourceLabel->setStyleSheet(QString());
    m_sourceLabel->setText(formatInstallSource(m_info));
    m_sourceLabel->setToolTip(m_info.trackedBuildProviderId.isEmpty()
                                      ? tr("Installed from remote: %1").arg(m_info.repoId)
                                      : tr("Tracked build source: %1").arg(m_info.trackedBuildProviderId));
}

void InstalledRowWidget::setUninstallInProgress(bool inProgress)
{
    if (!m_uninstallButton || !m_uninstallProgress)
        return;

    if (inProgress) {
        m_uninstallButton->setEnabled(false);
        m_uninstallButton->setText(QString());
        m_uninstallButton->setStyleSheet(QStringLiteral(
            "QPushButton {"
            "  border: 1px solid rgba(255,255,255,0.08);"
            "  border-radius: 14px;"
            "  background-color: rgba(70,70,78,0.65);"
            "  color: rgba(200,200,205,0.5);"
            "}"
        ));
        m_uninstallProgressValue = 0;
        m_uninstallProgress->setValue(0);
        m_uninstallProgress->show();
        layoutUninstallProgress();
        m_uninstallProgressTimer->start();
        return;
    }

    m_uninstallProgressTimer->stop();
    m_uninstallProgress->hide();
    m_uninstallButton->setEnabled(true);
    m_uninstallButton->setText(tr("Uninstall"));
    m_uninstallButton->setStyleSheet(QString());
}

void InstalledRowWidget::layoutUpdateProgress()
{
    if (!m_updateButton || !m_updateProgress)
        return;
    const int margin = 3;
    m_updateProgress->setGeometry(margin,
                                  margin,
                                  m_updateButton->width() - 2 * margin,
                                  m_updateButton->height() - 2 * margin);
}

void InstalledRowWidget::layoutUninstallProgress()
{
    if (!m_uninstallButton || !m_uninstallProgress)
        return;
    const int margin = 3;
    m_uninstallProgress->setGeometry(margin,
                                     margin,
                                     m_uninstallButton->width() - 2 * margin,
                                     m_uninstallButton->height() - 2 * margin);
}

void InstalledRowWidget::tickUninstallProgress()
{
    m_uninstallProgressValue += 3;
    if (m_uninstallProgressValue > 100)
        m_uninstallProgressValue = 0;
    m_uninstallProgress->setValue(m_uninstallProgressValue);
}

void InstalledRowWidget::tickUpdateProgress()
{
    m_updateProgressValue += 3;
    if (m_updateProgressValue > 100)
        m_updateProgressValue = 0;
    m_updateProgress->setValue(m_updateProgressValue);
}

void InstalledRowWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    layoutUninstallProgress();
    layoutUpdateProgress();
}

void InstalledRowWidget::mousePressEvent(QMouseEvent *event)
{
    const QPoint pos = event->position().toPoint();
    const bool onUninstall = m_uninstallButton && m_uninstallButton->geometry().contains(pos);
    const bool onUpdate = m_updateButton && m_updateButton->isVisible()
            && m_updateButton->geometry().contains(pos);
    const bool onSource = m_sourceLabel && m_sourceLabel->geometry().contains(pos);
    if (event->button() == Qt::LeftButton && onSource && m_trackGitLinkEnabled) {
        emit trackGitRepoRequested();
        return;
    }
    if (event->button() == Qt::LeftButton && !onUninstall && !onUpdate)
        emit openDetailsRequested();
    QWidget::mousePressEvent(event);
}
