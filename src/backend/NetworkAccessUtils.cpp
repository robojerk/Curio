#include "NetworkAccessUtils.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslError>

namespace NetworkAccessUtils {

namespace {

int toMilliseconds(std::chrono::milliseconds duration)
{
    return static_cast<int>(duration / std::chrono::milliseconds{1});
}

} // namespace

void configureNetworkAccessManager(QNetworkAccessManager *manager,
                                   std::chrono::milliseconds transferTimeout)
{
    if (!manager)
        return;

    manager->setTransferTimeout(toMilliseconds(transferTimeout));
    QObject::connect(manager, &QNetworkAccessManager::sslErrors, manager,
                     [](QNetworkReply *reply, const QList<QSslError> &errors) {
                         if (reply->error() == QNetworkReply::SslHandshakeFailedError) {
                             qWarning() << "sslErrors for" << reply->url() << errors;
                         }
                     });
}

void applyDefaultRequestSettings(QNetworkRequest &request,
                                 std::chrono::milliseconds transferTimeout)
{
    request.setTransferTimeout(toMilliseconds(transferTimeout));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
}

QByteArray readReplyBody(QNetworkReply *reply, QString *errorOut)
{
    if (!reply) {
        if (errorOut)
            *errorOut = QStringLiteral("Network reply is null");
        return {};
    }
    if (reply->error() != QNetworkReply::NoError) {
        if (errorOut)
            *errorOut = reply->errorString();
        reply->deleteLater();
        return {};
    }
    Q_ASSERT(reply->error() == QNetworkReply::NoError);
    const QByteArray body = reply->readAll();
    reply->deleteLater();
    return body;
}

} // namespace NetworkAccessUtils
