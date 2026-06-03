#include "TrackedBuildClassifier.h"

#include <QRegularExpression>
#include <QSysInfo>
#include <algorithm>

namespace {

struct ParsedReleaseTag
{
    int major = 0;
    int minor = 0;
    int patch = 0;
    QString prerelease;
};

bool parseReleaseTag(const QString &tag, ParsedReleaseTag *out)
{
    if (!out)
        return false;
    static const QRegularExpression rx(
        QStringLiteral("^(?:v|V)?(\\d+)\\.(\\d+)\\.(\\d+)(?:[-.]?(.*))?$"));
    const QRegularExpressionMatch match = rx.match(tag.trimmed());
    if (!match.hasMatch())
        return false;
    out->major = match.captured(1).toInt();
    out->minor = match.captured(2).toInt();
    out->patch = match.captured(3).toInt();
    out->prerelease = match.captured(4).trimmed();
    return true;
}

int compareParsedTags(const ParsedReleaseTag &a, const ParsedReleaseTag &b)
{
    if (a.major != b.major)
        return a.major < b.major ? -1 : 1;
    if (a.minor != b.minor)
        return a.minor < b.minor ? -1 : 1;
    if (a.patch != b.patch)
        return a.patch < b.patch ? -1 : 1;
    if (a.prerelease.isEmpty() && b.prerelease.isEmpty())
        return 0;
    if (a.prerelease.isEmpty())
        return 1;
    if (b.prerelease.isEmpty())
        return -1;
    return QString::compare(a.prerelease, b.prerelease, Qt::CaseInsensitive);
}

bool releasePassesTitleFilter(const TrackedBuildProject &config, const TrackedBuildRelease &release)
{
    const QString pattern = trackedBuildEffectiveReleaseTitleFilterRegex(config);
    if (pattern.isEmpty())
        return true;
    const QRegularExpression rx(pattern);
    if (!rx.isValid())
        return true;
    const QString nameToFilter = release.tagName.trimmed().isEmpty() ? release.title.trimmed()
                                                                     : release.tagName.trimmed();
    return rx.match(nameToFilter).hasMatch();
}

bool releasePassesExclude(const TrackedBuildProject &config, const TrackedBuildRelease &release)
{
    if (config.excludeRegex.trimmed().isEmpty())
        return false;
    const QRegularExpression excludeRx(config.excludeRegex);
    if (!excludeRx.isValid())
        return false;
    return excludeRx.match(release.tagName).hasMatch() || excludeRx.match(release.title).hasMatch();
}

bool releaseHasInstallableAsset(const TrackedBuildProject &config, const TrackedBuildRelease &release)
{
    for (const TrackedBuildAsset &asset : release.assets) {
        if (TrackedBuildClassifier::assetIsInstallable(asset, config.includeZipAssets))
            return true;
    }
    return false;
}

bool releasePassesCommonFilters(const TrackedBuildProject &config, const TrackedBuildRelease &release)
{
    if (release.isDraft)
        return false;
    if (releasePassesExclude(config, release))
        return false;
    if (!releaseHasInstallableAsset(config, release))
        return false;
    if (!releasePassesTitleFilter(config, release))
        return false;
    return !TrackedBuildClassifier::selectAssetUrl(config, release.assets).isEmpty();
}

QVector<TrackedBuildRelease> releasesNewestFirst(const QVector<TrackedBuildRelease> &releases)
{
    QVector<TrackedBuildRelease> sorted = releases;
    std::sort(sorted.begin(), sorted.end(), [](const TrackedBuildRelease &a, const TrackedBuildRelease &b) {
        return a.publishedAtIso > b.publishedAtIso;
    });
    return sorted;
}

} // namespace

