#pragma once

#include <QAbstractListModel>
#include <QVector>

#include "Operation.h"

class OperationModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        AppIdRole = Qt::UserRole + 1,
        AppNameRole,
        TypeRole,
        StatusRole,
        ProgressRole,
        MessageRole
    };

    explicit OperationModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void addOrUpdate(const Operation &op);

private:
    QVector<Operation> m_operations;
};

