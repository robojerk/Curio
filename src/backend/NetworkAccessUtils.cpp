#include "NetworkAccessUtils.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslError>

namespace NetworkAccessUtils {

void configureNetworkAccessManager(QNetworkAccessManager *manager, int transferTimeoutMs)
{
    if (!manager)
        return;

    manager->setTransferTimeout(transferTimeoutMs);
    QObject::connect(manager, &QNetworkAccessManager::sslErrors, manager,
                     [](QNetworkReply *reply, const QList<QSslError> &errors) {
                         if (reply->error() == QNetworkReply::SslHandshakeFailedError) {
                             qWarning() << "sslErrors for" << reply->url() << errors;
                         }
                     });
}

void applyDefaultRequestSettings(QNetworkRequest &request, int transferTimeoutMs)
{
    request.setTransferTimeout(transferTimeoutMs);
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
        return {};
    }
    return reply->readAll();
}

} // namespace NetworkAccessUtils
