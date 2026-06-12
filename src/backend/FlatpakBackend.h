#pragma once

#include <QObject>
#include <QSet>
#include <QHash>
#include <QVector>
#include <QString>
#include <QNetworkAccessManager>
#include <QTimer>

#include <memory>

#include "models/AppInfo.h"
#include "models/Operation.h"
#include "models/TrackedBuild.h"
#include "FlatpakScope.h"

class FlatpakInstallationService;
class FlatpakTransactionRunner;
class AppStreamProvider;

class FlatpakBackend : public QObject
{
    Q_OBJECT
public:
    explicit FlatpakBackend(QObject *parent = nullptr);
    ~FlatpakBackend();

    void refreshInstalled();
    void search(const QString &query);
    void fetchFlathubSuggestions();
    void fetchStoreSuggestions(const QString &repoId = QStringLiteral("flathub"),
                               bool forceRefresh = false);
    void refreshCatalogIndex(const QString &repoId = QStringLiteral("flathub"));
    void scheduleCatalogIndexAfterApiSuccess(const QString &repoId);
    void emitCachedDiscoveryData();
    bool flathubCollectionsFreshForToday() const;
    /** Load cached metadata for details/search without blocking on AppStream. */
    bool loadCachedAppMetadata(const QString &repoId, const QString &appId, AppInfo *out) const;
    void enrichAppMetadataAsync(const AppInfo &app);
    /** AppStream enrichment for detail views; list/browse avoids this for responsiveness. */
    void enrichAppMetadata(AppInfo &app) const;
    QString catalogPageUrlForInstalledApp(const QString &origin, const QString &appId) const;
    QString remoteConfigUrl(const QString &origin) const;
    void listRemotes();
    void addRemote(const QString &name, const QString &url);
    void removeRemote(const QString &name);

    void install(const QString &appId, const QString &repoId = QStringLiteral("flathub"));
    void installBundle(const QString &bundlePath, const QString &tempDirToCleanup = QString());
    /** Download a tracked-build release asset (.flatpak or .zip) and install it. */
    void installTrackedBuildAsset(const TrackedBuildProject &project,
                                  const QString &assetUrl,
                                  const QString &linkedAppId,
                                  const QString &releaseTag = QString());
    /** @return false if install could not be started (e.g. already in progress). */
    bool startTrackedBuildInstall(const TrackedBuildProject &project,
                                  const QString &assetUrl,
                                  const QString &linkedAppId,
                                  const QString &releaseTag = QString());
    void installReleaseAsset(const QString &assetUrl);

