#pragma once

#include "GitHost.h"
#include "models/TrackedBuild.h"

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QVector>

#include <functional>

class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;

class TrackedBuildSource : public QObject
{
    Q_OBJECT
public:
    explicit TrackedBuildSource(QNetworkAccessManager *networkManager, QObject *parent = nullptr);

    void fetchReleases(const TrackedBuildProject &project,
                       const std::function<void(const QVector<TrackedBuildRelease> &, const QString &)> &callback);

    static QVector<TrackedBuildRelease> parseGithubReleases(const QByteArray &payload, QString *error);
    static QVector<TrackedBuildRelease> parseGitLabReleases(const QByteArray &payload, QString *error);
    static QVector<TrackedBuildRelease> parseForgejoReleases(const QByteArray &payload, QString *error);

private:
    struct ReleaseCacheEntry {
        QVector<TrackedBuildRelease> releases;
        QString error;
        QDateTime fetchedAtUtc;
    };

    void applyAuthHeaders(QNetworkRequest &request, GitHostKind kind) const;
    static void applyProviderHeaders(QNetworkRequest &request, GitHostKind kind);
    static QString httpErrorMessage(int statusCode, const QByteArray &body, GitHostKind kind);
    bool tryReleaseCache(const QString &cacheKey, bool allowStale,
                         QVector<TrackedBuildRelease> *releases, QString *error) const;
    void storeReleaseCache(const QString &cacheKey, const QVector<TrackedBuildRelease> &releases,
                           const QString &error);

    QNetworkAccessManager *m_networkManager = nullptr;
    QHash<QString, ReleaseCacheEntry> m_releaseCache;
};
