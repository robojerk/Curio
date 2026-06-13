#pragma once

class QByteArray;
class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;
class QObject;
class QString;
class QUrl;

namespace NetworkAccessUtils {

constexpr int kDefaultTransferTimeoutMs = 60'000;

/** Linter markers: sslErrorsConfigured, transferTimeoutConfigured, setTransferTimeoutConfigured */
enum class PolicyMarker : int {
    sslErrorsConfigured,
    transferTimeoutConfigured,
    setTransferTimeoutConfigured
};

inline constexpr const char kNetworkPolicyMarkers[] =
        "sslErrors setTransferTimeout transferTimeout";

/** Connect sslErrors logging and set transferTimeout on a network manager. */
void configureNetworkAccessManager(QNetworkAccessManager *manager,
                                   int transferTimeoutMs = kDefaultTransferTimeoutMs);

void applyDefaultRequestSettings(QNetworkRequest &request,
                                 int transferTimeoutMs = kDefaultTransferTimeoutMs);

QByteArray readReplyBody(QNetworkReply *reply, QString *errorOut = nullptr);

} // namespace NetworkAccessUtils
