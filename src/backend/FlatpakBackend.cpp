#include "FlatpakBackend.h"
#include "AppStreamProvider.h"
#include "FlatpakInstallationService.h"
#include "FlatpakRemoteCatalog.h"
#include "FlatpakScope.h"
#include "FlatpakTransactionRunner.h"
#include "FlathubApiClient.h"
#include "TrackedBuildClassifier.h"
#include "TrackedBuildSource.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QDebug>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QNetworkProxyFactory>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QStringList>
#include <QTextStream>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUrl>
#include <QTimer>
#include <QUuid>
#include <QEventLoop>
#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>
#include <limits>
#include <QSet>
#include <algorithm>

namespace {
void dedupeScreenshotUrls(AppInfo &app);

QByteArray fetchUrlSync(QNetworkAccessManager *networkManager, const QString &url, QString *errorOut)
{
    if (!networkManager || url.trimmed().isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("Download URL is empty");
        return {};
    }

    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("curio-tracked-builds"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply *reply = networkManager->get(request);

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        if (errorOut)
            *errorOut = reply->errorString();
        reply->deleteLater();
        return {};
    }

    const QByteArray data = reply->readAll();
    reply->deleteLater();
    return data;
}

QJsonObject appInfoToJson(const AppInfo &app)
{
    QJsonObject appObj;
    appObj.insert(QStringLiteral("repoId"), app.repoId);
    appObj.insert(QStringLiteral("templateId"), app.templateId);
    appObj.insert(QStringLiteral("storePageUrl"), app.storePageUrl);
    appObj.insert(QStringLiteral("id"), app.id);
    appObj.insert(QStringLiteral("name"), app.name);
    appObj.insert(QStringLiteral("summary"), app.summary);
    appObj.insert(QStringLiteral("version"), app.version);
    appObj.insert(QStringLiteral("iconName"), app.iconName);
    appObj.insert(QStringLiteral("installed"), app.installed);
    appObj.insert(QStringLiteral("longDescription"), app.longDescription);
    appObj.insert(QStringLiteral("iconUrl"), app.iconUrl);

    QJsonArray screenshots;
    for (const QString &u : app.screenshotUrls)
        screenshots.append(u);
    appObj.insert(QStringLiteral("screenshotUrls"), screenshots);

    appObj.insert(QStringLiteral("developerName"), app.developerName);
    appObj.insert(QStringLiteral("projectLicense"), app.projectLicense);
    appObj.insert(QStringLiteral("homepageUrl"), app.homepageUrl);
    appObj.insert(QStringLiteral("bugtrackerUrl"), app.bugtrackerUrl);
    appObj.insert(QStringLiteral("donateUrl"), app.donateUrl);
    appObj.insert(QStringLiteral("helpUrl"), app.helpUrl);
    appObj.insert(QStringLiteral("translateUrl"), app.translateUrl);

    QJsonArray categories;
    for (const QString &cat : app.categories)
        categories.append(cat);
    appObj.insert(QStringLiteral("categories"), categories);

    appObj.insert(QStringLiteral("trackedBuildProviderId"), app.trackedBuildProviderId);
    appObj.insert(QStringLiteral("trackedBuildRepoSlug"), app.trackedBuildRepoSlug);
    appObj.insert(QStringLiteral("trackedStableVersion"), app.trackedStableVersion);
    appObj.insert(QStringLiteral("trackedStablePublishedAtIso"), app.trackedStablePublishedAtIso);
    appObj.insert(QStringLiteral("trackedStableAssetUrl"), app.trackedStableAssetUrl);
    appObj.insert(QStringLiteral("trackedNightlyVersion"), app.trackedNightlyVersion);
    appObj.insert(QStringLiteral("trackedNightlyPublishedAtIso"), app.trackedNightlyPublishedAtIso);
    appObj.insert(QStringLiteral("trackedNightlyAssetUrl"), app.trackedNightlyAssetUrl);
    appObj.insert(QStringLiteral("trackedBuildLastError"), app.trackedBuildLastError);
    return appObj;
}

AppInfo appInfoFromJson(const QJsonObject &appObj)
{
    AppInfo app;
    app.repoId = appObj.value(QStringLiteral("repoId")).toString(QStringLiteral("flathub"));
    app.templateId = appObj.value(QStringLiteral("templateId")).toString(QStringLiteral("default"));
    app.storePageUrl = appObj.value(QStringLiteral("storePageUrl")).toString();
    app.id = appObj.value(QStringLiteral("id")).toString();
    app.name = appObj.value(QStringLiteral("name")).toString();
    app.summary = appObj.value(QStringLiteral("summary")).toString();
    app.version = appObj.value(QStringLiteral("version")).toString();
    app.iconName = appObj.value(QStringLiteral("iconName")).toString();
    app.installed = appObj.value(QStringLiteral("installed")).toBool();
    app.longDescription = appObj.value(QStringLiteral("longDescription")).toString();
    app.iconUrl = appObj.value(QStringLiteral("iconUrl")).toString();
    for (const QJsonValue &u : appObj.value(QStringLiteral("screenshotUrls")).toArray())
        app.screenshotUrls.append(u.toString());
    app.developerName = appObj.value(QStringLiteral("developerName")).toString();
    app.projectLicense = appObj.value(QStringLiteral("projectLicense")).toString();
    app.homepageUrl = appObj.value(QStringLiteral("homepageUrl")).toString();
    app.bugtrackerUrl = appObj.value(QStringLiteral("bugtrackerUrl")).toString();
    app.donateUrl = appObj.value(QStringLiteral("donateUrl")).toString();
    app.helpUrl = appObj.value(QStringLiteral("helpUrl")).toString();
    app.translateUrl = appObj.value(QStringLiteral("translateUrl")).toString();
    for (const QJsonValue &c : appObj.value(QStringLiteral("categories")).toArray())
        app.categories.append(c.toString());
    app.trackedBuildProviderId = appObj.value(QStringLiteral("trackedBuildProviderId")).toString();
    app.trackedBuildRepoSlug = appObj.value(QStringLiteral("trackedBuildRepoSlug")).toString();
    app.trackedStableVersion = appObj.value(QStringLiteral("trackedStableVersion")).toString();
    app.trackedStablePublishedAtIso = appObj.value(QStringLiteral("trackedStablePublishedAtIso")).toString();
    app.trackedStableAssetUrl = appObj.value(QStringLiteral("trackedStableAssetUrl")).toString();
    app.trackedNightlyVersion = appObj.value(QStringLiteral("trackedNightlyVersion")).toString();
    app.trackedNightlyPublishedAtIso = appObj.value(QStringLiteral("trackedNightlyPublishedAtIso")).toString();
    app.trackedNightlyAssetUrl = appObj.value(QStringLiteral("trackedNightlyAssetUrl")).toString();
    app.trackedBuildLastError = appObj.value(QStringLiteral("trackedBuildLastError")).toString();
    dedupeScreenshotUrls(app);
    return app;
}

QJsonArray appsToJsonArray(const QVector<AppInfo> &apps)
{
    QJsonArray arr;
    for (const AppInfo &app : apps)
        arr.append(appInfoToJson(app));
    return arr;
}

QVector<AppInfo> appsFromJsonArray(const QJsonArray &arr)
{
    QVector<AppInfo> apps;
    apps.reserve(arr.size());
    for (const QJsonValue &appVal : arr) {
        const QJsonObject appObj = appVal.toObject();
        if (appObj.isEmpty())
            continue;
        const AppInfo app = appInfoFromJson(appObj);
        if (app.id.isEmpty())
            continue;
        apps.append(app);
    }
    return apps;
}

void dedupeScreenshotUrls(AppInfo &app)
{
    if (app.screenshotUrls.isEmpty())
        return;
    QSet<QString> seen;
    QStringList unique;
    for (const QString &raw : app.screenshotUrls) {
        const QUrl url(raw);
        const QString key = url.isValid()
                ? url.toString(QUrl::RemoveQuery | QUrl::RemoveFragment)
                : raw;
        if (key.isEmpty() || seen.contains(key))
            continue;
        seen.insert(key);
        unique.append(raw);
    }
    app.screenshotUrls = unique;
}

QVector<AppInfo> pickTopByScore(const QVector<AppInfo> &apps,
                                const std::function<int (const AppInfo &, int)> &scoreFn,
                                int limit)
{
    struct Ranked {
        AppInfo app;
        int score = 0;
        int index = 0;
    };

    QVector<Ranked> ranked;
    ranked.reserve(apps.size());
    for (int i = 0; i < apps.size(); ++i) {
        ranked.push_back({apps.at(i), scoreFn(apps.at(i), i), i});
    }

    std::stable_sort(ranked.begin(), ranked.end(), [](const Ranked &a, const Ranked &b) {
        if (a.score != b.score)
            return a.score > b.score;
        return a.index < b.index;
    });

    QVector<AppInfo> selected;
    selected.reserve(limit);
    QSet<QString> seenIds;
    for (const Ranked &r : ranked) {
        if (selected.size() >= limit)
            break;
        if (r.app.id.isEmpty() || seenIds.contains(r.app.id))
            continue;
        seenIds.insert(r.app.id);
        selected.push_back(r.app);
    }
    return selected;
}

struct StoreDiscoveryResult {
    QString repoId;
    QVector<AppInfo> trending;
    QVector<AppInfo> popular;
    QVector<AppInfo> recent;
    QVector<AppInfo> updated;
};

StoreDiscoveryResult buildStoreDiscoveryResult(const QVector<AppInfo> &apps, const QString &repoId)
{
    StoreDiscoveryResult result;
    result.repoId = repoId;

    if (apps.isEmpty())
        return result;

    const QStringList keywords = {
        QStringLiteral("browser"), QStringLiteral("chat"),
        QStringLiteral("music"), QStringLiteral("video"),
        QStringLiteral("photo"), QStringLiteral("graphics"),
        QStringLiteral("office"), QStringLiteral("game"),
        QStringLiteral("dev"), QStringLiteral("code"),
        QStringLiteral("education"), QStringLiteral("network"),
        QStringLiteral("science"), QStringLiteral("system"),
        QStringLiteral("utility"), QStringLiteral("mobile")
    };

    const QSet<QString> popularIds = {
        QStringLiteral("org.mozilla.firefox"),
        QStringLiteral("org.libreoffice.LibreOffice"),
        QStringLiteral("org.videolan.VLC"),
        QStringLiteral("org.gimp.GIMP"),
        QStringLiteral("org.inkscape.Inkscape"),
        QStringLiteral("com.valvesoftware.Steam"),
        QStringLiteral("com.spotify.Client"),
        QStringLiteral("com.discordapp.Discord"),
        QStringLiteral("org.signal.Signal"),
        QStringLiteral("com.slack.Slack"),
        QStringLiteral("org.kde.kdenlive"),
        QStringLiteral("md.obsidian.Obsidian")
    };

    result.trending = pickTopByScore(apps, [keywords](const AppInfo &app, int) {
        const QString haystack = (app.name + QLatin1Char(' ') + app.summary).toLower();
        int score = 0;
        for (const QString &kw : keywords) {
            if (haystack.contains(kw))
                ++score;
        }
        if (!app.summary.trimmed().isEmpty())
            score += 2;
        return score;
    }, 24);

    result.popular = pickTopByScore(apps, [popularIds, keywords](const AppInfo &app, int) {
        const QString haystack = (app.name + QLatin1Char(' ') + app.summary).toLower();
        int score = 0;
        if (popularIds.contains(app.id))
            score += 50;
        if (haystack.contains(QStringLiteral("official")))
            score += 3;
        for (const QString &kw : keywords) {
            if (haystack.contains(kw))
                ++score;
        }
        if (!app.summary.trimmed().isEmpty())
            score += 2;
        return score;
    }, 24);

    result.recent = pickTopByScore(apps, [](const AppInfo &app, int idx) {
        return static_cast<int>(qHash(app.id) % 10000) - (idx / 10);
    }, 24);

    result.updated = pickTopByScore(apps, [](const AppInfo &app, int) {
        int score = 0;
        const QString v = app.version;
        QRegularExpression rx(QStringLiteral("(\\d+)"));
        QRegularExpressionMatchIterator it = rx.globalMatch(v);
        int factor = 10000;
        while (it.hasNext() && factor > 1) {
            const auto m = it.next();
            score += m.captured(1).toInt() * factor;
            factor /= 10;
        }
        if (!v.trimmed().isEmpty())
            score += 5;
        return score;
    }, 24);

    return result;
}

QJsonObject trackedBuildToJson(const TrackedBuildProject &project)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("id"), project.id);
    obj.insert(QStringLiteral("providerId"), project.providerId);
    obj.insert(QStringLiteral("repoSlug"), project.repoSlug);
    obj.insert(QStringLiteral("linkedAppId"), project.linkedAppId);
    obj.insert(QStringLiteral("trackStable"), true);
    obj.insert(QStringLiteral("trackNightly"), project.includePrereleases);
    obj.insert(QStringLiteral("includePrereleases"), project.includePrereleases);
    obj.insert(QStringLiteral("assetFilterRegex"), project.assetFilterRegex);
    obj.insert(QStringLiteral("releaseTitleFilterRegex"), project.releaseTitleFilterRegex);
    obj.insert(QStringLiteral("excludeRegex"), project.excludeRegex);
    obj.insert(QStringLiteral("includeZipAssets"), project.includeZipAssets);
    obj.insert(QStringLiteral("zippedFlatpakFilterRegex"), project.zippedFlatpakFilterRegex);
    obj.insert(QStringLiteral("autoFlatpakFilterByArch"), project.autoFlatpakFilterByArch);
    obj.insert(QStringLiteral("latestStableVersion"), project.latestStableVersion);
    obj.insert(QStringLiteral("latestStablePublishedAtIso"), project.latestStablePublishedAtIso);
    obj.insert(QStringLiteral("latestStableAssetUrl"), project.latestStableAssetUrl);
    obj.insert(QStringLiteral("latestNightlyVersion"), project.latestNightlyVersion);
    obj.insert(QStringLiteral("latestNightlyPublishedAtIso"), project.latestNightlyPublishedAtIso);
    obj.insert(QStringLiteral("latestNightlyAssetUrl"), project.latestNightlyAssetUrl);
    obj.insert(QStringLiteral("lastCheckedAtIso"), project.lastCheckedAtIso);
    obj.insert(QStringLiteral("lastError"), project.lastError);
    obj.insert(QStringLiteral("lastAppliedReleaseTag"), project.lastAppliedReleaseTag);
    obj.insert(QStringLiteral("lastAppliedAssetUrl"), project.lastAppliedAssetUrl);
    obj.insert(QStringLiteral("lastAppliedAtIso"), project.lastAppliedAtIso);
    QJsonArray unclassified;
    for (const QString &tag : project.unclassifiedTags)
        unclassified.append(tag);
    obj.insert(QStringLiteral("unclassifiedTags"), unclassified);
    return obj;
}

