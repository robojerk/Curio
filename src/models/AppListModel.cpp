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

void AppListModel::patchApps(const QVector<AppInfo> &updates)
{
    if (updates.isEmpty())
        return;

    for (int row = 0; row < m_apps.size(); ++row) {
        for (const AppInfo &update : updates) {
            if (m_apps.at(row).id != update.id)
                continue;
            bool changed = false;
            if (!update.name.isEmpty() && update.name != m_apps[row].id
                    && (m_apps[row].name.isEmpty() || m_apps[row].name == m_apps[row].id
                        || m_apps[row].name != update.name)) {
                m_apps[row].name = update.name;
                changed = true;
            }
            if (!update.summary.isEmpty() && m_apps[row].summary != update.summary) {
                m_apps[row].summary = update.summary;
                changed = true;
            }
            if (!update.iconName.isEmpty() && m_apps[row].iconName != update.iconName) {
                m_apps[row].iconName = update.iconName;
                changed = true;
            }
            if (!update.iconUrl.isEmpty() && m_apps[row].iconUrl != update.iconUrl) {
                m_apps[row].iconUrl = update.iconUrl;
                changed = true;
            }
            if (!update.vcsUrl.isEmpty() && m_apps[row].vcsUrl != update.vcsUrl) {
                m_apps[row].vcsUrl = update.vcsUrl;
                changed = true;
            }
            if (!update.homepageUrl.isEmpty() && m_apps[row].homepageUrl != update.homepageUrl) {
                m_apps[row].homepageUrl = update.homepageUrl;
                changed = true;
            }
            if (!changed)
                break;
            const QModelIndex idx = index(row, 0);
            emit dataChanged(idx, idx);
            break;
        }
    }
}

