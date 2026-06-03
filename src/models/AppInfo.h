#pragma once

#include <QString>
#include <QStringList>

struct AppInfo
{
    // Source metadata allows the UI to render repository-specific templates.
    QString repoId = QStringLiteral("flathub");
    QString templateId = QStringLiteral("default");
    QString storePageUrl;

    QString id;
    QString name;
    QString summary;
    QString version;
    QString iconName;
    QString installationScope;
    bool installed = false;

    // Optional; filled from AppStream when available
    QString longDescription;
    QStringList screenshotUrls;
    QString iconUrl;
    QString developerName;
    QString projectLicense;
    QString homepageUrl;
    QString vcsUrl;
    QString bugtrackerUrl;
    /** flatpak remote config URL (from flatpak remotes), when not a catalog remote. */
    QString remoteRepoUrl;
    QString donateUrl;
    QString helpUrl;
    QString translateUrl;
    QStringList categories;

    // Optional tracked build metadata (Git-hosted release feeds)
    QString trackedBuildProviderId;
    QString trackedBuildRepoSlug;
    QString trackedStableVersion;
    QString trackedStablePublishedAtIso;
    QString trackedStableAssetUrl;
    QString trackedNightlyVersion;
    QString trackedNightlyPublishedAtIso;
    QString trackedNightlyAssetUrl;
    QString trackedBuildLastError;
};