TrackedBuildProject trackedBuildFromJson(const QJsonObject &obj)
{
    TrackedBuildProject project;
    project.id = obj.value(QStringLiteral("id")).toString();
    project.providerId = obj.value(QStringLiteral("providerId")).toString(QStringLiteral("github"));
    project.repoSlug = obj.value(QStringLiteral("repoSlug")).toString();
    project.linkedAppId = obj.value(QStringLiteral("linkedAppId")).toString();
    project.trackStable = obj.value(QStringLiteral("trackStable")).toBool(true);
    project.trackNightly = obj.value(QStringLiteral("trackNightly")).toBool(false);
    if (obj.contains(QStringLiteral("includePrereleases")))
        project.includePrereleases = obj.value(QStringLiteral("includePrereleases")).toBool(false);
    else
        project.includePrereleases = project.trackNightly;
    project.assetFilterRegex = obj.value(QStringLiteral("assetFilterRegex")).toString();
    project.releaseTitleFilterRegex = obj.value(QStringLiteral("releaseTitleFilterRegex")).toString();
    project.stableTagRegex = obj.value(QStringLiteral("stableTagRegex")).toString();
    project.nightlyTagRegex = obj.value(QStringLiteral("nightlyTagRegex")).toString();
    project.stableAssetRegex = obj.value(QStringLiteral("stableAssetRegex")).toString();
    project.nightlyAssetRegex = obj.value(QStringLiteral("nightlyAssetRegex")).toString();
    if (project.assetFilterRegex.isEmpty())
        project.assetFilterRegex = trackedBuildEffectiveAssetFilterRegex(project);
    if (project.releaseTitleFilterRegex.isEmpty())
        project.releaseTitleFilterRegex = trackedBuildEffectiveReleaseTitleFilterRegex(project);
    project.excludeRegex = obj.value(QStringLiteral("excludeRegex")).toString();
    project.includeZipAssets = obj.value(QStringLiteral("includeZipAssets")).toBool(false);
    project.zippedFlatpakFilterRegex = obj.value(QStringLiteral("zippedFlatpakFilterRegex")).toString();
    project.autoFlatpakFilterByArch = obj.value(QStringLiteral("autoFlatpakFilterByArch")).toBool(true);
    project.latestStableVersion = obj.value(QStringLiteral("latestStableVersion")).toString();
    project.latestStablePublishedAtIso = obj.value(QStringLiteral("latestStablePublishedAtIso")).toString();
    project.latestStableAssetUrl = obj.value(QStringLiteral("latestStableAssetUrl")).toString();
    project.latestNightlyVersion = obj.value(QStringLiteral("latestNightlyVersion")).toString();
    project.latestNightlyPublishedAtIso = obj.value(QStringLiteral("latestNightlyPublishedAtIso")).toString();
    project.latestNightlyAssetUrl = obj.value(QStringLiteral("latestNightlyAssetUrl")).toString();
    project.lastCheckedAtIso = obj.value(QStringLiteral("lastCheckedAtIso")).toString();
    project.lastError = obj.value(QStringLiteral("lastError")).toString();
    project.lastAppliedReleaseTag = obj.value(QStringLiteral("lastAppliedReleaseTag")).toString();
    project.lastAppliedAssetUrl = obj.value(QStringLiteral("lastAppliedAssetUrl")).toString();
    project.lastAppliedAtIso = obj.value(QStringLiteral("lastAppliedAtIso")).toString();
    for (const QJsonValue &value : obj.value(QStringLiteral("unclassifiedTags")).toArray())
        project.unclassifiedTags.append(value.toString());
    return project;
}

TrackedBuildProject defaultCurioTrackedBuildProject()
{
    TrackedBuildProject project;
    project.id = trackedBuildBuiltinCurioId();
    project.providerId = QStringLiteral("github");
    project.repoSlug = trackedBuildBuiltinRepoSlug();
    project.linkedAppId = trackedBuildBuiltinLinkedAppId();
    project.trackStable = true;
    project.trackNightly = false;
    project.includePrereleases = false;
    const QString arch = TrackedBuildClassifier::hostArchitectureToken();
    project.assetFilterRegex = QStringLiteral(R"(io\.github\.curio\.Curio-%1\.flatpak)")
            .arg(QRegularExpression::escape(arch));
    project.autoFlatpakFilterByArch = true;
    return project;
}
}

