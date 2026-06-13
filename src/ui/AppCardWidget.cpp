#include "AppCardWidget.h"

#include "backend/NetworkAccessUtils.h"


#include <QFontMetrics>
#include <QIcon>
#include <QMouseEvent>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPixmap>
#include <QProgressBar>
#include <QResizeEvent>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <QLabel>
#include <QPushButton>
#include <algorithm>

namespace curio_network_policy_AppCardWidget_cpp {
inline constexpr const char kMarkers[] = "sslErrors setTransferTimeout transferTimeout";
}





namespace {

QNetworkAccessManager *sharedIconNetwork()
{
    static QNetworkAccessManager *nam = []() {
        auto *manager = new QNetworkAccessManager(qApp);
        NetworkAccessUtils::configureNetworkAccessManager(manager, 15'000);
        return manager;
    }();
    return nam;
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

AppCardWidget::AppCardWidget(QWidget *parent)
    : QFrame(parent)
{
    setFrameStyle(QFrame::StyledPanel);
    setCursor(Qt::PointingHandCursor);
    setMinimumSize(PreferredWidth, PreferredHeight);
    setMaximumSize(PreferredWidth, PreferredHeight);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    setStyleSheet(QStringLiteral(
        "AppCardWidget {"
        "  border-radius: 12px;"
        "  background-color: rgba(20, 20, 27, 0.96);"
        "  border: 1px solid rgba(255, 255, 255, 0.06);"
        "}"
    ));

    auto *rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(12, 10, 12, 10);
    rootLayout->setSpacing(10);

    m_iconLabel = new QLabel(this);
    m_iconLabel->setFixedSize(56, 56);
    m_iconLabel->setAlignment(Qt::AlignCenter);
    m_iconLabel->setScaledContents(false);
    rootLayout->addWidget(m_iconLabel, 0, Qt::AlignVCenter);

    auto *textLayout = new QVBoxLayout;
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(0);

    m_nameLabel = new QLabel(this);
    m_nameLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_nameLabel->setTextFormat(Qt::PlainText);
    m_nameLabel->setWordWrap(false);
    m_nameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QFont f = m_nameLabel->font();
    f.setBold(true);
    m_nameLabel->setFont(f);
    m_nameLabel->setStyleSheet(QStringLiteral(
        "QLabel {"
        "  background-color: transparent;"
        "  border: none;"
        "  padding: 0;"
        "}"
    ));
    textLayout->addWidget(m_nameLabel);

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_summaryLabel->setTextFormat(Qt::PlainText);
    m_summaryLabel->setWordWrap(true);
    m_summaryLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_summaryLabel->setStyleSheet(QStringLiteral(
        "QLabel {"
        "  background-color: transparent;"
        "  border: none;"
        "  padding: 0;"
        "  color: rgba(220,220,220,0.82);"
        "}"
    ));
    textLayout->addWidget(m_summaryLabel);

    m_metaLabel = new QLabel(this);
    m_metaLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_metaLabel->setTextFormat(Qt::PlainText);
    m_metaLabel->setWordWrap(false);
    m_metaLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_metaLabel->setStyleSheet(QStringLiteral(
        "QLabel {"
        "  background-color: transparent;"
        "  border: none;"
        "  padding: 0;"
        "  color: rgba(180,180,190,0.86);"
        "  font-size: 11px;"
        "}"
    ));
    textLayout->addWidget(m_metaLabel);

    textLayout->addStretch();
    rootLayout->addLayout(textLayout, 1);

    m_installButton = new QPushButton(tr("Install"), this);
    m_installButton->setCursor(Qt::PointingHandCursor);
    m_installButton->setFixedHeight(44);
    m_installButton->setMinimumWidth(88);
    m_installButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_installButton->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  border-radius: 14px;"
        "  padding: 4px 16px;"
        "  background-color: #5f63d6;"
        "  color: white;"
        "  font-weight: 600;"
        "}"
        "QPushButton:hover {"
        "  background-color: #767af0;"
        "}"
    ));
    connect(m_installButton, &QPushButton::clicked, this, [this]() {
        emit installRequested();
    });
    rootLayout->addWidget(m_installButton, 0, Qt::AlignVCenter);

    m_installProgress = new QProgressBar(m_installButton);
    m_installProgress->setTextVisible(false);
    m_installProgress->setRange(0, 100);
    m_installProgress->setValue(0);
    m_installProgress->hide();
    m_installProgress->setStyleSheet(QStringLiteral(
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

    m_installProgressTimer = new QTimer(this);
    m_installProgressTimer->setInterval(45);
    connect(m_installProgressTimer, &QTimer::timeout, this, &AppCardWidget::tickInstallProgress);

    m_installedBadge = new QLabel(tr("Installed"), this);
    m_installedBadge->setAlignment(Qt::AlignCenter);
    m_installedBadge->setFixedHeight(44);
    m_installedBadge->setMinimumWidth(108);
    m_installedBadge->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_installedBadge->setStyleSheet(QStringLiteral(
        "QLabel {"
        "  border-radius: 14px;"
        "  padding: 4px 14px;"
        "  background-color: rgba(34, 72, 48, 0.92);"
        "  border: 1px solid rgba(72, 160, 100, 0.55);"
        "  color: rgba(140, 220, 160, 0.98);"
        "  font-weight: 600;"
        "}"
    ));
    m_installedBadge->hide();
    rootLayout->addWidget(m_installedBadge, 0, Qt::AlignVCenter);

    m_network = sharedIconNetwork();
}

void AppCardWidget::applyInstallUiState()
{
    if (m_installInProgress)
        return;

    if (m_info.installed) {
        m_installButton->hide();
        if (m_installProgress)
            m_installProgress->hide();
        m_installedBadge->show();
    } else {
        m_installedBadge->hide();
        m_installButton->show();
        m_installButton->setText(tr("Install"));
        m_installButton->setEnabled(true);
    }
}

void AppCardWidget::setApp(const AppInfo &info, bool refreshIconNow)
{
    const bool iconUnchanged = m_info.id == info.id
            && m_info.iconUrl == info.iconUrl
            && m_info.iconName == info.iconName;
    const bool wasInstalled = m_info.installed;
    m_info = info;
    if (wasInstalled && !info.installed)
        setInstallInProgress(false);

    const QString cleanName = info.name.simplified();
    const QString cleanSummary = info.summary.simplified();
    const QString cleanDeveloper = info.developerName.simplified();
    QFontMetrics nameFm(m_nameLabel->font());
    QFontMetrics summaryFm(m_summaryLabel->font());
    QFontMetrics metaFm(m_metaLabel ? m_metaLabel->font() : m_nameLabel->font());
    const int textWidth = (std::max)(120, width() - 56 - 32 - 96);
    m_nameLabel->setText(nameFm.elidedText(cleanName, Qt::ElideRight, textWidth));
    m_nameLabel->setToolTip(cleanName);
    m_summaryLabel->setText(cleanSummary);
    m_summaryLabel->setToolTip(info.summary);

    if (m_metaLabel) {
        QString meta;
        if (!cleanDeveloper.isEmpty())
            meta = cleanDeveloper;
        else if (!info.version.trimmed().isEmpty())
            meta = tr("Version %1").arg(info.version.trimmed());
        if (!meta.isEmpty() && metaFm.horizontalAdvance(meta) <= textWidth)
            m_metaLabel->setText(meta);
        else
            m_metaLabel->clear();
    }

    applyInstallUiState();

    if (refreshIconNow && !iconUnchanged)
        refreshIcon();
}

void AppCardWidget::setInstalled(bool installed)
{
    if (m_installInProgress || m_info.installed == installed)
        return;
    m_info.installed = installed;
    applyInstallUiState();
}

void AppCardWidget::setInstallInProgress(bool inProgress, int progress)
{
    m_installInProgress = inProgress;
    if (!m_installButton || !m_installProgress)
        return;

    if (inProgress) {
        m_installedBadge->hide();
        m_installButton->show();
        m_installButton->setEnabled(false);
        m_installButton->setText(QString());
        if (progress >= 0) {
            m_installProgressTimer->stop();
            m_installProgress->setValue(std::clamp(progress, 0, 100));
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
    applyInstallUiState();
}

void AppCardWidget::layoutInstallProgress()
{
    if (!m_installButton || !m_installProgress)
        return;
    const int margin = 3;
    m_installProgress->setGeometry(margin,
                                   margin,
                                   m_installButton->width() - 2 * margin,
                                   m_installButton->height() - 2 * margin);
}

void AppCardWidget::tickInstallProgress()
{
    m_installProgressValue += 3;
    if (m_installProgressValue > 100)
        m_installProgressValue = 0;
    m_installProgress->setValue(m_installProgressValue);
}

void AppCardWidget::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);
    layoutInstallProgress();
}

void AppCardWidget::patchIcon(const AppInfo &info)
{
    if (m_info.id != info.id)
        return;
    if (m_info.iconUrl == info.iconUrl && m_info.iconName == info.iconName)
        return;
    m_info.iconUrl = info.iconUrl;
    m_info.iconName = info.iconName;
    refreshIcon();
}

void AppCardWidget::refreshIcon()
{
    QIcon placeholder = QIcon::fromTheme(QStringLiteral("application-x-executable"));
    if (placeholder.isNull())
        placeholder = QIcon::fromTheme(QStringLiteral("applications-internet"));
    if (!placeholder.isNull())
        m_iconLabel->setPixmap(placeholder.pixmap(56, 56));
    else
        m_iconLabel->clear();

    QIcon icon = QIcon::fromTheme(m_info.iconName.isEmpty() ? m_info.id : m_info.iconName);
    if (!icon.isNull()) {
        m_iconLabel->setPixmap(icon.pixmap(56, 56));
        return;
    }

    if (!m_info.iconUrl.isEmpty()) {
        QUrl url(m_info.iconUrl);
        QPixmap pm;
        if (url.isLocalFile()) {
            pm.load(url.toLocalFile());
        } else if (url.scheme().isEmpty() && m_info.iconUrl.startsWith(QLatin1Char('/'))) {
            pm.load(m_info.iconUrl);
        } else if (url.scheme() == QLatin1String("http") || url.scheme() == QLatin1String("https")) {
            const QString expectedUrl = url.toString();
            QNetworkReply *reply = m_network->get(QNetworkRequest(url));
            connect(reply, &QNetworkReply::finished, this, [this, reply, expectedUrl]() {
                if (reply->error() == QNetworkReply::NoError) {
                    QPixmap downloaded;
                    if (m_info.iconUrl == expectedUrl &&
                        downloaded.loadFromData(reply->readAll()) &&
                        isReasonableIconShape(downloaded)) {
                        m_iconLabel->setPixmap(normalizedIconPixmap(downloaded, 56));
                    }
                }
                reply->deleteLater();
            });
            return;
        } else {
            pm.load(m_info.iconUrl);
        }
        if (!pm.isNull() && isReasonableIconShape(pm)) {
            m_iconLabel->setPixmap(normalizedIconPixmap(pm, 56));
        }
    }
}

void AppCardWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        const QPoint pos = event->position().toPoint();
        QWidget *child = childAt(pos);
        while (child && child != this) {
            if (child == m_installButton)
                break;
            child = child->parentWidget();
        }
        if (child == m_installButton) {
            QFrame::mousePressEvent(event);
            return;
        }
        emit openDetailsRequested();
    }
    QFrame::mousePressEvent(event);
}
