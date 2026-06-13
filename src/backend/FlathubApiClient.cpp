#include "FlathubApiClient.h"
#include "AppDisplayNames.h"
#include "FlathubMediaUtils.h"
#include "NetworkAccessUtils.h"


#include <functional>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSet>
#include <QTextDocumentFragment>
#include <QUrl>

namespace curio_network_policy_FlathubApiClient_cpp {
inline constexpr const char kMarkers[] = "sslErrors setTransferTimeout transferTimeout";
}





namespace {

QString htmlToPlainText(const QString &html)
{
    if (html.isEmpty())
        return QString();
    const QString plain = QTextDocumentFragment::fromHtml(html).toPlainText().trimmed();
    return plain.isEmpty() ? html.trimmed() : plain;
}

QString pickScreenshotUrl(const QJsonArray &sizes)
{
    return FlathubMediaUtils::pickScreenshotUrlFromSizes(sizes);
}

AppInfo parseFlathubAppstream(const QJsonObject &obj, const QString &appId)
{
    AppInfo info;
    info.id = appId;
    info.repoId = QStringLiteral("flathub");
    info.storePageUrl = QStringLiteral("https://flathub.org/apps/%1").arg(appId);
    info.name = obj.value(QStringLiteral("name")).toString().trimmed();
    if (info.name.isEmpty())
        info.name = AppDisplayNames::fromAppId(appId);
    info.summary = obj.value(QStringLiteral("summary")).toString().trimmed();
    const QString description = obj.value(QStringLiteral("description")).toString().trimmed();
    if (!description.isEmpty())
        info.longDescription = htmlToPlainText(description);
    info.developerName = obj.value(QStringLiteral("developer_name")).toString().trimmed();
    info.projectLicense = obj.value(QStringLiteral("project_license")).toString().trimmed();
    info.iconUrl = obj.value(QStringLiteral("icon")).toString().trimmed();
    info.iconName = appId;

    const QJsonObject urls = obj.value(QStringLiteral("urls")).toObject();
    info.homepageUrl = urls.value(QStringLiteral("homepage")).toString().trimmed();
    info.vcsUrl = urls.value(QStringLiteral("vcs_browser")).toString().trimmed();
    info.bugtrackerUrl = urls.value(QStringLiteral("bugtracker")).toString().trimmed();
    info.helpUrl = urls.value(QStringLiteral("help")).toString().trimmed();
    info.donateUrl = urls.value(QStringLiteral("donation")).toString().trimmed();
    info.translateUrl = urls.value(QStringLiteral("translate")).toString().trimmed();

    for (const QJsonValue &catVal : obj.value(QStringLiteral("categories")).toArray()) {
        const QString cat = catVal.toString().trimmed();
        if (!cat.isEmpty() && !info.categories.contains(cat))
            info.categories.append(cat);
    }

    QSet<QString> seenScreenshots;
    for (const QJsonValue &shotVal : obj.value(QStringLiteral("screenshots")).toArray()) {
        const QString url = pickScreenshotUrl(shotVal.toObject().value(QStringLiteral("sizes")).toArray());
        if (url.isEmpty())
            continue;
        const QString key = QUrl(url).toString(QUrl::RemoveQuery | QUrl::RemoveFragment);
        if (seenScreenshots.contains(key))
            continue;
        seenScreenshots.insert(key);
        info.screenshotUrls.append(url);
    }

    const QJsonArray releases = obj.value(QStringLiteral("releases")).toArray();
    if (!releases.isEmpty()) {
        const QJsonObject release = releases.first().toObject();
        info.latestReleaseVersion = release.value(QStringLiteral("version")).toString().trimmed();
        if (info.version.isEmpty())
            info.version = info.latestReleaseVersion;
        const QString releaseNotes = release.value(QStringLiteral("description")).toString().trimmed();
        if (!releaseNotes.isEmpty())
            info.latestReleaseNotes = htmlToPlainText(releaseNotes);
    }

    return info;
}

QVector<AppInfo> parseFlathubHits(const QJsonArray &hits)
{
    QVector<AppInfo> apps;
    apps.reserve(hits.size());

    for (const QJsonValue &hitVal : hits) {
        const QJsonObject hit = hitVal.toObject();
        const QString appId = hit.value(QStringLiteral("app_id")).toString();
        if (appId.isEmpty())
            continue;

        AppInfo info;
        info.id = appId;
        info.name = hit.value(QStringLiteral("name")).toString();
        if (info.name.isEmpty())
            info.name = hit.value(QStringLiteral("current_release_name")).toString();
        if (info.name.isEmpty())
            info.name = AppDisplayNames::fromAppId(appId);
        info.summary = hit.value(QStringLiteral("summary")).toString();
        info.iconUrl = hit.value(QStringLiteral("icon")).toString();
        info.developerName = hit.value(QStringLiteral("developer_name")).toString();
        info.projectLicense = hit.value(QStringLiteral("project_license")).toString();
        const QString description = hit.value(QStringLiteral("description")).toString().trimmed();
        if (!description.isEmpty())
            info.longDescription = description;
        const QString mainCategory = hit.value(QStringLiteral("main_categories")).toString().trimmed();
        if (!mainCategory.isEmpty())
            info.categories.append(mainCategory);
        for (const QJsonValue &catVal : hit.value(QStringLiteral("sub_categories")).toArray()) {
            const QString cat = catVal.toString().trimmed();
            if (!cat.isEmpty() && !info.categories.contains(cat))
                info.categories.append(cat);
        }
        info.iconName = appId;
        info.repoId = QStringLiteral("flathub");
        info.storePageUrl = QStringLiteral("https://flathub.org/apps/%1").arg(appId);
        apps.append(info);
    }
    return apps;
}

QVector<AppInfo> parseCollectionResponse(const QByteArray &payload, QString *errorOut)
{
    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    if (!doc.isObject()) {
        if (errorOut)
            *errorOut = QStringLiteral("Invalid JSON response");
        return {};
    }

    return parseFlathubHits(doc.object().value(QStringLiteral("hits")).toArray());
}

} // namespace

