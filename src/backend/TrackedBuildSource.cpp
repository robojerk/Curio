#include "TrackedBuildSource.h"

#include "CurioSettings.h"
#include "GitHost.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace {
GitHostKind kindFromProviderId(const QString &providerId)
{
    if (providerId == QStringLiteral("gitlab"))
        return GitHostKind::GitLab;
    if (providerId == QStringLiteral("codeberg"))
        return GitHostKind::Codeberg;
    return GitHostKind::GitHub;
}

QString releasesUrlForProject(const TrackedBuildProject &project)
{
    const GitHostKind kind = kindFromProviderId(project.providerId);
    const QString slug = project.repoSlug;
    switch (kind) {
    case GitHostKind::GitLab: {
        const int slash = slug.indexOf(QLatin1Char('/'));
        const QString encoded = slash >= 0
                ? QUrl::toPercentEncoding(slug.left(slash)).constData()
                    + QStringLiteral("%2F")
                    + QUrl::toPercentEncoding(slug.mid(slash + 1)).constData()
                : QUrl::toPercentEncoding(slug).constData();
        return QStringLiteral("https://gitlab.com/api/v4/projects/%1/releases?per_page=100").arg(encoded);
    }
    case GitHostKind::Codeberg:
        return QStringLiteral("https://codeberg.org/api/v1/repos/%1/releases?limit=100").arg(slug);
    case GitHostKind::GitHub:
    default:
        return QStringLiteral("https://api.github.com/repos/%1/releases?per_page=100").arg(slug);
    }
}
}

TrackedBuildSource::TrackedBuildSource(QNetworkAccessManager *networkManager, QObject *parent)
    : QObject(parent)
    , m_networkManager(networkManager)
{
}

void TrackedBuildSource::applyAuthHeaders(QNetworkRequest &request, GitHostKind kind) const
{
    CurioSettings &settings = CurioSettings::instance();
    settings.load();
    QString token;
    switch (kind) {
    case GitHostKind::GitLab:
        token = settings.effectiveGitlabPat();
        if (!token.isEmpty())
            request.setRawHeader("PRIVATE-TOKEN", token.toUtf8());
        break;
    case GitHostKind::Codeberg:
        token = settings.effectiveCodebergPat();
        if (!token.isEmpty())
            request.setRawHeader("Authorization", QByteArray("token ") + token.toUtf8());
        break;
    case GitHostKind::GitHub:
    default:
        token = settings.effectiveGithubPat();
        if (!token.isEmpty())
            request.setRawHeader("Authorization", QByteArray("Bearer ") + token.toUtf8());
        break;
    }
}