FlatpakBackend::FlatpakBackend(QObject *parent)
    : QObject(parent)
    , m_installations(std::make_unique<FlatpakInstallationService>())
    , m_appStreamProvider(new AppStreamProvider(this))
    , m_networkManager(new QNetworkAccessManager(this))
    , m_flathubApiClient(new FlathubApiClient(m_networkManager, this))
    , m_trackedBuildSource(new TrackedBuildSource(m_networkManager, this))
{
    Q_ASSERT(m_networkManager);
    Q_ASSERT(m_trackedBuildSource);
    if (!m_installations->isValid())
        qWarning() << "FlatpakBackend:" << m_installations->lastError();

    m_transactionRunner = new FlatpakTransactionRunner(m_installations.get(), this);
    connect(m_transactionRunner,
            &FlatpakTransactionRunner::operationProgress,
            this,
            &FlatpakBackend::operationProgress);
    connect(m_transactionRunner, &FlatpakTransactionRunner::operationFinished, this, [this](const Operation &op) {
        if (op.status == OperationStatus::Succeeded) {
            finalizeTrackedInstall(op);
            refreshInstalled();
        } else if (m_pendingTrackedInstalls.contains(op.appId)) {
            m_pendingTrackedInstalls.remove(op.appId);
        }
        emit operationFinished(op);
    });

    QNetworkProxyFactory::setUseSystemConfiguration(true);
    m_networkManager->setTransferTimeout(60'000);

    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!dataDir.isEmpty()) {
        QDir().mkpath(dataDir);
        m_cachePath = dataDir + QStringLiteral("/search_cache.json");
        m_flathubCachePath = dataDir + QStringLiteral("/flathub_cache.json");
        m_trackedBuildsPath = dataDir + QStringLiteral("/tracked_builds.json");
        loadSearchCache();
        loadFlathubCache();
        loadTrackedBuilds();
        ensureBuiltInTrackedBuilds();
    }
    if (qEnvironmentVariableIsSet("CURIO_APPSTREAM_DEBUG")) {
        qDebug() << "FlatpakBackend: AppStream provider available =" << m_appStreamProvider->isAvailable();
    }
}

bool FlatpakBackend::flathubCollectionsFreshForToday() const
{
    if (m_flathubCollectionDay != todayUtcCollectionDay())
        return false;
    // Trending is the default feed tab; partial cache must not skip the API.
    return !m_cachedFlathubTrending.isEmpty();
}

void FlatpakBackend::emitCachedDiscoveryData()
{
    if (m_flathubCacheTimestampMs <= 0)
        return;
    if (m_cachedFlathubTrending.isEmpty() && m_cachedFlathubPopular.isEmpty()
        && m_cachedFlathubRecent.isEmpty() && m_cachedFlathubUpdated.isEmpty()) {
        return;
    }
    m_discoveryCacheEmitted = true;

    emit storeCollectionsUpdated(QStringLiteral("flathub"),
                                 m_cachedFlathubTrending,
                                 m_cachedFlathubPopular,
                                 m_cachedFlathubRecent,
                                 m_cachedFlathubUpdated);
}

void FlatpakBackend::enrichAppMetadata(AppInfo &app) const
{
    m_appStreamProvider->enrich(app);
    dedupeScreenshotUrls(app);
}

QString FlatpakBackend::catalogPageUrlForInstalledApp(const QString &origin, const QString &appId) const
{
    return FlatpakRemoteCatalog::catalogPageUrlForApp(origin, appId);
}

QString FlatpakBackend::remoteConfigUrl(const QString &origin) const
{
    if (origin.trimmed().isEmpty())
        return QString();
    return m_remoteConfigUrls.value(origin.trimmed().toLower());
}

void FlatpakBackend::scheduleStoreIconEnrichment(const QString &repoId,
                                                 const QVector<AppInfo> &trending,
                                                 const QVector<AppInfo> &popular,
                                                 const QVector<AppInfo> &recent,
                                                 const QVector<AppInfo> &updated)
{
    QHash<QString, AppInfo> unique;
    const auto addList = [&unique](const QVector<AppInfo> &list) {
        for (const AppInfo &app : list) {
            if (app.id.isEmpty())
                continue;
            if (!app.iconUrl.isEmpty())
                continue;
            unique.insert(app.id, app);
        }
    };
    addList(trending);
    addList(popular);
    addList(recent);
    addList(updated);

    if (unique.isEmpty())
        return;

    if (m_storeIconEnrichTimer)
        m_storeIconEnrichTimer->stop();

    m_storeIconEnrichRepoId = repoId;
    m_storeIconEnrichQueue = unique.values();
    m_storeIconEnrichIndex = 0;

    if (!m_storeIconEnrichDeferTimer) {
        m_storeIconEnrichDeferTimer = new QTimer(this);
        m_storeIconEnrichDeferTimer->setSingleShot(true);
        connect(m_storeIconEnrichDeferTimer, &QTimer::timeout, this, [this]() {
            if (m_storeIconEnrichQueue.isEmpty())
                return;
            if (!m_storeIconEnrichTimer) {
                m_storeIconEnrichTimer = new QTimer(this);
                m_storeIconEnrichTimer->setSingleShot(false);
                m_storeIconEnrichTimer->setInterval(80);
                connect(m_storeIconEnrichTimer,
                        &QTimer::timeout,
                        this,
                        &FlatpakBackend::processStoreIconEnrichmentBatch);
            }
            if (!m_storeIconEnrichTimer->isActive())
                m_storeIconEnrichTimer->start();
        });
    }
    m_storeIconEnrichDeferTimer->start(3000);
}

void FlatpakBackend::processStoreIconEnrichmentBatch()
{
    constexpr int kBatchSize = 2;
    QVector<AppInfo> enrichedBatch;
    enrichedBatch.reserve(kBatchSize);

    while (m_storeIconEnrichIndex < m_storeIconEnrichQueue.size()
           && enrichedBatch.size() < kBatchSize) {
        AppInfo app = m_storeIconEnrichQueue.at(m_storeIconEnrichIndex++);
        enrichAppMetadata(app);
        enrichedBatch.append(app);
    }

    if (!enrichedBatch.isEmpty()) {
        if (m_storeIconEnrichRepoId.compare(QStringLiteral("flathub"), Qt::CaseInsensitive) == 0) {
            const auto patchList = [&enrichedBatch](QVector<AppInfo> &list) {
                for (AppInfo &app : list) {
                    for (const AppInfo &enriched : enrichedBatch) {
                        if (app.id == enriched.id) {
                            app.iconName = enriched.iconName;
                            app.iconUrl = enriched.iconUrl;
                            break;
                        }
                    }
                }
            };
            patchList(m_cachedFlathubTrending);
            patchList(m_cachedFlathubPopular);
            patchList(m_cachedFlathubRecent);
            patchList(m_cachedFlathubUpdated);
            m_storeIconEnrichCacheDirty = true;
        }
        emit storeIconsEnriched(m_storeIconEnrichRepoId, enrichedBatch);
    }

    if (m_storeIconEnrichIndex >= m_storeIconEnrichQueue.size()) {
        m_storeIconEnrichTimer->stop();
        if (m_storeIconEnrichCacheDirty) {
            m_storeIconEnrichCacheDirty = false;
            saveFlathubCache();
        }
    }
}

