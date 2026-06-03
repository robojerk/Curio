#pragma once

#include <QObject>
#include <QVector>

#include "models/AppInfo.h"

class QNetworkAccessManager;

struct FlathubCollectionsResult {
    QVector<AppInfo> trending;
    QVector<AppInfo> popular;
    QVector<AppInfo> recent;
    QVector<AppInfo> updated;
    QString errorMessage;
};

class FlathubApiClient : public QObject
{
    Q_OBJECT
public:
    static constexpr int kCollectionPageSize = 24;

    explicit FlathubApiClient(QNetworkAccessManager *networkManager, QObject *parent = nullptr);

    void fetchCollections(std::function<void (const FlathubCollectionsResult &)> onFinished);

private:
    QNetworkAccessManager *m_networkManager = nullptr;
    bool m_ownsNetworkManager = false;
};