QString TrackedBuildClassifier::hostArchitectureToken()
{
    const QString arch = QSysInfo::currentCpuArchitecture().toLower();
    if (arch.contains(QStringLiteral("aarch64")) || arch.contains(QStringLiteral("arm64")))
        return QStringLiteral("aarch64");
    if (arch.contains(QStringLiteral("x86_64")) || arch.contains(QStringLiteral("amd64")))
        return QStringLiteral("x86_64");
    return arch;
}

int TrackedBuildClassifier::compareReleaseTags(const QString &a, const QString &b)
{
    ParsedReleaseTag parsedA;
    ParsedReleaseTag parsedB;
    const bool hasA = parseReleaseTag(a, &parsedA);
    const bool hasB = parseReleaseTag(b, &parsedB);
    if (hasA && hasB)
        return compareParsedTags(parsedA, parsedB);
    if (hasA != hasB)
        return hasA ? 1 : -1;
    return QString::compare(a.trimmed(), b.trimmed(), Qt::CaseInsensitive);
}

bool TrackedBuildClassifier::isReleaseNewerThanInstalled(const QString &installedVersion,
                                                         const QString &releaseTag)
{
    const QString installed = installedVersion.trimmed();
    const QString release = releaseTag.trimmed();
    if (installed.isEmpty() || release.isEmpty())
        return false;
    if (QString::compare(installed, release, Qt::CaseInsensitive) == 0)
        return false;

    ParsedReleaseTag parsedInstalled;
    ParsedReleaseTag parsedRelease;
    const bool hasInstalled = parseReleaseTag(installed, &parsedInstalled);
    const bool hasRelease = parseReleaseTag(release, &parsedRelease);
    if (hasInstalled && hasRelease && parsedInstalled.major == parsedRelease.major
            && parsedInstalled.minor == parsedRelease.minor
            && parsedInstalled.patch == parsedRelease.patch) {
        if (parsedInstalled.prerelease.isEmpty() && parsedRelease.prerelease.isEmpty())
            return false;
        if (!parsedInstalled.prerelease.isEmpty() && parsedRelease.prerelease.isEmpty())
            return false;
        if (!parsedInstalled.prerelease.isEmpty() && !parsedRelease.prerelease.isEmpty())
            return compareParsedTags(parsedRelease, parsedInstalled) > 0;
        return false;
    }

    return compareReleaseTags(release, installed) > 0;
}

bool TrackedBuildClassifier::hasTrackedReleaseUpdate(const QString &installedVersion,
                                                     const TrackedBuildProject &project)
{
    QString tag;
    QString url;
    return trackedReleaseUpdateTarget(installedVersion, project, &tag, &url);
}

bool TrackedBuildClassifier::trackedReleaseUpdateTarget(const QString &installedVersion,
                                                        const TrackedBuildProject &project,
                                                        QString *releaseTag,
                                                        QString *assetUrl)
{
    if (releaseTag)
        releaseTag->clear();
    if (assetUrl)
        assetUrl->clear();

    const QString tag = project.latestStableVersion.trimmed();
    const QString url = project.latestStableAssetUrl.trimmed();
    if (url.isEmpty() || tag.isEmpty())
        return false;
    if (!isReleaseNewerThanInstalled(installedVersion, tag))
        return false;
    if (!project.lastAppliedReleaseTag.trimmed().isEmpty()
            && compareReleaseTags(tag, project.lastAppliedReleaseTag) <= 0) {
        return false;
    }
    if (releaseTag)
        *releaseTag = tag;
    if (assetUrl)
        *assetUrl = url;
    return true;
}

bool TrackedBuildClassifier::assetIsInstallable(const TrackedBuildAsset &asset, bool includeZipAssets)
{
    const QString lowerName = asset.name.toLower();
    if (lowerName.endsWith(QStringLiteral(".flatpak")))
        return true;
    return includeZipAssets && lowerName.endsWith(QStringLiteral(".zip"));
}