void FlatpakBackend::refreshInstalled()
{
    if (!m_installations || !m_installations->isValid()) {
        emit installedAppsUpdated({});
        return;
    }

    auto *watcher = new QFutureWatcher<QVector<AppInfo>>(this);
    connect(watcher, &QFutureWatcher<QVector<AppInfo>>::finished, this, [this, watcher]() {
        QVector<AppInfo> apps = watcher->result();
        for (AppInfo &app : apps)
            app.installed = true;
        applyTrackedBuildMetadata(&apps);
        syncTrackedApplyStateFromInstalled(apps);
        emit installedAppsUpdated(apps);
        scheduleInstalledMetadataEnrichment(apps);
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([installations = m_installations.get()]() {
        return installations->listInstalledApps();
    }));
}

void FlatpakBackend::search(const QString &query)
{
    QVector<AppInfo> cached;
    if (tryGetCachedSearch(query, &cached)) {
        applyTrackedBuildMetadata(&cached);
        emit searchResultsUpdated(cached);
        if (qEnvironmentVariableIsSet("CURIO_APPSTREAM_DEBUG")) {
            qDebug() << "FlatpakBackend: cache hit for query" << query;
        }
        return;
    }

    if (!m_installations || !m_installations->isValid()) {
        emit searchResultsUpdated({});
        return;
    }

    auto *watcher = new QFutureWatcher<QVector<AppInfo>>(this);
    connect(watcher, &QFutureWatcher<QVector<AppInfo>>::finished, this, [this, watcher, query]() {
        QVector<AppInfo> apps = watcher->result();
        applyTrackedBuildMetadata(&apps);
        putCachedSearch(query, apps);
        emit searchResultsUpdated(apps);
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([installations = m_installations.get(), query]() {
        return installations->searchApps(query);
    }));
}

void FlatpakBackend::fetchFlathubSuggestions()
{
    fetchStoreSuggestions(QStringLiteral("flathub"));
}

QString FlatpakBackend::todayUtcCollectionDay()
{
    return QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyy-MM-dd"));
}

QVector<AppInfo> FlatpakBackend::mergeAppListsById(const QVector<AppInfo> &fresh,
                                                   const QVector<AppInfo> &cached)
{
    QHash<QString, AppInfo> cachedById;
    for (const AppInfo &app : cached) {
        if (!app.id.isEmpty())
            cachedById.insert(app.id, app);
    }

    QVector<AppInfo> merged;
    merged.reserve(fresh.size());
    for (AppInfo app : fresh) {
        const auto it = cachedById.constFind(app.id);
        if (it != cachedById.constEnd()) {
            const AppInfo &prev = it.value();
            if (app.name.isEmpty() || app.name == app.id)
                app.name = prev.name;
            if (app.summary.isEmpty())
                app.summary = prev.summary;
            if (app.version.isEmpty())
                app.version = prev.version;
            if (app.iconUrl.isEmpty())
                app.iconUrl = prev.iconUrl;
            if (app.iconName.isEmpty() || app.iconName == app.id)
                app.iconName = prev.iconName.isEmpty() ? app.id : prev.iconName;
            if (app.longDescription.isEmpty())
                app.longDescription = prev.longDescription;
            for (const QString &url : prev.screenshotUrls) {
                if (!app.screenshotUrls.contains(url))
                    app.screenshotUrls.append(url);
            }
        }
        merged.append(app);
    }
    return merged;
}

void FlatpakBackend::mergeCatalogFields(AppInfo *app, const AppInfo &catalog)
{
    if (app == nullptr || app->id.isEmpty())
        return;
    if (app->name.isEmpty() || app->name == app->id)
        app->name = catalog.name;
    if (app->summary.isEmpty())
        app->summary = catalog.summary;
    if (app->version.isEmpty())
        app->version = catalog.version;
}

void FlatpakBackend::applyFlathubCollections(const QString &normalizedRepoId,
                                             QVector<AppInfo> trending,
                                             QVector<AppInfo> popular,
                                             QVector<AppInfo> recent,
                                             QVector<AppInfo> updated,
                                             const QString &cacheSource)
{
    tagAppsWithRepo(trending, normalizedRepoId);
    tagAppsWithRepo(popular, normalizedRepoId);
    tagAppsWithRepo(recent, normalizedRepoId);
    tagAppsWithRepo(updated, normalizedRepoId);
    applyTrackedBuildMetadata(&trending);
    applyTrackedBuildMetadata(&popular);
    applyTrackedBuildMetadata(&recent);
    applyTrackedBuildMetadata(&updated);

    m_cachedFlathubTrending = trending;
    m_cachedFlathubPopular = popular;
    m_cachedFlathubRecent = recent;
    m_cachedFlathubUpdated = updated;
    m_flathubCacheTimestampMs = QDateTime::currentMSecsSinceEpoch();
    m_flathubCollectionDay = todayUtcCollectionDay();
    m_flathubCacheSource = cacheSource;
    saveFlathubCache();

    emit storeCollectionsUpdated(normalizedRepoId, trending, popular, recent, updated);
    scheduleStoreIconEnrichment(normalizedRepoId, trending, popular, recent, updated);
    patchCollectionsFromCatalogIndex(normalizedRepoId);
}

void FlatpakBackend::fetchFlathubCollectionsViaApi(const QString &normalizedRepoId,
                                                   bool allowRemoteLsFallback,
                                                   bool forceRefresh)
{
    if (!m_discoveryCacheEmitted)
        emitCachedDiscoveryData();

    if (!forceRefresh && flathubCollectionsFreshForToday())
        return;

    m_flathubApiClient->fetchCollections([this, normalizedRepoId, allowRemoteLsFallback](
                                             const FlathubCollectionsResult &result) {
        if (!result.errorMessage.isEmpty()
            && result.trending.isEmpty() && result.popular.isEmpty()
            && result.recent.isEmpty() && result.updated.isEmpty()) {
            if (allowRemoteLsFallback) {
                qWarning() << "Flathub API collections failed, falling back to remote-ls:"
                           << result.errorMessage;
                fetchStoreSuggestionsViaRemoteLs(normalizedRepoId);
                return;
            }
            emit storeFetchFailed(normalizedRepoId, result.errorMessage);
            return;
        }

        QVector<AppInfo> trending = mergeAppListsById(result.trending, m_cachedFlathubTrending);
        QVector<AppInfo> popular = mergeAppListsById(result.popular, m_cachedFlathubPopular);
        QVector<AppInfo> recent = mergeAppListsById(result.recent, m_cachedFlathubRecent);
        QVector<AppInfo> updated = mergeAppListsById(result.updated, m_cachedFlathubUpdated);

        applyFlathubCollections(normalizedRepoId,
                                trending,
                                popular,
                                recent,
                                updated,
                                QStringLiteral("api"));
    });
}

void FlatpakBackend::fetchStoreSuggestionsViaRemoteLs(const QString &normalizedRepoId)
{
    if (!m_installations || !m_installations->isValid()) {
        emit storeFetchFailed(normalizedRepoId,
                              tr("Could not load apps from remote '%1'.").arg(normalizedRepoId));
        return;
    }

    auto *watcher = new QFutureWatcher<StoreDiscoveryResult>(this);
    connect(watcher,
            &QFutureWatcher<StoreDiscoveryResult>::finished,
            this,
            [this, watcher, normalizedRepoId]() {
                const StoreDiscoveryResult built = watcher->result();
                watcher->deleteLater();

                if (normalizedRepoId.compare(QStringLiteral("flathub"), Qt::CaseInsensitive) == 0) {
                    applyFlathubCollections(normalizedRepoId,
                                            built.trending,
                                            built.popular,
                                            built.recent,
                                            built.updated,
                                            QStringLiteral("remote-ls"));
                } else {
                    QVector<AppInfo> trending = built.trending;
                    QVector<AppInfo> popular = built.popular;
                    QVector<AppInfo> recent = built.recent;
                    QVector<AppInfo> updated = built.updated;
                    tagAppsWithRepo(trending, normalizedRepoId);
                    tagAppsWithRepo(popular, normalizedRepoId);
                    tagAppsWithRepo(recent, normalizedRepoId);
                    tagAppsWithRepo(updated, normalizedRepoId);
                    applyTrackedBuildMetadata(&trending);
                    applyTrackedBuildMetadata(&popular);
                    applyTrackedBuildMetadata(&recent);
                    applyTrackedBuildMetadata(&updated);
                    emit storeCollectionsUpdated(normalizedRepoId,
                                               trending,
                                               popular,
                                               recent,
                                               updated);
                    scheduleStoreIconEnrichment(normalizedRepoId,
                                                trending,
                                                popular,
                                                recent,
                                                updated);
                }
            });
    watcher->setFuture(QtConcurrent::run([installations = m_installations.get(), normalizedRepoId]() {
        return buildStoreDiscoveryResult(installations->listRemoteApps(normalizedRepoId), normalizedRepoId);
    }));
}

void FlatpakBackend::fetchStoreSuggestions(const QString &repoId, bool forceRefresh)
{
    const QString normalizedRepoId = repoId.trimmed().isEmpty()
            ? QStringLiteral("flathub")
            : repoId.trimmed();

    if (normalizedRepoId.compare(QStringLiteral("flathub"), Qt::CaseInsensitive) == 0) {
        fetchFlathubCollectionsViaApi(normalizedRepoId, true, forceRefresh);
        return;
    }

    fetchStoreSuggestionsViaRemoteLs(normalizedRepoId);
}

void FlatpakBackend::refreshCatalogIndex(const QString &repoId)
{
    const QString normalizedRepoId = repoId.trimmed().isEmpty()
            ? QStringLiteral("flathub")
            : repoId.trimmed();

    if (!m_installations || !m_installations->isValid())
        return;

    auto *watcher = new QFutureWatcher<QHash<QString, AppInfo>>(this);
    connect(watcher,
            &QFutureWatcher<QHash<QString, AppInfo>>::finished,
            this,
            [this, watcher, normalizedRepoId]() {
                m_catalogIndex = watcher->result();
                m_catalogIndexRepoId = normalizedRepoId;
                watcher->deleteLater();
                patchCollectionsFromCatalogIndex(normalizedRepoId);
            });
    watcher->setFuture(QtConcurrent::run([installations = m_installations.get(), normalizedRepoId]() {
        QHash<QString, AppInfo> index;
        const QVector<AppInfo> apps = installations->listRemoteApps(normalizedRepoId);
        for (AppInfo app : apps) {
            app.repoId = normalizedRepoId;
            if (!app.id.isEmpty())
                index.insert(app.id, app);
        }
        return index;
    }));
}

void FlatpakBackend::patchCollectionsFromCatalogIndex(const QString &repoId)
{
    if (m_catalogIndex.isEmpty() || m_catalogIndexRepoId != repoId)
        return;

    QVector<AppInfo> patches;
    const auto patchList = [this, &patches](QVector<AppInfo> &list) {
        for (AppInfo &app : list) {
            const auto it = m_catalogIndex.constFind(app.id);
            if (it == m_catalogIndex.constEnd())
                continue;
            const AppInfo before = app;
            mergeCatalogFields(&app, it.value());
            if (app.name != before.name || app.summary != before.summary
                || app.version != before.version) {
                patches.append(app);
            }
        }
    };

    if (repoId.compare(QStringLiteral("flathub"), Qt::CaseInsensitive) == 0) {
        patchList(m_cachedFlathubTrending);
        patchList(m_cachedFlathubPopular);
        patchList(m_cachedFlathubRecent);
        patchList(m_cachedFlathubUpdated);
        if (!patches.isEmpty())
            saveFlathubCache();
    }

    if (!patches.isEmpty())
        emit storeCatalogIndexReady(repoId, patches);
}

void FlatpakBackend::tagAppsWithRepo(QVector<AppInfo> &apps, const QString &repoId) const
{
    for (AppInfo &app : apps) {
        app.repoId = repoId;
        app.templateId = QStringLiteral("default");
        if (app.storePageUrl.isEmpty())
            app.storePageUrl = defaultStorePageUrlForApp(repoId, app.id);
    }
}

QString FlatpakBackend::defaultStorePageUrlForApp(const QString &repoId, const QString &appId)
{
    return FlatpakRemoteCatalog::catalogPageUrlForApp(repoId, appId);
}

void FlatpakBackend::listRemotes()
{
    if (!m_installations || !m_installations->isValid()) {
        emit remotesUpdated({});
        return;
    }

    auto *watcher = new QFutureWatcher<QVector<QPair<QString, QString>>>(this);
    connect(watcher, &QFutureWatcher<QVector<QPair<QString, QString>>>::finished, this, [this, watcher]() {
        const QVector<QPair<QString, QString>> remotes = watcher->result();
        for (const auto &remote : remotes) {
            if (!remote.second.isEmpty())
                m_remoteConfigUrls.insert(remote.first.toLower(), remote.second);
        }
        emit remotesUpdated(remotes);
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([installations = m_installations.get()]() {
        return installations->listRemotes();
    }));
}

void FlatpakBackend::addRemote(const QString &name, const QString &url)
{
    const QString trimmedName = name.trimmed();
    const QString trimmedUrl = url.trimmed();
    if (trimmedName.isEmpty() || trimmedUrl.isEmpty()) {
        emit remoteAddFinished(false, tr("Remote name and URL are required"));
        return;
    }

    if (!m_installations || !m_installations->isValid()) {
        emit remoteAddFinished(false, m_installations ? m_installations->lastError()
                                                      : tr("Flatpak backend unavailable"));
        return;
    }

    auto *watcher = new QFutureWatcher<QPair<bool, QString>>(this);
    connect(watcher, &QFutureWatcher<QPair<bool, QString>>::finished, this, [this, watcher, trimmedName, trimmedUrl]() {
        const QPair<bool, QString> result = watcher->result();
        watcher->deleteLater();
        if (result.first) {
            m_remoteConfigUrls.insert(trimmedName.toLower(), trimmedUrl);
            emit remoteAddFinished(true, tr("Remote added"));
            listRemotes();
            return;
        }
        emit remoteAddFinished(false,
                               result.second.isEmpty() ? tr("Failed to add remote") : result.second);
    });
    watcher->setFuture(QtConcurrent::run([installations = m_installations.get(), trimmedName, trimmedUrl]() {
        QString error;
        const bool ok = installations->addRemoteUser(trimmedName, trimmedUrl, &error);
        return qMakePair(ok, error);
    }));
}

void FlatpakBackend::removeRemote(const QString &name)
{
    const QString trimmedName = name.trimmed();
    if (trimmedName.isEmpty()) {
        emit remoteRemoveFinished(false, tr("Select a remote to remove"));
        return;
    }
    if (trimmedName.compare(QStringLiteral("flathub"), Qt::CaseInsensitive) == 0) {
        emit remoteRemoveFinished(false, tr("Flathub cannot be removed"));
        return;
    }

    if (!m_installations || !m_installations->isValid()) {
        emit remoteRemoveFinished(false, m_installations ? m_installations->lastError()
                                                         : tr("Flatpak backend unavailable"));
        return;
    }

    auto *watcher = new QFutureWatcher<bool>(this);
    connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher]() {
        const bool ok = watcher->result();
        watcher->deleteLater();
        if (ok) {
            emit remoteRemoveFinished(true, tr("Remote removed"));
            listRemotes();
            return;
        }
        emit remoteRemoveFinished(false, tr("Failed to remove remote"));
    });
    watcher->setFuture(QtConcurrent::run([installations = m_installations.get(), trimmedName]() {
        QString error;
        return installations->removeRemoteUser(trimmedName, &error);
    }));
}

void FlatpakBackend::install(const QString &appId, const QString &repoId)
{
    if (!m_transactionRunner) {
        Operation failed;
        failed.appId = appId;
        failed.type = OperationType::Install;
        failed.status = OperationStatus::Failed;
        failed.message = tr("Flatpak backend unavailable");
        emit operationStarted(failed);
        emit operationFinished(failed);
        return;
    }

    Operation op;
    op.appId = appId;
    op.type = OperationType::Install;
    op.status = OperationStatus::Running;
    op.message = tr("Installing…");
    emit operationStarted(op);
    m_transactionRunner->installFromRemote(op, repoId, FlatpakScope::User);
}

void FlatpakBackend::runFlatpakInstallForOperation(const Operation &op,
                                                   const QString &bundlePath,
                                                   const QString &tempDirToCleanup)
{
    const auto cleanupTemp = [this, tempDirToCleanup]() {
        if (!tempDirToCleanup.isEmpty())
            cleanupTrackedBuildTempDir(tempDirToCleanup);
    };

    if (!m_transactionRunner) {
        Operation failed = op;
        failed.status = OperationStatus::Failed;
        failed.message = tr("Flatpak backend unavailable");
        cleanupTemp();
        m_activeTrackedInstallAppIds.remove(op.appId);
        m_pendingTrackedInstalls.remove(op.appId);
        emit operationFinished(failed);
        return;
    }

    if (!QFileInfo::exists(bundlePath)) {
        Operation failed = op;
        failed.status = OperationStatus::Failed;
        failed.message = tr("Install bundle not found: %1").arg(bundlePath);
        cleanupTemp();
        m_activeTrackedInstallAppIds.remove(op.appId);
        m_pendingTrackedInstalls.remove(op.appId);
        emit operationFinished(failed);
        return;
    }

    const FlatpakScope scope = m_installations
            ? m_installations->scopeForAppId(op.appId)
            : FlatpakScope::User;

    prepareRuntimeRemoteForBundle(scope, bundlePath);

    const bool replaceExisting = op.type == OperationType::Update;
    m_transactionRunner->installBundle(
            op,
            bundlePath,
            scope,
            replaceExisting,
            [this, cleanupTemp, appId = op.appId]() {
                cleanupTemp();
                m_activeTrackedInstallAppIds.remove(appId);
            });
}

void FlatpakBackend::installBundle(const QString &bundlePath, const QString &tempDirToCleanup)
{
    Operation op;
    op.appId = QFileInfo(bundlePath).fileName();
    op.type = OperationType::Install;
    op.status = OperationStatus::Running;
    op.message = tr("Installing bundle…");
    emit operationStarted(op);
    runFlatpakInstallForOperation(op, bundlePath, tempDirToCleanup);
}

QString FlatpakBackend::installationScopeForApp(const QString &appId) const
{
    if (!m_installations)
        return QString();
    return flatpakScopeToString(m_installations->scopeForAppId(appId));
}

void FlatpakBackend::prepareRuntimeRemoteForBundle(FlatpakScope scope, const QString &bundlePath)
{
    if (!m_installations || !m_networkManager)
        return;

    const QString repoUrl = m_installations->bundleRuntimeRepoUrl(bundlePath);
    if (repoUrl.isEmpty())
        return;

    QString fetchError;
    const QByteArray repoData = fetchUrlSync(m_networkManager, repoUrl, &fetchError);
    if (repoData.isEmpty()) {
        qWarning() << "FlatpakBackend: failed to fetch runtime repo" << repoUrl << fetchError;
        return;
    }

    QString ensureError;
    if (!m_installations->ensureRuntimeRemoteForBundle(scope, bundlePath, repoData, &ensureError))
        qWarning() << "FlatpakBackend: failed to add runtime remote:" << ensureError;
}

void FlatpakBackend::cleanupTrackedBuildTempDir(const QString &dirPath) const
{
    if (dirPath.isEmpty())
        return;
    QDir dir(dirPath);
    if (dir.exists())
        dir.removeRecursively();
}

QString FlatpakBackend::extractFlatpakFromZip(const QString &zipPath,
                                              const TrackedBuildProject &project,
                                              QString *error) const
{
    const QFileInfo zipInfo(zipPath);
    const QString extractDir = zipInfo.absolutePath() + QLatin1Char('/')
            + zipInfo.completeBaseName() + QStringLiteral("-extract");
    cleanupTrackedBuildTempDir(extractDir);
    QDir().mkpath(extractDir);

    QProcess unzip;
    unzip.setProgram(QStringLiteral("unzip"));
    unzip.setArguments({QStringLiteral("-o"), zipPath, QStringLiteral("-d"), extractDir});
    unzip.start();
    if (!unzip.waitForStarted(5000) || !unzip.waitForFinished(120'000) || unzip.exitCode() != 0) {
        if (error)
            *error = tr("Failed to extract zip: %1").arg(QString::fromUtf8(unzip.readAllStandardError()));
        cleanupTrackedBuildTempDir(extractDir);
        return QString();
    }

    QRegularExpression filterRx(project.zippedFlatpakFilterRegex);
    QStringList flatpakPaths;
    QDirIterator it(extractDir, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        if (!path.endsWith(QStringLiteral(".flatpak"), Qt::CaseInsensitive))
            continue;
        const QString fileName = QFileInfo(path).fileName();
        if (!project.zippedFlatpakFilterRegex.isEmpty()) {
            if (!filterRx.isValid() || !filterRx.match(fileName).hasMatch())
                continue;
        }
        flatpakPaths.append(path);
    }

    if (flatpakPaths.isEmpty()) {
        if (error)
            *error = tr("No matching .flatpak found inside zip archive");
        cleanupTrackedBuildTempDir(extractDir);
        return QString();
    }

    if (project.autoFlatpakFilterByArch && flatpakPaths.size() > 1) {
        const QString arch = TrackedBuildClassifier::hostArchitectureToken();
        for (const QString &path : flatpakPaths) {
            if (QFileInfo(path).fileName().contains(arch, Qt::CaseInsensitive))
                return path;
        }
    }
    return flatpakPaths.first();
}

void FlatpakBackend::installTrackedBuildAsset(const TrackedBuildProject &project,
                                              const QString &assetUrl,
                                              const QString &linkedAppId,
                                              const QString &releaseTag)
{
    startTrackedBuildInstall(project, assetUrl, linkedAppId, releaseTag);
}

bool FlatpakBackend::startTrackedBuildInstall(const TrackedBuildProject &project,
                                              const QString &assetUrl,
                                              const QString &linkedAppId,
                                              const QString &releaseTag)
{
    const QString trimmedUrl = assetUrl.trimmed();
    if (trimmedUrl.isEmpty())
        return false;

    const QString appId = linkedAppId.trimmed().isEmpty() ? project.linkedAppId : linkedAppId;
    if (appId.isEmpty())
        return false;
    if (m_activeTrackedInstallAppIds.contains(appId)) {
        Operation op;
        op.appId = appId;
        op.type = OperationType::Update;
        op.status = OperationStatus::Failed;
        op.message = tr("An install for %1 is already in progress.").arg(appId);
        emit operationFinished(op);
        return false;
    }

    QString appliedTag = releaseTag.trimmed();
    if (appliedTag.isEmpty())
        appliedTag = inferReleaseTagForAsset(project, trimmedUrl);

    PendingTrackedInstall pending;
    pending.releaseTag = appliedTag;
    pending.assetUrl = trimmedUrl;
    m_pendingTrackedInstalls.insert(appId, pending);

    Operation op;
    op.appId = appId;
    op.type = OperationType::Update;
    op.status = OperationStatus::Running;
    op.message = tr("Downloading release…");
    m_activeTrackedInstallAppIds.insert(appId);
    emit operationStarted(op);

    const QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (cacheDir.isEmpty()) {
        m_activeTrackedInstallAppIds.remove(appId);
        m_pendingTrackedInstalls.remove(appId);
        Operation failed = op;
        failed.status = OperationStatus::Failed;
        failed.message = tr("No cache directory available for download");
        emit operationFinished(failed);
        return false;
    }
    const QString tempDir = cacheDir + QStringLiteral("/tracked-build-")
            + QUuid::createUuid().toString(QUuid::WithoutBraces);
    QDir().mkpath(tempDir);

    QNetworkRequest request{QUrl(trimmedUrl)};
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("curio-tracked-builds"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply *reply = m_networkManager->get(request);

    connect(reply, &QNetworkReply::downloadProgress, this, [this, op](qint64 received, qint64 total) mutable {
        Operation progress = op;
        if (total > 0) {
            progress.progress = static_cast<int>((received * 100) / total);
            progress.message = tr("Downloading release… %1%")
                    .arg(progress.progress);
        } else {
            progress.message = tr("Downloading release…");
        }
        emit operationProgress(progress);
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply, op, trimmedUrl, tempDir, project, appId]() {
        auto finishFailed = [this, tempDir, appId](Operation failedOp) {
            cleanupTrackedBuildTempDir(tempDir);
            m_activeTrackedInstallAppIds.remove(appId);
            m_pendingTrackedInstalls.remove(appId);
            emit operationFinished(failedOp);
        };

        if (reply->error() != QNetworkReply::NoError) {
            Operation failed = op;
            failed.status = OperationStatus::Failed;
            failed.message = tr("Download failed: %1").arg(reply->errorString());
            finishFailed(failed);
            reply->deleteLater();
            return;
        }

        const QByteArray payload = reply->readAll();
        reply->deleteLater();
        if (payload.isEmpty()) {
            Operation failed = op;
            failed.status = OperationStatus::Failed;
            failed.message = tr("Downloaded bundle was empty");
            finishFailed(failed);
            return;
        }

        const QString fileName = QUrl(trimmedUrl).fileName();
        const QString downloadPath = tempDir + QLatin1Char('/') + fileName;
        QSaveFile downloadFile(downloadPath);
        if (!downloadFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            Operation failed = op;
            failed.status = OperationStatus::Failed;
            failed.message = tr("Could not write downloaded file");
            finishFailed(failed);
            return;
        }
        if (downloadFile.write(payload) != payload.size() || !downloadFile.commit()) {
            Operation failed = op;
            failed.status = OperationStatus::Failed;
            failed.message = tr("Could not save downloaded file");
            finishFailed(failed);
            return;
        }

        if (!QFileInfo::exists(downloadPath) || QFileInfo(downloadPath).size() != payload.size()) {
            Operation failed = op;
            failed.status = OperationStatus::Failed;
            failed.message = tr("Downloaded file verification failed");
            finishFailed(failed);
            return;
        }

        QString bundlePath = downloadPath;
        if (downloadPath.endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive)) {
            Operation extractProgress = op;
            extractProgress.message = tr("Extracting release bundle…");
            emit operationProgress(extractProgress);
            QString extractError;
            bundlePath = extractFlatpakFromZip(downloadPath, project, &extractError);
            if (bundlePath.isEmpty() || !QFileInfo::exists(bundlePath)) {
                Operation failed = op;
                failed.status = OperationStatus::Failed;
                failed.message = extractError.isEmpty()
                        ? tr("Extracted bundle not found")
                        : extractError;
                finishFailed(failed);
                return;
            }
        }

        const QString scope = installationScopeForApp(appId);
        Operation installProgress = op;
        installProgress.message = scope == QStringLiteral("system")
                ? tr("Installing release (enter admin password if prompted)…")
                : tr("Installing release…");
        installProgress.progress = 0;
        emit operationProgress(installProgress);
        runFlatpakInstallForOperation(op, bundlePath, tempDir);
    });

    return true;
}

void FlatpakBackend::installReleaseAsset(const QString &assetUrl)
{
    TrackedBuildProject curioProject;
    curioProject.linkedAppId = QStringLiteral("io.github.curio.Curio");
    for (const TrackedBuildProject &project : m_trackedBuildProjects) {
        if (project.id == trackedBuildBuiltinCurioId()) {
            curioProject = project;
            break;
        }
    }
    installTrackedBuildAsset(curioProject, assetUrl, QStringLiteral("io.github.curio.Curio"));
}

void FlatpakBackend::uninstall(const QString &appId)
{
    if (!m_transactionRunner) {
        Operation op;
        op.appId = appId;
        op.type = OperationType::Uninstall;
        op.status = OperationStatus::Failed;
        op.message = tr("Flatpak backend unavailable");
        emit operationStarted(op);
        emit operationFinished(op);
        return;
    }

    Operation op;
    op.appId = appId;
    op.type = OperationType::Uninstall;
    op.status = OperationStatus::Running;
    op.message = tr("Uninstalling…");
    emit operationStarted(op);

    const FlatpakScope scope = flatpakScopeFromString(installationScopeForApp(appId));
    m_transactionRunner->uninstallRef(op, scope);
}

void FlatpakBackend::update(const QString &appId, const QString &installedVersion)
{
    if (tryInstallTrackedBuildUpdate(appId, installedVersion))
        return;

    if (!m_transactionRunner) {
        Operation op;
        op.appId = appId;
        op.type = OperationType::Update;
        op.status = OperationStatus::Failed;
        op.message = tr("Flatpak backend unavailable");
        emit operationStarted(op);
        emit operationFinished(op);
        return;
    }

    Operation op;
    op.appId = appId;
    op.type = OperationType::Update;
    op.status = OperationStatus::Running;
    op.message = tr("Updating…");
    emit operationStarted(op);

    const FlatpakScope scope = flatpakScopeFromString(installationScopeForApp(appId));
    m_transactionRunner->updateRef(op, scope);
}

bool FlatpakBackend::tryInstallTrackedBuildUpdate(const QString &appId,
                                                  const QString &installedVersion)
{
    const QString trimmedId = appId.trimmed();
    if (trimmedId.isEmpty() || installedVersion.trimmed().isEmpty())
        return false;

    for (const TrackedBuildProject &project : m_trackedBuildProjects) {
        if (project.linkedAppId.trimmed() != trimmedId)
            continue;
        QString releaseTag;
        QString assetUrl;
        if (!TrackedBuildClassifier::trackedReleaseUpdateTarget(installedVersion, project,
                                                                &releaseTag, &assetUrl)) {
            continue;
        }
        startTrackedBuildInstall(project, assetUrl, trimmedId, releaseTag);
        return true;
    }
    return false;
}

QString FlatpakBackend::cacheKeyForQuery(const QString &query) const
{
    return query.trimmed().toLower();
}

bool FlatpakBackend::tryGetCachedSearch(const QString &query, QVector<AppInfo> *apps) const
{
    if (!apps)
        return false;
    const QString key = cacheKeyForQuery(query);
    if (key.isEmpty())
        return false;
    if (!m_searchCache.contains(key) || !m_searchCacheTimestamps.contains(key))
        return false;

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 ts = m_searchCacheTimestamps.value(key);
    if (now - ts > m_cacheTtlMs)
        return false;

    *apps = m_searchCache.value(key);
    return true;
}

void FlatpakBackend::putCachedSearch(const QString &query, const QVector<AppInfo> &apps)
{
    const QString key = cacheKeyForQuery(query);
    if (key.isEmpty())
        return;
    m_searchCache.insert(key, apps);
    m_searchCacheTimestamps.insert(key, QDateTime::currentMSecsSinceEpoch());

    // Keep cache bounded by removing oldest entries.
    while (m_searchCache.size() > m_cacheMaxEntries) {
        QString oldestKey;
        qint64 oldestTs = std::numeric_limits<qint64>::max();
        for (auto it = m_searchCacheTimestamps.cbegin(); it != m_searchCacheTimestamps.cend(); ++it) {
            if (it.value() < oldestTs) {
                oldestTs = it.value();
                oldestKey = it.key();
            }
        }
        if (oldestKey.isEmpty())
            break;
        m_searchCache.remove(oldestKey);
        m_searchCacheTimestamps.remove(oldestKey);
    }
    saveSearchCache();
}

void FlatpakBackend::loadSearchCache()
{
    if (m_cachePath.isEmpty())
        return;

    QFile f(m_cachePath);
    if (!f.exists() || !f.open(QIODevice::ReadOnly))
        return;

    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject())
        return;
    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("schemaVersion")).toInt(1) < m_cacheSchemaVersion)
        return;

    const QJsonArray entries = root.value(QStringLiteral("entries")).toArray();
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (const QJsonValue &entryVal : entries) {
        const QJsonObject entryObj = entryVal.toObject();
        const QString key = entryObj.value(QStringLiteral("query")).toString();
        const qint64 ts = static_cast<qint64>(entryObj.value(QStringLiteral("ts")).toDouble());
        if (key.isEmpty() || ts <= 0 || (now - ts > m_cacheTtlMs))
            continue;

        QVector<AppInfo> apps;
        const QJsonArray appsArray = entryObj.value(QStringLiteral("apps")).toArray();
        apps = appsFromJsonArray(appsArray);

        m_searchCache.insert(key, apps);
        m_searchCacheTimestamps.insert(key, ts);
    }
}

