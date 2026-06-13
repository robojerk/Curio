#include "CatalogCache.h"

#include <QDebug>
#include <QMutexLocker>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>
#include <QSaveFile>
#include <QStandardPaths>
#include <QtConcurrent>
#include <algorithm>

namespace {

QString sanitizedAppIdFileName(const QString &appId)
{
    QString safe = appId;
    safe.replace(QLatin1Char('/'), QLatin1Char('_'));
    return safe;
}

} // namespace

CatalogCache::CatalogCache(QObject *parent)
    : QObject(parent)
{
}

QString CatalogCache::cacheRoot() const
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (base.isEmpty())
        return QString();
    return base + QStringLiteral("/catalog");
}

QString CatalogCache::indexPath(const QString &repoId) const
{
    return cacheRoot() + QLatin1Char('/') + repoId.toLower() + QStringLiteral("/index.json");
}

QString CatalogCache::appPath(const QString &repoId, const QString &appId) const
{
    return cacheRoot() + QLatin1Char('/') + repoId.toLower() + QStringLiteral("/apps/")
            + sanitizedAppIdFileName(appId) + QStringLiteral(".json");
}

AppInfo CatalogCache::appFromJson(const QJsonObject &obj)
{
    AppInfo app;
    app.id = obj.value(QStringLiteral("id")).toString();
    app.name = obj.value(QStringLiteral("name")).toString();
    app.summary = obj.value(QStringLiteral("summary")).toString();
    app.version = obj.value(QStringLiteral("version")).toString();
    app.iconName = obj.value(QStringLiteral("iconName")).toString();
    app.iconUrl = obj.value(QStringLiteral("iconUrl")).toString();
    app.repoId = obj.value(QStringLiteral("repoId")).toString();
    app.storePageUrl = obj.value(QStringLiteral("storePageUrl")).toString();
    app.developerName = obj.value(QStringLiteral("developerName")).toString();
    return app;
}

QJsonObject CatalogCache::appToJson(const AppInfo &app)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("id"), app.id);
    obj.insert(QStringLiteral("name"), app.name);
    obj.insert(QStringLiteral("summary"), app.summary);
    obj.insert(QStringLiteral("version"), app.version);
    obj.insert(QStringLiteral("iconName"), app.iconName);
    obj.insert(QStringLiteral("iconUrl"), app.iconUrl);
    obj.insert(QStringLiteral("repoId"), app.repoId);
    obj.insert(QStringLiteral("storePageUrl"), app.storePageUrl);
    obj.insert(QStringLiteral("developerName"), app.developerName);
    return obj;
}

bool CatalogCache::isIndexFresh(const QString &repoId, qint64 maxAgeMs) const
{
    QMutexLocker lock(&m_mutex);
    const QString key = repoId.toLower();
    const auto it = m_indexUpdatedMs.constFind(key);
    if (it == m_indexUpdatedMs.cend())
        return false;
    return QDateTime::currentMSecsSinceEpoch() - it.value() <= maxAgeMs;
}

bool CatalogCache::loadIndex(const QString &repoId)
{
    const QString path = indexPath(repoId);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return false;

    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("version")).toInt() != kCacheVersion)
        return false;

    QStringList ids;
    for (const QJsonValue &val : root.value(QStringLiteral("ids")).toArray())
        ids.append(val.toString());

    QMutexLocker lock(&m_mutex);
    m_idsByRepo.insert(repoId.toLower(), ids);
    m_indexUpdatedMs.insert(repoId.toLower(), root.value(QStringLiteral("updatedAtMs")).toVariant().toLongLong());
    return true;
}

QStringList CatalogCache::enumerateIds(const QString &repoId) const
{
    QMutexLocker lock(&m_mutex);
    return m_idsByRepo.value(repoId.toLower());
}

bool CatalogCache::loadApp(const QString &repoId, const QString &appId, AppInfo *out) const
{
    if (out == nullptr || appId.isEmpty())
        return false;

    QFile file(appPath(repoId, appId));
    if (!file.open(QIODevice::ReadOnly))
        return false;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return false;

    *out = appFromJson(doc.object());
    return !out->id.isEmpty();
}

void CatalogCache::saveApp(const QString &repoId, const AppInfo &app)
{
    if (app.id.isEmpty())
        return;

    const QString root = cacheRoot();
    if (root.isEmpty())
        return;

    const QString repoKey = repoId.toLower();
    QDir().mkpath(root + QLatin1Char('/') + repoKey + QStringLiteral("/apps"));

    QSaveFile file(appPath(repoId, app.id));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;

    AppInfo stored = app;
    if (stored.repoId.isEmpty())
        stored.repoId = repoId;

    file.write(QJsonDocument(appToJson(stored)).toJson(QJsonDocument::Compact));
    if (!file.commit())
        return;

    QMutexLocker lock(&m_mutex);
    QStringList &ids = m_idsByRepo[repoKey];
    if (!ids.contains(app.id))
        ids.append(app.id);
    writeIndexLocked(repoId);
}

void CatalogCache::saveAppsBatch(const QString &repoId, const QVector<AppInfo> &apps)
{
    if (apps.isEmpty())
        return;

    const QString repoKey = repoId.toLower();
    const QString root = cacheRoot();
    if (root.isEmpty())
        return;

    QDir().mkpath(root + QLatin1Char('/') + repoKey + QStringLiteral("/apps"));

    QStringList newIds;
    {
        QMutexLocker lock(&m_mutex);
        newIds = m_idsByRepo.value(repoKey);
    }

    for (const AppInfo &app : apps) {
        if (app.id.isEmpty())
            continue;
        QSaveFile file(appPath(repoId, app.id));
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
            continue;
        AppInfo stored = app;
        if (stored.repoId.isEmpty())
            stored.repoId = repoId;
        file.write(QJsonDocument(appToJson(stored)).toJson(QJsonDocument::Compact));
        if (file.commit() && !newIds.contains(app.id))
            newIds.append(app.id);
    }

    QMutexLocker lock(&m_mutex);
    m_idsByRepo.insert(repoKey, newIds);
    writeIndexLocked(repoId);
}

void CatalogCache::writeIndexLocked(const QString &repoId)
{
    const QString repoKey = repoId.toLower();
    const QStringList ids = m_idsByRepo.value(repoKey);
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    m_indexUpdatedMs.insert(repoKey, now);

    QJsonObject root;
    root.insert(QStringLiteral("version"), kCacheVersion);
    root.insert(QStringLiteral("repoId"), repoId);
    root.insert(QStringLiteral("updatedAtMs"), now);
    QJsonArray idArray;
    for (const QString &id : ids)
        idArray.append(id);
    root.insert(QStringLiteral("ids"), idArray);

    QSaveFile file(indexPath(repoId));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    file.commit();
}

QVector<AppInfo> CatalogCache::searchLocal(const QString &repoId, const QString &query, int limit) const
{
    QVector<AppInfo> results;
    if (query.trimmed().size() < 2)
        return results;

    const QString needle = query.trimmed().toLower();
    const QStringList ids = enumerateIds(repoId);
    results.reserve((std::min)(limit, static_cast<int>(ids.size())));

    for (const QString &id : ids) {
        if (results.size() >= limit)
            break;
        AppInfo app;
        if (!loadApp(repoId, id, &app))
            continue;
        const QString haystack = (app.id + QLatin1Char(' ') + app.name + QLatin1Char(' ')
                                  + app.summary)
                                         .toLower();
        if (haystack.contains(needle))
            results.append(app);
    }
    return results;
}