    QNetworkAccessManager *networkAccessManager() const { return m_networkManager; }
    void uninstall(const QString &appId);
    void launchApp(const QString &appId);
    void update(const QString &appId, const QString &installedVersion = QString());
    void updateInstalledFlatpakRef(const QString &flatpakRef, FlatpakScope scope);
    /** Install from tracked Git release when newer than installedVersion; returns true if handled. */
    bool tryInstallTrackedBuildUpdate(const QString &appId, const QString &installedVersion);
    void applyTrackedBuildMetadata(QVector<AppInfo> *apps) const;
    void refreshTrackedBuilds();
    void refreshRemoteUpdates();
    bool hasRemoteUpdate(const QString &appId) const;
    int remoteUpdateCount() const;
    int runtimeUpdateCount() const;
    QVector<AppInfo> runtimeUpdates() const;
    QVector<TrackedBuildProject> trackedBuildProjects() const;
    void upsertTrackedBuildProject(const TrackedBuildProject &project);
    void removeTrackedBuildProject(const QString &projectId);

signals:
    void installedAppsUpdated(const QVector<AppInfo> &apps);
    void searchResultsUpdated(const QVector<AppInfo> &apps);
    void searchResultsPatched(const QVector<AppInfo> &apps);
    void storeSuggestionsUpdated(const QString &repoId, const QVector<AppInfo> &apps);
    void storeCollectionsUpdated(const QString &repoId,
                                 const QVector<AppInfo> &trending,
                                 const QVector<AppInfo> &popular,
                                 const QVector<AppInfo> &recent,
                                 const QVector<AppInfo> &updated);
    void storeFetchFailed(const QString &repoId, const QString &message);
    /** Metadata patches from background remote-ls catalog index. */
    void storeCatalogIndexReady(const QString &repoId, const QVector<AppInfo> &patches);
    /** Incremental catalog index chunk (every ~200 apps). */
    void catalogChunkReady(const QString &repoId, const QVector<AppInfo> &apps);
    /** Batched store list patches (icons + catalog fields), coalesced ~80ms. */
    void storeCollectionsPatched(const QString &repoId, const QVector<AppInfo> &apps);
    /** Batched AppStream icon metadata for apps currently shown in store lists. */
    void storeIconsEnriched(const QString &repoId, const QVector<AppInfo> &apps);
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
    void trackedBuildProjectsUpdated(const QVector<TrackedBuildProject> &projects);
    void trackedBuildRefreshFinished();
    void trackedBuildRefreshFailed(const QString &message);
    void remoteUpdatesChecked(int appUpdateCount, int runtimeUpdateCount);
    void remoteUpdatesCheckFailed(const QString &message);
    void installedAppsMetadataEnriched(const QVector<AppInfo> &patches);
    void appMetadataEnriched(const AppInfo &app);
    void catalogIndexFinished(const QString &repoId);

private:
    QString cacheKeyForQuery(const QString &query) const;
    bool tryGetCachedSearch(const QString &query, QVector<AppInfo> *apps) const;
    void putCachedSearch(const QString &query, const QVector<AppInfo> &apps);
    void loadSearchCache();
    void saveSearchCache() const;
    void loadFlathubCache();
    void saveFlathubCache() const;
    void loadTrackedBuilds();
    void ensureBuiltInTrackedBuilds();
    void saveTrackedBuilds() const;
    void tagAppsWithRepo(QVector<AppInfo> &apps, const QString &repoId) const;
    static QString defaultStorePageUrlForApp(const QString &repoId, const QString &appId);
    void refreshTrackedBuildsForIndex(int index, int generation);
    void beginTrackedBuildRefresh();
    void completeTrackedBuildRefreshPass();
    void classifyAndStoreTrackedRelease(TrackedBuildProject *project, const QVector<TrackedBuildRelease> &releases) const;
    QString extractFlatpakFromZip(const QString &zipPath,
                                  const TrackedBuildProject &project,
                                  QString *error) const;
    void cleanupTrackedBuildTempDir(const QString &dirPath) const;
    void runFlatpakInstallForOperation(const Operation &op,
                                       const QString &bundlePath,
                                       const QString &tempDirToCleanup);
    void prepareRuntimeRemoteForBundle(FlatpakScope scope, const QString &bundlePath);
    QString installationScopeForApp(const QString &appId) const;
    void finalizeTrackedInstall(const Operation &op);
    void recordTrackedReleaseApplied(const QString &linkedAppId,
                                     const QString &releaseTag,
                                     const QString &assetUrl);
    static QString inferReleaseTagForAsset(const TrackedBuildProject &project, const QString &assetUrl);
    void syncTrackedApplyStateFromInstalled(const QVector<AppInfo> &apps);
    void scheduleStoreIconEnrichment(const QString &repoId,
                                     const QVector<AppInfo> &trending,
                                     const QVector<AppInfo> &popular,
                                     const QVector<AppInfo> &recent,
                                     const QVector<AppInfo> &updated);
    void processStoreIconEnrichmentBatch();
    void fetchStoreSuggestionsViaRemoteLs(const QString &normalizedRepoId);
    void fetchFlathubCollectionsViaApi(const QString &normalizedRepoId,
                                       bool allowRemoteLsFallback = true,
                                       bool forceRefresh = false);
    void applyFlathubCollections(const QString &normalizedRepoId,
                                 QVector<AppInfo> trending,
                                 QVector<AppInfo> popular,
                                 QVector<AppInfo> recent,
                                 QVector<AppInfo> updated,
                                 const QString &cacheSource);
    static QVector<AppInfo> mergeAppListsById(const QVector<AppInfo> &fresh,
                                              const QVector<AppInfo> &cached);
    static void mergeCatalogFields(AppInfo *app, const AppInfo &catalog);
    void patchCollectionsFromCatalogIndex(const QString &repoId);
    void queueStoreCollectionPatch(const AppInfo &app);
    void flushStoreCollectionPatches();
    void refreshCatalogIndexInProcess(const QString &repoId);
    void refreshCatalogIndexViaSubprocess(const QString &repoId);
    static QString todayUtcCollectionDay();
    void scheduleInstalledMetadataEnrichment(const QVector<AppInfo> &apps);
    void processInstalledMetadataEnrichmentBatch();
    void publishSearchResults(const QString &query, QVector<AppInfo> apps);
    void hydrateSearchResultsFromCache(QVector<AppInfo> &apps) const;
    void runLibflatpakSearch(const QString &query);
    void scheduleSearchMetadataEnrichment(const QString &query, const QVector<AppInfo> &apps);
    void processSearchMetadataEnrichmentBatch();