void FlatpakBackend::saveSearchCache() const
{
    if (m_cachePath.isEmpty())
        return;

    QJsonArray entries;
    for (auto it = m_searchCache.cbegin(); it != m_searchCache.cend(); ++it) {
        const QString &key = it.key();
        const QVector<AppInfo> &apps = it.value();

        QJsonArray appsArray;
        appsArray = appsToJsonArray(apps);

        QJsonObject entry;
        entry.insert(QStringLiteral("query"), key);
        entry.insert(QStringLiteral("ts"), static_cast<double>(m_searchCacheTimestamps.value(key)));
        entry.insert(QStringLiteral("apps"), appsArray);
        entries.append(entry);
    }

    QJsonObject root;
    root.insert(QStringLiteral("schemaVersion"), m_cacheSchemaVersion);
    root.insert(QStringLiteral("entries"), entries);
    QFile f(m_cachePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    f.close();
}

void FlatpakBackend::loadFlathubCache()
{
    if (m_flathubCachePath.isEmpty())
        return;

    QFile f(m_flathubCachePath);
    if (!f.exists() || !f.open(QIODevice::ReadOnly))
        return;

    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject())
        return;
    const QJsonObject root = doc.object();
    const int schemaVersion = root.value(QStringLiteral("schemaVersion")).toInt(1);
    if (schemaVersion < 2)
        return;

    const qint64 ts = static_cast<qint64>(root.value(QStringLiteral("ts")).toDouble());
    if (ts <= 0)
        return;

    m_cachedFlathubTrending = appsFromJsonArray(root.value(QStringLiteral("trending")).toArray());
    m_cachedFlathubPopular = appsFromJsonArray(root.value(QStringLiteral("popular")).toArray());
    m_cachedFlathubRecent = appsFromJsonArray(root.value(QStringLiteral("recent")).toArray());
    m_cachedFlathubUpdated = appsFromJsonArray(root.value(QStringLiteral("updated")).toArray());
    m_flathubCacheTimestampMs = ts;
    m_flathubCollectionDay = root.value(QStringLiteral("collectionDay")).toString();
    m_flathubCacheSource = root.value(QStringLiteral("source")).toString();
}

