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
    setFixedSize(180, 220);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(6);

    m_iconLabel = new QLabel(this);
    m_iconLabel->setFixedSize(128, 128);
    m_iconLabel->setAlignment(Qt::AlignCenter);
    m_iconLabel->setScaledContents(false);
    layout->addWidget(m_iconLabel, 0, Qt::AlignCenter);

    m_nameLabel = new QLabel(this);
    m_nameLabel->setAlignment(Qt::AlignCenter);
    m_nameLabel->setTextFormat(Qt::PlainText);
    m_nameLabel->setWordWrap(false);
    m_nameLabel->setFixedHeight(20);
    m_nameLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    QFont f = m_nameLabel->font();
    f.setBold(true);
    m_nameLabel->setFont(f);
    layout->addWidget(m_nameLabel);

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setAlignment(Qt::AlignCenter);
    m_summaryLabel->setTextFormat(Qt::PlainText);
    m_summaryLabel->setWordWrap(false);
    m_summaryLabel->setFixedHeight(18);
    m_summaryLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    layout->addWidget(m_summaryLabel);

    m_installButton = new QPushButton(tr("Install"), this);
    m_installButton->setCursor(Qt::PointingHandCursor);
    connect(m_installButton, &QPushButton::clicked, this, [this]() {
        emit installRequested();
    });
    layout->addWidget(m_installButton);

    m_network = new QNetworkAccessManager(this);
}

void AppCardWidget::setApp(const AppInfo &info)
{
    m_info = info;
    const QString cleanName = info.name.simplified();
    const QString cleanSummary = info.summary.simplified();
    QFontMetrics nameFm(m_nameLabel->font());
    QFontMetrics summaryFm(m_summaryLabel->font());
    m_nameLabel->setText(nameFm.elidedText(cleanName, Qt::ElideRight, 156));
    m_nameLabel->setToolTip(cleanName);
    m_summaryLabel->setText(summaryFm.elidedText(cleanSummary, Qt::ElideRight, 156));
    m_summaryLabel->setToolTip(info.summary);
    m_installButton->setVisible(!info.installed);

    // Always set a deterministic placeholder first to avoid transient oversized draws.
    QIcon placeholder = QIcon::fromTheme(QStringLiteral("application-x-executable"));
    if (placeholder.isNull())
        placeholder = QIcon::fromTheme(QStringLiteral("applications-internet"));
    if (!placeholder.isNull())
        m_iconLabel->setPixmap(placeholder.pixmap(128, 128));
    else
        m_iconLabel->clear();

    QIcon icon = QIcon::fromTheme(info.iconName.isEmpty() ? info.id : info.iconName);
    if (!icon.isNull()) {
        m_iconLabel->setPixmap(icon.pixmap(128, 128));
        return;
    }

    if (!info.iconUrl.isEmpty()) {
        QUrl url(info.iconUrl);
        QPixmap pm;
        if (url.isLocalFile()) {
            pm.load(url.toLocalFile());
        } else if (url.scheme().isEmpty() && info.iconUrl.startsWith(QLatin1Char('/'))) {
            pm.load(info.iconUrl);
        } else if (url.scheme() == QLatin1String("http") || url.scheme() == QLatin1String("https")) {
            const QString expectedUrl = url.toString();
            QNetworkReply *reply = m_network->get(QNetworkRequest(url));
            connect(reply, &QNetworkReply::finished, this, [this, reply, expectedUrl]() {
                if (reply->error() == QNetworkReply::NoError) {
                    QPixmap downloaded;
                    if (m_info.iconUrl == expectedUrl &&
                        downloaded.loadFromData(reply->readAll()) &&
                        isReasonableIconShape(downloaded)) {
                        m_iconLabel->setPixmap(normalizedIconPixmap(downloaded, 128));
                    }
                }
                reply->deleteLater();
            });
            // Keep fallback until async image is loaded.
        } else {
            pm.load(info.iconUrl);
        }
        if (!pm.isNull() && isReasonableIconShape(pm)) {
            m_iconLabel->setPixmap(normalizedIconPixmap(pm, 128));
            return;
        }
    }
}

void AppCardWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && childAt(event->position().toPoint()) != m_installButton)
        emit openDetailsRequested();
    QFrame::mousePressEvent(event);
}
