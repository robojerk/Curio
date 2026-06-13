#include "OperationModel.h"

namespace {
constexpr int kMaxOperations = 200;
}

OperationModel::OperationModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int OperationModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_operations.size();
}

QVariant OperationModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_operations.size())
        return {};

    const Operation &op = m_operations.at(index.row());

    if (role == Qt::DisplayRole) {
        const QString action = (op.type == OperationType::Install)
                ? QStringLiteral("Install")
                : (op.type == OperationType::Uninstall)
                  ? QStringLiteral("Uninstall")
                  : QStringLiteral("Update");
        const QString status = (op.status == OperationStatus::Running)
                ? QStringLiteral("Running")
                : (op.status == OperationStatus::Succeeded)
                  ? QStringLiteral("Succeeded")
                  : (op.status == OperationStatus::Failed)
                    ? QStringLiteral("Failed")
                    : QStringLiteral("Pending");
        const QString detail = op.message.trimmed().isEmpty() ? QString() : QStringLiteral(" - %1").arg(op.message.trimmed());
        return QStringLiteral("%1 %2 [%3]%4").arg(action).arg(op.appId).arg(status).arg(detail);
    }

    switch (role) {
    case AppIdRole:
        return op.appId;
    case AppNameRole:
        return op.appName;
    case TypeRole:
        return static_cast<int>(op.type);
    case StatusRole:
        return static_cast<int>(op.status);
    case ProgressRole:
        return op.progress;
    case MessageRole:
        return op.message;
    }
    return {};
}

QHash<int, QByteArray> OperationModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[AppIdRole] = "appId";
    roles[AppNameRole] = "appName";
    roles[TypeRole] = "type";
    roles[StatusRole] = "status";
    roles[ProgressRole] = "progress";
    roles[MessageRole] = "message";
    return roles;
}

void OperationModel::addOrUpdate(const Operation &op)
{
    if (m_operations.size() >= kMaxOperations) {
        beginRemoveRows(QModelIndex(), 0, 0);
        m_operations.removeFirst();
        endRemoveRows();
    }
    const int row = m_operations.size();
    beginInsertRows(QModelIndex(), row, row);
    m_operations.append(op);
    endInsertRows();
}

std::optional<Operation> OperationModel::operationForAppId(const QString &appId) const
{
    for (int i = m_operations.size() - 1; i >= 0; --i) {
        if (m_operations[i].appId == appId)
            return m_operations[i];
    }
    return std::nullopt;
}