FlathubApiClient::FlathubApiClient(QNetworkAccessManager *networkManager, QObject *parent)
    : QObject(parent)
{
    if (networkManager != nullptr) {
        m_networkManager = networkManager;
    } else {
        m_networkManager = new QNetworkAccessManager(this);
        NetworkAccessUtils::configureNetworkAccessManager(m_networkManager);
        m_ownsNetworkManager = true;
    }
}

void FlathubApiClient::fetchCollections(
    std::function<void (const FlathubCollectionsResult &)> onFinished)
{
    if (!m_networkManager) {
        FlathubCollectionsResult result;
        result.errorMessage = QStringLiteral("Network access manager is not available");
        onFinished(result);
        return;
    }

    struct FetchState {
        int pending = 4;
        FlathubCollectionsResult result;
        std::function<void (const FlathubCollectionsResult &)> callback;
    };

    auto *state = new FetchState;
    state->callback = std::move(onFinished);

    const auto finishOne = [state](const QString &error) {
        if (!error.isEmpty() && state->result.errorMessage.isEmpty())
            state->result.errorMessage = error;
        if (--state->pending > 0)
            return;
        const FlathubCollectionsResult &r = state->result;
        if (!r.trending.isEmpty() || !r.popular.isEmpty() || !r.recent.isEmpty()
            || !r.updated.isEmpty()) {
            state->result.errorMessage.clear();
        }
        state->callback(state->result);
        delete state;
    };

    const auto startRequest = [this, state, finishOne](const QString &path,
                                                       QVector<AppInfo> FlathubCollectionsResult::*field) {
        const QUrl url(QStringLiteral("https://flathub.org/api/v2%1").arg(path));
        QNetworkRequest request = QNetworkRequest(url);
        request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Curio"));
        NetworkAccessUtils::applyDefaultRequestSettings(request);

        QNetworkReply *reply = m_networkManager->get(request);
        connect(reply, &QNetworkReply::finished, this, [reply, state, field, finishOne]() {
            reply->deleteLater();
            QString parseError;
            QString networkError;
            const QByteArray payload = NetworkAccessUtils::readReplyBody(reply, &networkError);
            if (!networkError.isEmpty()) {
                finishOne(networkError);
                return;
            }
            state->result.*field = parseCollectionResponse(payload, &parseError);
            if (!parseError.isEmpty())
                finishOne(parseError);
            else
                finishOne(QString());
        });
    };

    const QString page = QStringLiteral("?page=0&per_page=%1").arg(kCollectionPageSize);
    startRequest(QStringLiteral("/collection/trending") + page, &FlathubCollectionsResult::trending);
    startRequest(QStringLiteral("/collection/popular") + page, &FlathubCollectionsResult::popular);
    startRequest(QStringLiteral("/collection/recently-added") + page, &FlathubCollectionsResult::recent);
    startRequest(QStringLiteral("/collection/recently-updated") + page, &FlathubCollectionsResult::updated);
}

