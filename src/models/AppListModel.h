#pragma once

#include <QAbstractListModel>
#include <QSet>
#include <QVector>

#include "AppInfo.h"

class AppListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles : int {
        IdRole = Qt::UserRole + 1,
        NameRole,
        SummaryRole,
        VersionRole,
        InstalledRole,
        IconUrlRole,
        IconNameRole,
        DeveloperRole,
        SkeletonRole
    };

    explicit AppListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setApps(const QVector<AppInfo> &apps);
    void patchApps(const QVector<AppInfo> &updates);
    void syncInstalledFlags(const QSet<QString> &installedIds);
    AppInfo appAt(int row) const;

private:
    QVector<AppInfo> m_apps;
};