void TrackedBuildSource::applyProviderHeaders(QNetworkRequest &request, GitHostKind kind)
{
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Curio/1.0 (+https://github.com/robojerk/curio)"));
    if (kind == GitHostKind::GitHub) {
        request.setRawHeader("Accept", "application/vnd.github+json");
        request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    }
}

QString TrackedBuildSource::httpErrorMessage(int statusCode, const QByteArray &body, GitHostKind kind)
{
    QString apiMessage;
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (doc.isObject())
        apiMessage = doc.object().value(QStringLiteral("message")).toString();

    if (statusCode == 403 || statusCode == 429) {
        const bool rateLimited = apiMessage.contains(QStringLiteral("rate limit"), Qt::CaseInsensitive);
        if (kind == GitHostKind::GitHub && (rateLimited || statusCode == 403)) {
            return QObject::tr(
                    "GitHub API rate limit exceeded. Add a token in Settings → Tracked Builds "
                    "(public repos only need a token with no scopes), set GITHUB_TOKEN, or try again later.");
        }
        if (rateLimited || statusCode == 429) {
            return QObject::tr(
                    "API rate limit exceeded. Add a token in Settings → Tracked Builds or try again later.");
        }
        return QObject::tr(
                "Access denied (HTTP %1). Check repository visibility or add an API token in Settings → Tracked Builds.")
                .arg(statusCode);
    }
    if (statusCode == 404)
        return QObject::tr("Repository or releases not found (HTTP 404).");
    if (!apiMessage.isEmpty())
        return QObject::tr("HTTP %1: %2").arg(statusCode).arg(apiMessage);
    return QObject::tr("HTTP error %1").arg(statusCode);
}

bool TrackedBuildSource::tryReleaseCache(const QString &cacheKey, bool allowStale,
                                         QVector<TrackedBuildRelease> *releases, QString *error) const
{
    const auto it = m_releaseCache.constFind(cacheKey);
    if (it == m_releaseCache.cend())
        return false;

    const ReleaseCacheEntry &entry = it.value();
    const qint64 ageSecs = entry.fetchedAtUtc.secsTo(QDateTime::currentDateTimeUtc());
    const qint64 maxAge = allowStale ? 86'400 : 1'200;
    if (ageSecs < 0 || ageSecs > maxAge)
        return false;
    if (!entry.error.isEmpty() && !allowStale)
        return false;

    if (releases)
        *releases = entry.releases;
    if (error)
        *error = entry.error;
    return true;
}

void TrackedBuildSource::storeReleaseCache(const QString &cacheKey,
                                           const QVector<TrackedBuildRelease> &releases,
                                           const QString &error)
{
    ReleaseCacheEntry entry;
    entry.releases = releases;
    entry.error = error;
    entry.fetchedAtUtc = QDateTime::currentDateTimeUtc();
    m_releaseCache.insert(cacheKey, entry);
}

void TrackedBuildSource::fetchReleases(
    const TrackedBuildProject &project,
    const std::function<void(const FetchResult &)> &callback)
{
    if (!m_networkManager) {
        if (callback) {
            FetchResult result;
            result.error = QObject::tr("Network unavailable");
            callback(result);
        }
        return;
    }
    if (project.repoSlug.isEmpty()) {
        if (callback) {
            FetchResult result;
            result.error = QObject::tr("Repository slug is required");
            callback(result);
        }
        return;
    }

    const GitHostKind kind = kindFromProviderId(project.providerId);
    const QString url = releasesUrlForProject(project);
    const QString cacheKey = QStringLiteral("%1|%2").arg(project.providerId, url);

    QVector<TrackedBuildRelease> cachedReleases;
    QString cachedError;
    if (tryReleaseCache(cacheKey, false, &cachedReleases, &cachedError)) {
        if (callback) {
            FetchResult result;
            result.releases = cachedReleases;
            result.error = cachedError;
            callback(result);
        }
        return;
    }

    QNetworkRequest request{QUrl(url)};
    applyProviderHeaders(request, kind);
    applyAuthHeaders(request, kind);
    if (!project.cachedEtag.isEmpty())
        request.setRawHeader("If-None-Match", project.cachedEtag.toUtf8());
    if (!project.cachedLastModified.isEmpty())
        request.setRawHeader("If-Modified-Since", project.cachedLastModified.toUtf8());

    QNetworkReply *reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, kind, cacheKey, callback]() {
        FetchResult result;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray payload = reply->readAll();

        const QByteArray responseEtag = reply->rawHeader("ETag");
        const QByteArray responseLastModified = reply->rawHeader("Last-Modified");
        const QByteArray responseRateLimitReset = reply->rawHeader("X-RateLimit-Reset");
        if (!responseEtag.isEmpty())
            result.etag = QString::fromUtf8(responseEtag);
        if (!responseLastModified.isEmpty())
            result.lastModified = QString::fromUtf8(responseLastModified);
        if (!responseRateLimitReset.isEmpty()) {
            bool ok = false;
            const qint64 resetSeconds = QString::fromUtf8(responseRateLimitReset).toLongLong(&ok);
            if (ok) {
                result.nextAllowedRefreshAtIso = QDateTime::fromSecsSinceEpoch(resetSeconds, Qt::UTC)
                        .toString(Qt::ISODate);
            }
        }

        if (status == 304) {
            result.notModified = true;
            if (tryReleaseCache(cacheKey, true, &result.releases, nullptr)) {
                result.error.clear();
            }
            if (callback)
                callback(result);
            reply->deleteLater();
            return;
        }

        if (status >= 400) {
            result.error = httpErrorMessage(status, payload, kind);
            if (status == 403 || status == 429)
                result.rateLimited = true;
            if ((status == 403 || status == 429)
                    && tryReleaseCache(cacheKey, true, &result.releases, nullptr)
                    && !result.releases.isEmpty()) {
                result.error.clear();
                if (result.nextAllowedRefreshAtIso.isEmpty()) {
                    result.nextAllowedRefreshAtIso = QDateTime::currentDateTimeUtc()
                            .addSecs(30 * 60)
                            .toString(Qt::ISODate);
                }
                if (callback)
                    callback(result);
                reply->deleteLater();
                return;
            }
        } else if (reply->error() != QNetworkReply::NoError) {
            result.error = reply->errorString();
            if (tryReleaseCache(cacheKey, true, &result.releases, nullptr)
                    && !result.releases.isEmpty()) {
                result.error.clear();
                if (callback)
                    callback(result);
                reply->deleteLater();
                return;
            }
        } else {
            switch (kind) {
            case GitHostKind::GitLab:
                result.releases = parseGitLabReleases(payload, &result.error);
                break;
            case GitHostKind::Codeberg:
                result.releases = parseForgejoReleases(payload, &result.error);
                break;
            case GitHostKind::GitHub:
            default:
                result.releases = parseGithubReleases(payload, &result.error);
                break;
            }
            if (result.error.isEmpty())
                storeReleaseCache(cacheKey, result.releases, QString());
        }

        if (callback)
            callback(result);
        reply->deleteLater();
    });
}

