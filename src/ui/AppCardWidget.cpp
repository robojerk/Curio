#include "AppCardWidget.h"

#include <QFontMetrics>
#include <QIcon>
#include <QMouseEvent>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPixmap>
#include <QUrl>
#include <QVBoxLayout>

#include <QLabel>
#include <QPushButton>

namespace {

QNetworkAccessManager *sharedIconNetwork()
{
    static QNetworkAccessManager *nam = []() {
        auto *manager = new QNetworkAccessManager(qApp);
        manager->setTransferTimeout(15'000);
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
    m_installButton->setFixedHeight(30);
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

    m_network = sharedIconNetwork();
}

void AppCardWidget::setApp(const AppInfo &info)
{
    const bool iconUnchanged = m_info.id == info.id
            && m_info.iconUrl == info.iconUrl
            && m_info.iconName == info.iconName;
    m_info = info;
    const QString cleanName = info.name.simplified();
    const QString cleanSummary = info.summary.simplified();
    const QString cleanDeveloper = info.developerName.simplified();
    QFontMetrics nameFm(m_nameLabel->font());
    QFontMetrics summaryFm(m_summaryLabel->font());
    QFontMetrics metaFm(m_metaLabel ? m_metaLabel->font() : m_nameLabel->font());
    const int textWidth = qMax(120, width() - 56 - 32 - 96);
    m_nameLabel->setText(nameFm.elidedText(cleanName, Qt::ElideRight, textWidth));
    m_nameLabel->setToolTip(cleanName);
    // Let the summary wrap naturally within the fixed-height card without manual elision
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

    if (info.installed) {
        m_installButton->setText(tr("Installed"));
        m_installButton->setEnabled(false);
    } else {
        m_installButton->setText(tr("Install"));
        m_installButton->setEnabled(true);
    }

    if (!iconUnchanged)
        refreshIcon();
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
    // Always set a deterministic placeholder first to avoid transient oversized draws.
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
    if (event->button() == Qt::LeftButton && childAt(event->position().toPoint()) != m_installButton)
        emit openDetailsRequested();
    QFrame::mousePressEvent(event);
}