void FlatpakBackend::saveFlathubCache() const
{
    if (m_flathubCachePath.isEmpty())
        return;

    QJsonObject root;
    root.insert(QStringLiteral("schemaVersion"), m_cacheSchemaVersion);
    root.insert(QStringLiteral("ts"), static_cast<double>(m_flathubCacheTimestampMs));
    root.insert(QStringLiteral("collectionDay"),
                m_flathubCollectionDay.isEmpty() ? todayUtcCollectionDay() : m_flathubCollectionDay);
    root.insert(QStringLiteral("source"),
                m_flathubCacheSource.isEmpty() ? QStringLiteral("api") : m_flathubCacheSource);
    root.insert(QStringLiteral("trending"), appsToJsonArray(m_cachedFlathubTrending));
    root.insert(QStringLiteral("popular"), appsToJsonArray(m_cachedFlathubPopular));
    root.insert(QStringLiteral("recent"), appsToJsonArray(m_cachedFlathubRecent));
    root.insert(QStringLiteral("updated"), appsToJsonArray(m_cachedFlathubUpdated));

    QFile f(m_flathubCachePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    f.close();
}

void FlatpakBackend::loadTrackedBuilds()
{
    m_trackedBuildProjects.clear();
    if (m_trackedBuildsPath.isEmpty())
        return;
    QFile file(m_trackedBuildsPath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly))
        return;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!doc.isObject())
        return;
    const QJsonObject root = doc.object();
    const QJsonArray arr = root.value(QStringLiteral("projects")).toArray();
    for (const QJsonValue &value : arr) {
        const QJsonObject obj = value.toObject();
        if (obj.isEmpty())
            continue;
        TrackedBuildProject project = trackedBuildFromJson(obj);
        if (project.id.isEmpty())
            project.id = QStringLiteral("%1:%2").arg(project.providerId, project.repoSlug);
        if (project.providerId.isEmpty() || project.repoSlug.isEmpty())
            continue;
        m_trackedBuildProjects.append(project);
    }
}

