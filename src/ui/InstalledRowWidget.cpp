#include "InstalledRowWidget.h"

#include <QFontMetrics>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMouseEvent>
#include <QPixmap>
#include <QPushButton>
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
}

InstalledRowWidget::InstalledRowWidget(QWidget *parent)
    : QWidget(parent)
{
    setCursor(Qt::PointingHandCursor);
    setFixedHeight(56);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);

    m_iconLabel = new QLabel(this);
    m_iconLabel->setFixedSize(40, 40);
    layout->addWidget(m_iconLabel);

    auto *textLayout = new QVBoxLayout;
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

    m_uninstallButton = new QPushButton(tr("Uninstall"), this);
    m_uninstallButton->setCursor(Qt::PointingHandCursor);
    connect(m_uninstallButton, &QPushButton::clicked, this, [this]() {
        emit uninstallRequested();
    });
    layout->addWidget(m_uninstallButton);
}

void InstalledRowWidget::setApp(const AppInfo &info)
{
    m_info = info;
    m_nameLabel->setText(info.name);
    QFontMetrics fm(m_summaryLabel->font());
    m_summaryLabel->setText(fm.elidedText(info.summary, Qt::ElideRight, 400));

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

void InstalledRowWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && childAt(event->position().toPoint()) != m_uninstallButton)
        emit openDetailsRequested();
    QWidget::mousePressEvent(event);
}