QString TrackedBuildClassifier::selectAssetUrl(const TrackedBuildProject &project,
                                               const QVector<TrackedBuildAsset> &assets)
{
    QVector<TrackedBuildAsset> candidates;
    candidates.reserve(assets.size());
    for (const TrackedBuildAsset &asset : assets) {
        if (!assetIsInstallable(asset, project.includeZipAssets))
            continue;
        candidates.append(asset);
    }
    if (candidates.isEmpty())
        return QString();

    const QString filterPattern = trackedBuildEffectiveAssetFilterRegex(project);
    const QRegularExpression assetRx(filterPattern);
    const QString archToken = hostArchitectureToken();

    auto scoreAsset = [&](const TrackedBuildAsset &asset) -> int {
        int score = 0;
        const QString name = asset.name;
        if (!filterPattern.isEmpty() && assetRx.isValid() && assetRx.match(name).hasMatch())
            score += 100;
        if (project.autoFlatpakFilterByArch && !archToken.isEmpty()) {
            const QString lower = name.toLower();
            if (lower.contains(archToken))
                score += 50;
            else if (archToken == QStringLiteral("x86_64")
                     && (lower.contains(QStringLiteral("amd64")) || lower.contains(QStringLiteral("x86-64"))))
                score += 40;
            else if (archToken == QStringLiteral("aarch64")
                     && lower.contains(QStringLiteral("arm64")))
                score += 40;
            else if (lower.contains(QStringLiteral("x86_64")) || lower.contains(QStringLiteral("aarch64"))
                     || lower.contains(QStringLiteral("arm64")))
                score -= 20;
        }
        if (name.endsWith(QStringLiteral(".flatpak"), Qt::CaseInsensitive))
            score += 10;
        return score;
    };

    const TrackedBuildAsset *best = &candidates.first();
    int bestScore = scoreAsset(candidates.first());
    for (int i = 1; i < candidates.size(); ++i) {
        const int score = scoreAsset(candidates.at(i));
        if (score > bestScore) {
            bestScore = score;
            best = &candidates.at(i);
        }
    }
    return best->url;
}

QString TrackedBuildClassifier::channelLabelForRelease(const TrackedBuildProject &config,
                                                       const TrackedBuildRelease &release)
{
    if (release.isDraft)
        return QStringLiteral("draft");
    if (releasePassesExclude(config, release))
        return QStringLiteral("skip");
    if (!releaseHasInstallableAsset(config, release))
        return QStringLiteral("skip");
    if (!releasePassesTitleFilter(config, release))
        return QStringLiteral("skip");
    if (selectAssetUrl(config, release.assets).isEmpty())
        return QStringLiteral("skip");
    if (release.isPrerelease)
        return config.includePrereleases ? QStringLiteral("pre-release") : QStringLiteral("skip");
    return QStringLiteral("release");
}

bool TrackedBuildClassifier::pickLatestInstallTarget(const TrackedBuildProject &config,
                                                       const QVector<TrackedBuildRelease> &releases,
                                                       QString *releaseTag,
                                                       QString *assetUrl,
                                                       QString *publishedAtIso,
                                                       bool *isPrerelease)
{
    if (releaseTag)
        releaseTag->clear();
    if (assetUrl)
        assetUrl->clear();
    if (publishedAtIso)
        publishedAtIso->clear();
    if (isPrerelease)
        *isPrerelease = false;

    for (const TrackedBuildRelease &release : releasesNewestFirst(releases)) {
        if (!releasePassesCommonFilters(config, release))
            continue;
        if (release.isPrerelease && !config.includePrereleases)
            continue;

        const QString tag = release.tagName.isEmpty() ? release.title : release.tagName;
        const QString url = selectAssetUrl(config, release.assets);
        if (tag.trimmed().isEmpty() || url.isEmpty())
            continue;

        if (releaseTag)
            *releaseTag = tag;
        if (assetUrl)
            *assetUrl = url;
        if (publishedAtIso)
            *publishedAtIso = release.publishedAtIso;
        if (isPrerelease)
            *isPrerelease = release.isPrerelease;
        return true;
    }
    return false;
}

