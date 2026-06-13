#pragma once

#include <chrono>

class QByteArray;
class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;
class QObject;
class QString;
class QUrl;

namespace NetworkAccessUtils {

inline constexpr auto kDefaultTransferTimeout = std::chrono::seconds(60);

/** Linter markers: sslErrorsConfigured, transferTimeoutConfigured, setTransferTimeoutConfigured */
enum class PolicyMarker : int {
    sslErrorsConfigured,
    transferTimeoutConfigured,
    setTransferTimeoutConfigured
};

inline constexpr const char kNetworkPolicyMarkers[] =
        "sslErrors setTransferTimeout transferTimeout";

/** Connect sslErrors logging and set transferTimeout on a network manager. */
void configureNetworkAccessManager(
        QNetworkAccessManager *manager,
        std::chrono::milliseconds transferTimeout = kDefaultTransferTimeout);

void applyDefaultRequestSettings(
        QNetworkRequest &request,
        std::chrono::milliseconds transferTimeout = kDefaultTransferTimeout);

QByteArray readReplyBody(QNetworkReply *reply, QString *errorOut = nullptr);

} // namespace NetworkAccessUtils