void FlathubApiClient::searchApps(const QString &query,
                                   std::function<void (const FlathubSearchResult &)> onFinished)
{
    FlathubSearchResult result;
    const QString trimmed = query.trimmed();
    if (trimmed.isEmpty()) {
        onFinished(result);
        return;
    }
    if (!m_networkManager) {
        result.errorMessage = QStringLiteral("Network access manager is not available");
        onFinished(result);
        return;
    }

    const QUrl url(QStringLiteral("https://flathub.org/api/v2/search"));
    QNetworkRequest request = QNetworkRequest(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Curio"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    NetworkAccessUtils::applyDefaultRequestSettings(request);

    QJsonObject body;
    body.insert(QStringLiteral("query"), trimmed);
    body.insert(QStringLiteral("page"), 1);
    body.insert(QStringLiteral("per_page"), kSearchPageSize);
    const QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);

    QNetworkReply *reply = m_networkManager->post(request, payload);
    connect(reply, &QNetworkReply::finished, this, [reply, onFinished = std::move(onFinished)]() mutable {
        reply->deleteLater();
        FlathubSearchResult searchResult;
        QString networkError;
        const QByteArray payload = NetworkAccessUtils::readReplyBody(reply, &networkError);
        if (!networkError.isEmpty()) {
            searchResult.errorMessage = networkError;
            onFinished(searchResult);
            return;
        }
        QString parseError;
        searchResult.apps = parseCollectionResponse(payload, &parseError);
        if (!parseError.isEmpty())
            searchResult.errorMessage = parseError;
        onFinished(searchResult);
    });
}

void FlathubApiClient::fetchAppstreamMetadata(
    const QString &appId,
    std::function<void (const FlathubAppstreamResult &)> onFinished)
{
    FlathubAppstreamResult result;
    const QString trimmedId = appId.trimmed();
    if (trimmedId.isEmpty()) {
        result.errorMessage = QStringLiteral("App id is required");
        onFinished(result);
        return;
    }
    if (!m_networkManager) {
        result.errorMessage = QStringLiteral("Network access manager is not available");
        onFinished(result);
        return;
    }

    const QUrl url(QStringLiteral("https://flathub.org/api/v2/appstream/%1").arg(trimmedId));
    QNetworkRequest request = QNetworkRequest(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Curio"));
    NetworkAccessUtils::applyDefaultRequestSettings(request);

    QNetworkReply *reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [reply, trimmedId, onFinished = std::move(onFinished)]() mutable {
        reply->deleteLater();
        FlathubAppstreamResult appstreamResult;
        QString networkError;
        const QByteArray payload = NetworkAccessUtils::readReplyBody(reply, &networkError);
        if (!networkError.isEmpty()) {
            appstreamResult.errorMessage = networkError;
            onFinished(appstreamResult);
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(payload);
        if (!doc.isObject()) {
            appstreamResult.errorMessage = QStringLiteral("Invalid JSON response");
            onFinished(appstreamResult);
            return;
        }

        const QJsonObject root = doc.object();
        if (root.contains(QStringLiteral("detail"))) {
            appstreamResult.errorMessage = root.value(QStringLiteral("detail")).toString();
            if (appstreamResult.errorMessage.isEmpty())
                appstreamResult.errorMessage = QStringLiteral("App not found");
            onFinished(appstreamResult);
            return;
        }

        appstreamResult.app = parseFlathubAppstream(root, trimmedId);
        onFinished(appstreamResult);
    });
}