void FlatpakBackend::ensureBuiltInTrackedBuilds()
{
    const TrackedBuildProject defaults = defaultCurioTrackedBuildProject();

    for (TrackedBuildProject &existing : m_trackedBuildProjects) {
        if (existing.id != defaults.id)
            continue;
        if (trackedBuildBuiltinConfigValid(existing)
                && !trackedBuildEffectiveAssetFilterRegex(existing).isEmpty()) {
            return;
        }
        existing.providerId = defaults.providerId;
        existing.repoSlug = defaults.repoSlug;
        existing.linkedAppId = defaults.linkedAppId;
        existing.trackStable = defaults.trackStable;
        existing.trackNightly = defaults.trackNightly;
        existing.includePrereleases = defaults.includePrereleases;
        existing.assetFilterRegex = defaults.assetFilterRegex;
        existing.autoFlatpakFilterByArch = defaults.autoFlatpakFilterByArch;
        existing.latestStableVersion.clear();
        existing.latestStablePublishedAtIso.clear();
        existing.latestStableAssetUrl.clear();
        existing.latestNightlyVersion.clear();
        existing.latestNightlyPublishedAtIso.clear();
        existing.latestNightlyAssetUrl.clear();
        existing.unclassifiedTags.clear();
        existing.lastError.clear();
        saveTrackedBuilds();
        emit trackedBuildProjectsUpdated(m_trackedBuildProjects);
        return;
    }

    m_trackedBuildProjects.append(defaults);
    saveTrackedBuilds();
    emit trackedBuildProjectsUpdated(m_trackedBuildProjects);
}