QVector<TrackedBuildRelease> TrackedBuildSource::parseGithubReleases(const QByteArray &payload, QString *error)
{
    QVector<TrackedBuildRelease> releases;
    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    if (!doc.isArray()) {
        if (error)
            *error = QObject::tr("Unexpected GitHub response format");
        return releases;
    }
    const QJsonArray arr = doc.array();
    releases.reserve(arr.size());
    for (const QJsonValue &value : arr) {
        const QJsonObject obj = value.toObject();
        if (obj.isEmpty())
            continue;
        TrackedBuildRelease rel;
        rel.releaseId = QString::number(static_cast<qint64>(obj.value(QStringLiteral("id")).toDouble()));
        rel.tagName = obj.value(QStringLiteral("tag_name")).toString();
        rel.title = obj.value(QStringLiteral("name")).toString();
        rel.publishedAtIso = obj.value(QStringLiteral("published_at")).toString();
        rel.isPrerelease = obj.value(QStringLiteral("prerelease")).toBool(false);
        rel.isDraft = obj.value(QStringLiteral("draft")).toBool(false);
        const QJsonArray assets = obj.value(QStringLiteral("assets")).toArray();
        for (const QJsonValue &assetVal : assets) {
            const QJsonObject a = assetVal.toObject();
            TrackedBuildAsset asset;
            asset.name = a.value(QStringLiteral("name")).toString();
            asset.url = a.value(QStringLiteral("browser_download_url")).toString();
            asset.updatedAtIso = a.value(QStringLiteral("updated_at")).toString();
            if (!asset.name.isEmpty() && !asset.url.isEmpty())
                rel.assets.append(asset);
        }
        releases.append(rel);
    }
    return releases;
}

QVector<TrackedBuildRelease> TrackedBuildSource::parseGitLabReleases(const QByteArray &payload, QString *error)
{
    QVector<TrackedBuildRelease> releases;
    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    if (!doc.isArray()) {
        if (error)
            *error = QObject::tr("Unexpected GitLab response format");
        return releases;
    }
    const QJsonArray arr = doc.array();
    releases.reserve(arr.size());
    for (const QJsonValue &value : arr) {
        const QJsonObject obj = value.toObject();
        if (obj.isEmpty())
            continue;
        TrackedBuildRelease rel;
        rel.releaseId = obj.value(QStringLiteral("tag_name")).toString();
        rel.tagName = obj.value(QStringLiteral("tag_name")).toString();
        rel.title = obj.value(QStringLiteral("name")).toString();
        rel.publishedAtIso = obj.value(QStringLiteral("released_at")).toString();
        // GitLab has no GitHub-style prerelease flag; upcoming_release is scheduled-only.
        rel.isPrerelease = false;
        rel.isDraft = false;
        const QJsonObject assetsObj = obj.value(QStringLiteral("assets")).toObject();
        const QJsonArray links = assetsObj.value(QStringLiteral("links")).toArray();
        for (const QJsonValue &linkVal : links) {
            const QJsonObject link = linkVal.toObject();
            TrackedBuildAsset asset;
            asset.name = link.value(QStringLiteral("name")).toString();
            asset.url = link.value(QStringLiteral("direct_asset_url")).toString();
            if (asset.url.isEmpty())
                asset.url = link.value(QStringLiteral("url")).toString();
            if (asset.name.isEmpty() && !asset.url.isEmpty())
                asset.name = QUrl(asset.url).fileName();
            if (!asset.name.isEmpty() && !asset.url.isEmpty())
                rel.assets.append(asset);
        }
        releases.append(rel);
    }
    return releases;
}

QVector<TrackedBuildRelease> TrackedBuildSource::parseForgejoReleases(const QByteArray &payload, QString *error)
{
    return parseGithubReleases(payload, error);
}
