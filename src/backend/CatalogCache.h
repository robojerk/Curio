#pragma once

#include <QHash>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include "models/AppInfo.h"

class CatalogCache : public QObject
{
    Q_OBJECT
public:
    static constexpr int kCacheVersion = 1;

    explicit CatalogCache(QObject *parent = nullptr);

    QString cacheRoot() const;
    bool loadIndex(const QString &repoId);
    bool isIndexFresh(const QString &repoId, qint64 maxAgeMs) const;
    QStringList enumerateIds(const QString &repoId) const;
    bool loadApp(const QString &repoId, const QString &appId, AppInfo *out) const;
    void saveApp(const QString &repoId, const AppInfo &app);
    void saveAppsBatch(const QString &repoId, const QVector<AppInfo> &apps);
    QVector<AppInfo> searchLocal(const QString &repoId, const QString &query, int limit = 50) const;

private:
    QString indexPath(const QString &repoId) const;
    QString appPath(const QString &repoId, const QString &appId) const;
    static AppInfo appFromJson(const QJsonObject &obj);
    static QJsonObject appToJson(const AppInfo &app);
    void writeIndexLocked(const QString &repoId);

    mutable QMutex m_mutex;
    QHash<QString, QStringList> m_idsByRepo;
    QHash<QString, qint64> m_indexUpdatedMs;
};
