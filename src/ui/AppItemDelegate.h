#pragma once

#include <QStyledItemDelegate>

class AppItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit AppItemDelegate(QObject *parent = nullptr);

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;
    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
};

