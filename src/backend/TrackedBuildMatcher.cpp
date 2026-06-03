#include "TrackedBuildMatcher.h"

#include "FlatpakRemoteCatalog.h"

#include <QObject>

QString TrackedBuildMatcher::gitUrlFromApp(const AppInfo &app)
{
    if (!app.vcsUrl.trimmed().isEmpty()) {
        if (GitHost::parseUrl(app.vcsUrl))
            return app.vcsUrl.trimmed();
    }
    if (!app.homepageUrl.trimmed().isEmpty()) {
        if (GitHost::parseUrl(app.homepageUrl))
            return app.homepageUrl.trimmed();
    }
    return QString();
}

std::optional<ParsedGitRepo> TrackedBuildMatcher::parsedGitRepoFromApp(const AppInfo &app)
{
    const QString url = gitUrlFromApp(app);
    if (url.isEmpty())
        return std::nullopt;
    return GitHost::parseUrl(url);
}

const TrackedBuildProject *TrackedBuildMatcher::findProjectByRepo(
    const QVector<TrackedBuildProject> &projects,
    const ParsedGitRepo &repo)
{
    const QString id = GitHost::projectId(repo);
    for (const TrackedBuildProject &project : projects) {
        if (project.id == id)
            return &project;
    }
    return nullptr;
}

QVector<AppInfo> TrackedBuildMatcher::findInstalledAppsForRepo(const QVector<AppInfo> &apps,
                                                               const ParsedGitRepo &repo)
{
    QVector<AppInfo> matches;
    for (const AppInfo &app : apps) {
        const std::optional<ParsedGitRepo> parsed = parsedGitRepoFromApp(app);
        if (!parsed)
            continue;
        if (GitHost::sameRepository(*parsed, repo))
            matches.append(app);
    }
    return matches;
}

bool TrackedBuildMatcher::isEligibleForTrackLink(const AppInfo &app,
                                                 const QVector<TrackedBuildProject> &projects)
{
    if (app.id.isEmpty() || !app.installed)
        return false;
    if (!app.trackedBuildProviderId.trimmed().isEmpty())
        return false;
    if (FlatpakRemoteCatalog::isCatalogOrigin(app.repoId))
        return false;

    const std::optional<ParsedGitRepo> parsed = parsedGitRepoFromApp(app);
    if (!parsed)
        return false;

    const TrackedBuildProject *existing = findProjectByRepo(projects, *parsed);
    if (existing && !existing->linkedAppId.trimmed().isEmpty()
            && existing->linkedAppId != app.id) {
        return false;
    }
    return true;
}

QString TrackedBuildMatcher::providerDisplayName(const QString &providerId)
{
    const QString provider = providerId.trimmed().toLower();
    if (provider == QStringLiteral("github"))
        return QStringLiteral("GitHub");
    if (provider == QStringLiteral("gitlab"))
        return QStringLiteral("GitLab");
    if (provider == QStringLiteral("codeberg"))
        return QStringLiteral("Codeberg");
    if (provider.isEmpty())
        return QString();
    return provider.at(0).toUpper() + provider.mid(1);
}

QString TrackedBuildMatcher::trackLinkLabelForApp(const AppInfo &app)
{
    const std::optional<ParsedGitRepo> parsed = parsedGitRepoFromApp(app);
    if (!parsed)
        return QString();
    const QString name = providerDisplayName(parsed->providerId());
    if (name.isEmpty())
        return QString();
    return QObject::tr("Track on %1").arg(name);
}
