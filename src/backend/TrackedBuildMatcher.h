#pragma once

#include "GitHost.h"
#include "models/AppInfo.h"
#include "models/TrackedBuild.h"

#include <QVector>

struct TrackedBuildSetupHints
{
    QString bannerText;
    QString prefilledUrl;
    QString prefilledLinkedAppId;
};

class TrackedBuildMatcher
{
public:
    static QString gitUrlFromApp(const AppInfo &app);
    static std::optional<ParsedGitRepo> parsedGitRepoFromApp(const AppInfo &app);

    static const TrackedBuildProject *findProjectByRepo(const QVector<TrackedBuildProject> &projects,
                                                        const ParsedGitRepo &repo);
    static QVector<AppInfo> findInstalledAppsForRepo(const QVector<AppInfo> &apps,
                                                     const ParsedGitRepo &repo);

    static bool isEligibleForTrackLink(const AppInfo &app,
                                       const QVector<TrackedBuildProject> &projects);
    static QString trackLinkLabelForApp(const AppInfo &app);
    static QString providerDisplayName(const QString &providerId);
};
