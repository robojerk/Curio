#pragma once

#include "models/TrackedBuild.h"

#include <QVector>

class TrackedBuildClassifier
{
public:
    static QString hostArchitectureToken();
    static int compareReleaseTags(const QString &a, const QString &b);
    static bool isReleaseNewerThanInstalled(const QString &installedVersion, const QString &releaseTag);
    static bool hasTrackedReleaseUpdate(const QString &installedVersion,
                                        const TrackedBuildProject &project);
    static bool trackedReleaseUpdateTarget(const QString &installedVersion,
                                           const TrackedBuildProject &project,
                                           QString *releaseTag,
                                           QString *assetUrl);
    static bool assetIsInstallable(const TrackedBuildAsset &asset, bool includeZipAssets);
    static QString selectAssetUrl(const TrackedBuildProject &project,
                                const QVector<TrackedBuildAsset> &assets);
    static QString channelLabelForRelease(const TrackedBuildProject &config,
                                          const TrackedBuildRelease &release);
    static void classifyReleases(const TrackedBuildProject &config,
                                 const QVector<TrackedBuildRelease> &releases,
                                 TrackedBuildProject *out);
    /** Latest installable asset per current channel settings (stable preferred over pre-release). */
    static bool pickLatestInstallTarget(const TrackedBuildProject &config,
                                        const QVector<TrackedBuildRelease> &releases,
                                        QString *releaseTag,
                                        QString *assetUrl,
                                        QString *publishedAtIso = nullptr,
                                        bool *isPrerelease = nullptr);
    static bool pickLatestStableRelease(const TrackedBuildProject &config,
                                        const QVector<TrackedBuildRelease> &releases,
                                        QString *releaseTag,
                                        QString *assetUrl,
                                        QString *publishedAtIso = nullptr);
    static bool latestTrackedInstallTarget(const TrackedBuildProject &config,
                                           const QVector<TrackedBuildRelease> &releases,
                                           QString *releaseTag,
                                           QString *assetUrl);
    static TrackedBuildSuggestions suggestFromReleases(const QVector<TrackedBuildRelease> &releases);
};