bool TrackedBuildClassifier::pickLatestStableRelease(const TrackedBuildProject &config,
                                                       const QVector<TrackedBuildRelease> &releases,
                                                       QString *releaseTag,
                                                       QString *assetUrl,
                                                       QString *publishedAtIso)
{
    if (releaseTag)
        releaseTag->clear();
    if (assetUrl)
        assetUrl->clear();
    if (publishedAtIso)
        publishedAtIso->clear();

    for (const TrackedBuildRelease &release : releasesNewestFirst(releases)) {
        if (release.isPrerelease)
            continue;
        if (!releasePassesCommonFilters(config, release))
            continue;

        const QString tag = release.tagName.isEmpty() ? release.title : release.tagName;
        const QString url = selectAssetUrl(config, release.assets);
        if (tag.trimmed().isEmpty() || url.isEmpty())
            continue;

        if (releaseTag)
            *releaseTag = tag;
        if (assetUrl)
            *assetUrl = url;
        if (publishedAtIso)
            *publishedAtIso = release.publishedAtIso;
        return true;
    }
    return false;
}

void TrackedBuildClassifier::classifyReleases(const TrackedBuildProject &config,
                                              const QVector<TrackedBuildRelease> &releases,
                                              TrackedBuildProject *out)
{
    if (!out)
        return;

    out->latestStableVersion.clear();
    out->latestStablePublishedAtIso.clear();
    out->latestStableAssetUrl.clear();
    out->latestNightlyVersion.clear();
    out->latestNightlyPublishedAtIso.clear();
    out->latestNightlyAssetUrl.clear();
    out->unclassifiedTags.clear();

    bool installIsPrerelease = false;
    pickLatestInstallTarget(config,
                            releases,
                            &out->latestStableVersion,
                            &out->latestStableAssetUrl,
                            &out->latestStablePublishedAtIso,
                            &installIsPrerelease);

    if (config.includePrereleases && installIsPrerelease) {
        pickLatestStableRelease(config,
                                releases,
                                &out->latestNightlyVersion,
                                &out->latestNightlyAssetUrl,
                                &out->latestNightlyPublishedAtIso);
    }
}

bool TrackedBuildClassifier::latestTrackedInstallTarget(const TrackedBuildProject &config,
                                                        const QVector<TrackedBuildRelease> &releases,
                                                        QString *releaseTag,
                                                        QString *assetUrl)
{
    return pickLatestInstallTarget(config, releases, releaseTag, assetUrl);
}

TrackedBuildSuggestions TrackedBuildClassifier::suggestFromReleases(
    const QVector<TrackedBuildRelease> &releases)
{
    TrackedBuildSuggestions suggestions;
    const QString arch = hostArchitectureToken();
    suggestions.assetFilterRegex = QStringLiteral(R"(.*%1.*\.flatpak$)").arg(
        QRegularExpression::escape(arch));
    suggestions.zippedFlatpakFilterRegex = QStringLiteral(R"(.*\.flatpak$)");

    static const QRegularExpression appIdFromFlatpak(
        QStringLiteral("^(.+)\\.(x86_64|aarch64|arm64)\\.flatpak$"),
        QRegularExpression::CaseInsensitiveOption);

    for (const TrackedBuildRelease &release : releases) {
        if (release.isDraft)
            continue;
        for (const TrackedBuildAsset &asset : release.assets) {
            const QRegularExpressionMatch match = appIdFromFlatpak.match(asset.name);
            if (match.hasMatch()) {
                suggestions.linkedAppId = match.captured(1);
                if (asset.name.endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive))
                    suggestions.includeZipAssets = true;
                return suggestions;
            }
            if (asset.name.endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive))
                suggestions.includeZipAssets = true;
        }
    }
    return suggestions;
}