void FlatpakBackend::saveTrackedBuilds() const
{
    if (m_trackedBuildsPath.isEmpty())
        return;
    QJsonObject root;
    root.insert(QStringLiteral("schemaVersion"), 1);
    QJsonArray projects;
    for (const TrackedBuildProject &project : m_trackedBuildProjects)
        projects.append(trackedBuildToJson(project));
    root.insert(QStringLiteral("projects"), projects);
    QFile file(m_trackedBuildsPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    file.close();
}

QVector<TrackedBuildProject> FlatpakBackend::trackedBuildProjects() const
{
    return m_trackedBuildProjects;
}

void FlatpakBackend::upsertTrackedBuildProject(const TrackedBuildProject &incoming)
{
    TrackedBuildProject project = incoming;
    project.providerId = project.providerId.trimmed().toLower();
    project.repoSlug = project.repoSlug.trimmed();
    if (project.id.trimmed().isEmpty())
        project.id = QStringLiteral("%1:%2").arg(project.providerId, project.repoSlug);
    if (project.providerId.isEmpty() || project.repoSlug.isEmpty())
        return;

    const bool isBuiltin = project.id == trackedBuildBuiltinCurioId();
    if (isBuiltin && !trackedBuildBuiltinConfigValid(project)) {
        const TrackedBuildProject defaults = defaultCurioTrackedBuildProject();
        project.providerId = defaults.providerId;
        project.repoSlug = defaults.repoSlug;
        project.linkedAppId = defaults.linkedAppId;
        project.assetFilterRegex = defaults.assetFilterRegex;
        project.autoFlatpakFilterByArch = defaults.autoFlatpakFilterByArch;
    }
    bool replaced = false;
    for (TrackedBuildProject &existing : m_trackedBuildProjects) {
        if (existing.id != project.id)
            continue;
        if (isBuiltin) {
            existing.latestStableVersion = project.latestStableVersion;
            existing.latestStablePublishedAtIso = project.latestStablePublishedAtIso;
            existing.latestStableAssetUrl = project.latestStableAssetUrl;
            existing.latestNightlyVersion = project.latestNightlyVersion;
            existing.latestNightlyPublishedAtIso = project.latestNightlyPublishedAtIso;
            existing.latestNightlyAssetUrl = project.latestNightlyAssetUrl;
            existing.unclassifiedTags = project.unclassifiedTags;
            existing.lastCheckedAtIso = project.lastCheckedAtIso;
            existing.lastError = project.lastError;
        } else {
            existing = project;
        }
        replaced = true;
        break;
    }
    if (!replaced && !isBuiltin)
        m_trackedBuildProjects.append(project);
    saveTrackedBuilds();
    emit trackedBuildProjectsUpdated(m_trackedBuildProjects);
}

void FlatpakBackend::removeTrackedBuildProject(const QString &projectId)
{
    if (projectId.trimmed().isEmpty() || projectId == trackedBuildBuiltinCurioId())
        return;
    for (int i = 0; i < m_trackedBuildProjects.size(); ++i) {
        if (m_trackedBuildProjects.at(i).id == projectId) {
            m_trackedBuildProjects.remove(i);
            saveTrackedBuilds();
            emit trackedBuildProjectsUpdated(m_trackedBuildProjects);
            return;
        }
    }
}

void FlatpakBackend::classifyAndStoreTrackedRelease(TrackedBuildProject *project,
                                                    const QVector<TrackedBuildRelease> &releases) const
{
    if (!project)
        return;
    TrackedBuildClassifier::classifyReleases(*project, releases, project);
}

void FlatpakBackend::refreshTrackedBuilds()
{
    if (m_trackedBuildProjects.isEmpty()) {
        emit trackedBuildProjectsUpdated(m_trackedBuildProjects);
        emit trackedBuildRefreshFinished();
        return;
    }
    if (m_trackedBuildRefreshInFlight) {
        m_trackedBuildRefreshReschedule = true;
        return;
    }
    beginTrackedBuildRefresh();
}

void FlatpakBackend::beginTrackedBuildRefresh()
{
    if (m_trackedBuildProjects.isEmpty()) {
        m_trackedBuildRefreshInFlight = false;
        emit trackedBuildProjectsUpdated(m_trackedBuildProjects);
        emit trackedBuildRefreshFinished();
        return;
    }
    if (!m_trackedBuildSource && m_networkManager)
        m_trackedBuildSource = new TrackedBuildSource(m_networkManager, this);
    if (!m_trackedBuildSource || !m_networkManager) {
        m_trackedBuildRefreshInFlight = false;
        emit trackedBuildRefreshFailed(tr("Network is not available"));
        emit trackedBuildRefreshFinished();
        return;
    }
    m_trackedBuildRefreshInFlight = true;
    ++m_trackedBuildRefreshGeneration;
    const int generation = m_trackedBuildRefreshGeneration;
    m_pendingTrackedBuildChecks = m_trackedBuildProjects.size();
    for (int i = 0; i < m_trackedBuildProjects.size(); ++i)
        refreshTrackedBuildsForIndex(i, generation);
}

void FlatpakBackend::completeTrackedBuildRefreshPass()
{
    saveTrackedBuilds();
    m_trackedBuildRefreshInFlight = false;
    if (m_trackedBuildRefreshReschedule) {
        m_trackedBuildRefreshReschedule = false;
        beginTrackedBuildRefresh();
        return;
    }
    emit trackedBuildRefreshFinished();
}

void FlatpakBackend::refreshTrackedBuildsForIndex(int index, int generation)
{
    if (generation != m_trackedBuildRefreshGeneration) {
        return;
    }
    if (index < 0 || index >= m_trackedBuildProjects.size()) {
        if (--m_pendingTrackedBuildChecks <= 0)
            completeTrackedBuildRefreshPass();
        return;
    }
    TrackedBuildProject project = m_trackedBuildProjects.at(index);
    m_trackedBuildSource->fetchReleases(project, [this, index, generation](const QVector<TrackedBuildRelease> &releases,
                                                               const QString &error) {
        if (generation != m_trackedBuildRefreshGeneration) {
            return;
        }
        if (index < 0 || index >= m_trackedBuildProjects.size()) {
            if (--m_pendingTrackedBuildChecks <= 0)
                completeTrackedBuildRefreshPass();
            return;
        }
        TrackedBuildProject &project = m_trackedBuildProjects[index];
        if (!error.isEmpty()) {
            project.lastError = error;
        } else {
            classifyAndStoreTrackedRelease(&project, releases);
            project.lastError.clear();
        }
        project.lastCheckedAtIso = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        emit trackedBuildProjectsUpdated(m_trackedBuildProjects);
        if (!project.lastError.isEmpty())
            emit trackedBuildRefreshFailed(QStringLiteral("%1: %2").arg(project.repoSlug, project.lastError));
        if (--m_pendingTrackedBuildChecks <= 0)
            completeTrackedBuildRefreshPass();
    });
}

void FlatpakBackend::applyTrackedBuildMetadata(QVector<AppInfo> *apps) const
{
    if (!apps || apps->isEmpty() || m_trackedBuildProjects.isEmpty())
        return;
    QHash<QString, TrackedBuildProject> byAppId;
    for (const TrackedBuildProject &project : m_trackedBuildProjects) {
        if (!project.linkedAppId.trimmed().isEmpty())
            byAppId.insert(project.linkedAppId.trimmed(), project);
    }
    for (AppInfo &app : *apps) {
        const auto it = byAppId.constFind(app.id);
        if (it == byAppId.cend())
            continue;
        const TrackedBuildProject &project = it.value();
        app.trackedBuildProviderId = project.providerId;
        app.trackedBuildRepoSlug = project.repoSlug;
        app.trackedStableVersion = project.latestStableVersion;
        app.trackedStablePublishedAtIso = project.latestStablePublishedAtIso;
        app.trackedStableAssetUrl = project.latestStableAssetUrl;
        app.trackedNightlyVersion = project.latestNightlyVersion;
        app.trackedNightlyPublishedAtIso = project.latestNightlyPublishedAtIso;
        app.trackedNightlyAssetUrl = project.latestNightlyAssetUrl;
        app.trackedBuildLastError = project.lastError;
    }
}

QString FlatpakBackend::inferReleaseTagForAsset(const TrackedBuildProject &project,
                                                const QString &assetUrl)
{
    const QString url = assetUrl.trimmed();
    if (url.isEmpty())
        return QString();
    if (url == project.latestStableAssetUrl.trimmed() && !project.latestStableVersion.isEmpty())
        return project.latestStableVersion;
    if (url == project.latestNightlyAssetUrl.trimmed() && !project.latestNightlyVersion.isEmpty())
        return project.latestNightlyVersion;
    return QString();
}

void FlatpakBackend::recordTrackedReleaseApplied(const QString &linkedAppId,
                                                 const QString &releaseTag,
                                                 const QString &assetUrl)
{
    const QString appId = linkedAppId.trimmed();
    const QString tag = releaseTag.trimmed();
    if (appId.isEmpty() || tag.isEmpty())
        return;

    bool changed = false;
    for (TrackedBuildProject &project : m_trackedBuildProjects) {
        if (project.linkedAppId.trimmed() != appId)
            continue;
        project.lastAppliedReleaseTag = tag;
        if (!assetUrl.trimmed().isEmpty())
            project.lastAppliedAssetUrl = assetUrl.trimmed();
        project.lastAppliedAtIso = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        changed = true;
        break;
    }
    if (changed) {
        saveTrackedBuilds();
        emit trackedBuildProjectsUpdated(m_trackedBuildProjects);
    }
}

void FlatpakBackend::finalizeTrackedInstall(const Operation &op)
{
    if (op.type != OperationType::Update && op.type != OperationType::Install)
        return;

    const auto pendingIt = m_pendingTrackedInstalls.constFind(op.appId);
    if (pendingIt == m_pendingTrackedInstalls.cend())
        return;

    const PendingTrackedInstall pending = pendingIt.value();
    m_pendingTrackedInstalls.remove(op.appId);

    QString releaseTag = pending.releaseTag;
    if (releaseTag.isEmpty()) {
        for (const TrackedBuildProject &project : m_trackedBuildProjects) {
            if (project.linkedAppId.trimmed() != op.appId.trimmed())
                continue;
            releaseTag = inferReleaseTagForAsset(project, pending.assetUrl);
            break;
        }
    }

    if (releaseTag.isEmpty())
        return;

    recordTrackedReleaseApplied(op.appId, releaseTag, pending.assetUrl);
}

void FlatpakBackend::syncTrackedApplyStateFromInstalled(const QVector<AppInfo> &apps)
{
    bool changed = false;
    for (TrackedBuildProject &project : m_trackedBuildProjects) {
        if (!project.lastAppliedReleaseTag.trimmed().isEmpty())
            continue;

        const QString linkedId = project.linkedAppId.trimmed();
        if (linkedId.isEmpty())
            continue;

        QString installedVersion;
        for (const AppInfo &app : apps) {
            if (app.id == linkedId) {
                installedVersion = app.version;
                break;
            }
        }
        if (installedVersion.trimmed().isEmpty())
            continue;

        QString channelTag;
        QString channelAssetUrl;
        if (!project.latestStableVersion.isEmpty()) {
            channelTag = project.latestStableVersion;
            channelAssetUrl = project.latestStableAssetUrl;
        } else {
            continue;
        }

        if (TrackedBuildClassifier::isReleaseNewerThanInstalled(installedVersion, channelTag))
            continue;

        project.lastAppliedReleaseTag = channelTag;
        if (!channelAssetUrl.isEmpty())
            project.lastAppliedAssetUrl = channelAssetUrl;
        project.lastAppliedAtIso = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        changed = true;
    }

    if (changed) {
        saveTrackedBuilds();
        emit trackedBuildProjectsUpdated(m_trackedBuildProjects);
    }
}

void FlatpakBackend::scheduleInstalledMetadataEnrichment(const QVector<AppInfo> &apps)
{
    if (apps.isEmpty() || !m_appStreamProvider || !m_appStreamProvider->isAvailable())
        return;

    m_installedEnrichQueue = apps;
    m_installedEnrichIndex = 0;

    if (!m_installedEnrichTimer) {
        m_installedEnrichTimer = new QTimer(this);
        m_installedEnrichTimer->setSingleShot(false);
        m_installedEnrichTimer->setInterval(50);
        connect(m_installedEnrichTimer, &QTimer::timeout, this, [this]() {
            processInstalledMetadataEnrichmentBatch();
        });
    }
    if (!m_installedEnrichTimer->isActive())
        m_installedEnrichTimer->start();
}

void FlatpakBackend::processInstalledMetadataEnrichmentBatch()
{
    constexpr int kBatchSize = 3;
    QVector<AppInfo> enrichedBatch;
    enrichedBatch.reserve(kBatchSize);

    while (m_installedEnrichIndex < m_installedEnrichQueue.size()
           && enrichedBatch.size() < kBatchSize) {
        AppInfo app = m_installedEnrichQueue.at(m_installedEnrichIndex++);
        const QString nameBefore = app.name;
        const QString summaryBefore = app.summary;
        enrichAppMetadata(app);
        const bool nameImproved = app.name != nameBefore
                && !app.name.isEmpty() && app.name != app.id;
        const bool summaryImproved = !app.summary.isEmpty() && app.summary != summaryBefore;
        if (nameImproved || summaryImproved || !app.vcsUrl.isEmpty() || !app.homepageUrl.isEmpty()
                || !app.iconUrl.isEmpty()) {
            enrichedBatch.append(app);
        }
    }

    if (!enrichedBatch.isEmpty())
        emit installedAppsMetadataEnriched(enrichedBatch);

    if (m_installedEnrichIndex >= m_installedEnrichQueue.size() && m_installedEnrichTimer)
        m_installedEnrichTimer->stop();
}