    std::unique_ptr<FlatpakInstallationService> m_installations;
    FlatpakTransactionRunner *m_transactionRunner = nullptr;
    AppStreamProvider *m_appStreamProvider = nullptr;
    QNetworkAccessManager *m_networkManager = nullptr;
    class FlathubApiClient *m_flathubApiClient = nullptr;
    class TrackedBuildSource *m_trackedBuildSource = nullptr;
    class CatalogCache *m_catalogCache = nullptr;
    QHash<QString, QVector<AppInfo>> m_searchCache;
    QHash<QString, qint64> m_searchCacheTimestamps;
    QString m_cachePath;
    qint64 m_cacheTtlMs = 24LL * 60LL * 60LL * 1000LL;
    int m_cacheMaxEntries = 200;
    int m_cacheSchemaVersion = 4;
    QString m_flathubCachePath;
    QVector<AppInfo> m_cachedFlathubTrending;
    QVector<AppInfo> m_cachedFlathubPopular;
    QVector<AppInfo> m_cachedFlathubRecent;
    QVector<AppInfo> m_cachedFlathubUpdated;
    qint64 m_flathubCacheTimestampMs = 0;
    QString m_flathubCollectionDay;
    QString m_flathubCacheSource;
    bool m_discoveryCacheEmitted = false;
    QHash<QString, AppInfo> m_catalogIndex;
    QString m_catalogIndexRepoId;
    QString m_trackedBuildsPath;
    QVector<TrackedBuildProject> m_trackedBuildProjects;
    QHash<QString, QString> m_remoteConfigUrls;
    int m_pendingTrackedBuildChecks = 0;
    int m_trackedBuildRefreshGeneration = 0;
    bool m_trackedBuildRefreshInFlight = false;
    bool m_trackedBuildRefreshReschedule = false;
    QSet<QString> m_activeTrackedInstallAppIds;
    struct PendingTrackedInstall {
        QString releaseTag;
        QString assetUrl;
    };
    QHash<QString, PendingTrackedInstall> m_pendingTrackedInstalls;
    QTimer *m_storeIconEnrichTimer = nullptr;
    QTimer *m_storeIconEnrichDeferTimer = nullptr;
    QString m_storeIconEnrichRepoId;
    QVector<AppInfo> m_storeIconEnrichQueue;
    int m_storeIconEnrichIndex = 0;
    bool m_storeIconEnrichCacheDirty = false;
    QTimer *m_installedEnrichTimer = nullptr;
    QVector<AppInfo> m_installedEnrichQueue;
    int m_installedEnrichIndex = 0;
    QString m_activeSearchQuery;
    QTimer *m_searchEnrichTimer = nullptr;
    QVector<AppInfo> m_searchEnrichQueue;
    int m_searchEnrichIndex = 0;
    QString m_searchEnrichQuery;
    QTimer *m_uiCoalesceTimer = nullptr;
    QHash<QString, AppInfo> m_pendingStorePatches;
    QString m_pendingStorePatchRepoId;
    QSet<QString> m_remoteUpdateAppIds;
    QVector<AppInfo> m_runtimeUpdates;
    bool m_catalogIndexRunning = false;
    QString m_pendingCatalogIndexRepoId;
};
