#include "AppItemDelegate.h"

#include <QApplication>
#include <QPainter>

#include "models/AppListModel.h"

AppItemDelegate::AppItemDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

QSize AppItemDelegate::sizeHint(const QStyleOptionViewItem &option,
                                const QModelIndex &index) const
{
    Q_UNUSED(index);
    const int height = option.fontMetrics.height() * 3 + 16;
    return QSize(option.rect.width(), height);
}

void AppItemDelegate::paint(QPainter *painter,
                            const QStyleOptionViewItem &option,
                            const QModelIndex &index) const
{
    if (!index.isValid())
        return;

    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);

    const QString name = index.data(AppListModel::NameRole).toString();
    const QString summary = index.data(AppListModel::SummaryRole).toString();
    const QString version = index.data(AppListModel::VersionRole).toString();
    const bool installed = index.data(AppListModel::InstalledRole).toBool();

    QStyle *style = QApplication::style();

    painter->save();

    // Draw selection/background
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, nullptr);

    QRect rect = opt.rect.marginsRemoved(QMargins(8, 8, 8, 8));

    // Card background
    QColor cardColor = opt.palette.base().color();
    if (opt.state & QStyle::State_Selected) {
        cardColor = opt.palette.highlight().color();
    }
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setBrush(cardColor);
    painter->setPen(Qt::NoPen);
    painter->drawRoundedRect(rect, 6, 6);

    // Inner content
    QRect contentRect = rect.adjusted(12, 8, -12, -8);
    painter->setPen((opt.state & QStyle::State_Selected)
                        ? opt.palette.highlightedText().color()
                        : opt.palette.text().color());

    // Icon on the left
    const int iconSize = 48;
    QRect iconRect = contentRect;
    iconRect.setWidth(iconSize);
    iconRect.setHeight(iconSize);
    iconRect.moveTop(contentRect.top());

    QIcon icon = QIcon::fromTheme(index.data(AppListModel::IdRole).toString());
    if (!icon.isNull()) {
        icon.paint(painter, iconRect);
    }

    // Text area to the right of the icon
    QRect textRect = contentRect.adjusted(iconSize + 12, 0, 0, 0);

    QFont nameFont = opt.font;
    nameFont.setPointSizeF(nameFont.pointSizeF() + 1);
    nameFont.setBold(true);
    painter->setFont(nameFont);

    QRect nameRect = textRect;
    nameRect.setBottom(nameRect.top() + opt.fontMetrics.height() + 2);
    painter->drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter,
                      version.isEmpty() ? name : QStringLiteral("%1  %2").arg(name, version));

    // Summary line
    QFont summaryFont = opt.font;
    painter->setFont(summaryFont);
    QRect summaryRect = textRect;
    summaryRect.setTop(nameRect.bottom() + 4);
    painter->drawText(summaryRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                      summary);

    // Installed badge
    if (installed) {
        QString badgeText = QObject::tr("Installed");
        QFont badgeFont = opt.font;
        badgeFont.setPointSizeF(badgeFont.pointSizeF() - 1);
        painter->setFont(badgeFont);

        QFontMetrics fm(badgeFont);
        const int badgePaddingH = 8;
        const int badgePaddingV = 4;
        QSize badgeSize = fm.size(Qt::TextSingleLine, badgeText);
        badgeSize.rwidth() += badgePaddingH * 2;
        badgeSize.rheight() += badgePaddingV * 2;

        QRect badgeRect(contentRect.right() - badgeSize.width(),
                        contentRect.top(),
                        badgeSize.width(),
                        badgeSize.height());

        QColor badgeBg = (opt.state & QStyle::State_Selected)
                             ? opt.palette.highlightedText().color().darker(120)
                             : opt.palette.link().color();
        QColor badgeFg = (opt.state & QStyle::State_Selected)
                             ? opt.palette.highlight().color()
                             : opt.palette.window().color();

        painter->setBrush(badgeBg);
        painter->setPen(Qt::NoPen);
        painter->drawRoundedRect(badgeRect, 8, 8);

        painter->setPen(badgeFg);
        painter->drawText(badgeRect, Qt::AlignCenter, badgeText);
    }

    painter->restore();
}

