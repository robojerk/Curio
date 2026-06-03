#pragma once

#include <QAbstractListModel>
#include <QVector>

#include "AppInfo.h"

class AppListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        SummaryRole,
        VersionRole,
        InstalledRole
    };

    explicit AppListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setApps(const QVector<AppInfo> &apps);
    void patchApps(const QVector<AppInfo> &updates);
    AppInfo appAt(int row) const;

private:
    QVector<AppInfo> m_apps;
};

