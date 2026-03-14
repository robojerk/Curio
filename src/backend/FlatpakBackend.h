#pragma once

#include <QObject>
#include <QHash>
#include <QVector>
#include <QString>

#include <functional>

#include "models/AppInfo.h"
#include "models/Operation.h"

class FlatpakBackend : public QObject
{
    Q_OBJECT
public:
    explicit FlatpakBackend(QObject *parent = nullptr);

    void refreshInstalled();
    void search(const QString &query);
    void fetchFlathubSuggestions();
    void listRemotes();
    void addRemote(const QString &name, const QString &url);
    void removeRemote(const QString &name);

    void install(const QString &appId);
    void uninstall(const QString &appId);
    void update(const QString &appId);

signals:
    void installedAppsUpdated(const QVector<AppInfo> &apps);
    void searchResultsUpdated(const QVector<AppInfo> &apps);
    void flathubSuggestionsUpdated(const QVector<AppInfo> &apps);
    void flathubCollectionsUpdated(const QVector<AppInfo> &trending,
                                   const QVector<AppInfo> &popular,
                                   const QVector<AppInfo> &recent,
                                   const QVector<AppInfo> &updated);

    void operationStarted(const Operation &op);
    void operationProgress(const Operation &op);
    void operationFinished(const Operation &op);
    void remotesUpdated(const QVector<QPair<QString, QString>> &remotes);
    void remoteAddFinished(bool ok, const QString &message);
    void remoteRemoveFinished(bool ok, const QString &message);

private:
    QVector<AppInfo> parseListOutput(const QByteArray &output) const;
    void runFlatpakCommand(const QStringList &arguments,
                           std::function<void (const QByteArray &)> onStdout,
                           std::function<void (int exitCode, const QByteArray &stderrData)> onFinished = {});
    QString cacheKeyForQuery(const QString &query) const;
    bool tryGetCachedSearch(const QString &query, QVector<AppInfo> *apps) const;
    void putCachedSearch(const QString &query, const QVector<AppInfo> &apps);
    void loadSearchCache();
    void saveSearchCache() const;

    class AppStreamProvider *m_appStreamProvider = nullptr;
    QHash<QString, QVector<AppInfo>> m_searchCache;
    QHash<QString, qint64> m_searchCacheTimestamps;
    QString m_cachePath;
    qint64 m_cacheTtlMs = 24LL * 60LL * 60LL * 1000LL;
    int m_cacheMaxEntries = 200;
    int m_cacheSchemaVersion = 2;
};

