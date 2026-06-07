#include "FlathubApiClient.h"
#include "AppDisplayNames.h"

#include <functional>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace {

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
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Curio"));
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::NoLessSafeRedirectPolicy);

        QNetworkReply *reply = m_networkManager->get(request);
        connect(reply, &QNetworkReply::finished, this, [reply, state, field, finishOne]() {
            reply->deleteLater();
            QString parseError;
            if (reply->error() != QNetworkReply::NoError) {
                finishOne(reply->errorString());
                return;
            }
            state->result.*field = parseCollectionResponse(reply->readAll(), &parseError);
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
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Curio"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    QJsonObject body;
    body.insert(QStringLiteral("query"), trimmed);
    body.insert(QStringLiteral("page"), 1);
    body.insert(QStringLiteral("per_page"), kSearchPageSize);
    const QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);

    QNetworkReply *reply = m_networkManager->post(request, payload);
    connect(reply, &QNetworkReply::finished, this, [reply, onFinished = std::move(onFinished)]() mutable {
        reply->deleteLater();
        FlathubSearchResult searchResult;
        if (reply->error() != QNetworkReply::NoError) {
            searchResult.errorMessage = reply->errorString();
            onFinished(searchResult);
            return;
        }
        QString parseError;
        searchResult.apps = parseCollectionResponse(reply->readAll(), &parseError);
        if (!parseError.isEmpty())
            searchResult.errorMessage = parseError;
        onFinished(searchResult);
    });
}
