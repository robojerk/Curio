#include "AppListModel.h"

AppListModel::AppListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int AppListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_apps.size();
}

QVariant AppListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_apps.size())
        return {};

    const AppInfo &app = m_apps.at(index.row());

    switch (role) {
    case IdRole:
        return app.id;
    case NameRole:
        return app.name;
    case SummaryRole:
        return app.summary;
    case VersionRole:
        return app.version;
    case InstalledRole:
        return app.installed;
    default:
        return {};
    }
}

QHash<int, QByteArray> AppListModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "id";
    roles[NameRole] = "name";
    roles[SummaryRole] = "summary";
    roles[VersionRole] = "version";
    roles[InstalledRole] = "installed";
    return roles;
}

void AppListModel::setApps(const QVector<AppInfo> &apps)
{
    beginResetModel();
    m_apps = apps;
    endResetModel();
}

AppInfo AppListModel::appAt(int row) const
{
    if (row < 0 || row >= m_apps.size())
        return {};
    return m_apps.at(row);
}

