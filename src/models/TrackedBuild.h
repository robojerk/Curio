#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

inline QString trackedBuildBuiltinCurioId()
{
    return QStringLiteral("github:robojerk/Curio");
}

inline QString trackedBuildBuiltinRepoSlug()
{
    return QStringLiteral("robojerk/Curio");
}

inline QString trackedBuildBuiltinLinkedAppId()
{
    return QStringLiteral("io.github.curio.Curio");
}

struct TrackedBuildAsset
{
    QString name;
    QString url;
    QString updatedAtIso;
};

struct TrackedBuildRelease
{
    QString releaseId;
    QString tagName;
    QString title;
    QString publishedAtIso;
    bool isPrerelease = false;
    bool isDraft = false;
    QVector<TrackedBuildAsset> assets;
};

struct TrackedBuildProject
{
    QString id;
    QString providerId = QStringLiteral("github");
    QString repoSlug;
    QString linkedAppId;

    bool trackStable = true;
    bool trackNightly = false;

    /** When false (default), follow newest GitHub **release** (skip API pre-releases). When true, newest release or pre-release. */
    bool includePrereleases = false;

    QString assetFilterRegex;
    QString releaseTitleFilterRegex;
    QString excludeRegex;

    // Legacy JSON fields (read-only migration; not written on save when new fields are set)
    QString stableTagRegex;
    QString nightlyTagRegex;
    QString stableAssetRegex;
    QString nightlyAssetRegex;

    bool includeZipAssets = false;
    QString zippedFlatpakFilterRegex;
    bool autoFlatpakFilterByArch = true;

    QString latestStableVersion;
    QString latestStablePublishedAtIso;
    QString latestStableAssetUrl;

    QString latestNightlyVersion;
    QString latestNightlyPublishedAtIso;
    QString latestNightlyAssetUrl;

    QStringList unclassifiedTags;
    QString lastCheckedAtIso;
    QString lastError;

    /** Last tracked release successfully installed via Curio (Git tag, e.g. v2.3.2). */
    QString lastAppliedReleaseTag;
    QString lastAppliedAssetUrl;
    QString lastAppliedAtIso;

    bool isBuiltIn() const { return id == trackedBuildBuiltinCurioId(); }
};

inline QString trackedBuildEffectiveAssetFilterRegex(const TrackedBuildProject &project)
{
    const QString unified = project.assetFilterRegex.trimmed();
    if (!unified.isEmpty())
        return unified;
    const QString stable = project.stableAssetRegex.trimmed();
    if (!stable.isEmpty())
        return stable;
    return project.nightlyAssetRegex.trimmed();
}

inline QString trackedBuildEffectiveReleaseTitleFilterRegex(const TrackedBuildProject &project)
{
    const QString unified = project.releaseTitleFilterRegex.trimmed();
    if (!unified.isEmpty())
        return unified;
    return project.stableTagRegex.trimmed();
}

inline bool trackedBuildBuiltinConfigValid(const TrackedBuildProject &project)
{
    return project.id == trackedBuildBuiltinCurioId()
            && project.providerId == QStringLiteral("github")
            && project.repoSlug == trackedBuildBuiltinRepoSlug()
            && project.linkedAppId == trackedBuildBuiltinLinkedAppId();
}

struct TrackedBuildSuggestions
{
    QString linkedAppId;
    QString assetFilterRegex;
    QString zippedFlatpakFilterRegex;
    bool includeZipAssets = false;
};
