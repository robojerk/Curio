#include "FlathubMediaUtils.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QUrl>

#include <cmath>
#include <limits>

namespace FlathubMediaUtils {

namespace {

constexpr int kGalleryIdealWidth = 752;
constexpr int kGalleryMaxWidth = 1280;
constexpr int kGalleryMinWidth = 320;

QString encodeImgproxySource(const QString &sourceUrl)
{
    return QString::fromUtf8(
            sourceUrl.toUtf8().toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

QString buildImgproxyUrl(const QString &sourceUrl, int resizeWidth, int resizeHeight)
{
    const QString encoded = encodeImgproxySource(sourceUrl);
    if (resizeWidth > 0 && resizeHeight > 0) {
        return QStringLiteral("https://imgproxy.flathub.org/insecure/rs:fit:%1:%2/q:90/f:avif/%3")
                .arg(resizeWidth)
                .arg(resizeHeight)
                .arg(encoded);
    }
    return QStringLiteral("https://imgproxy.flathub.org/insecure/q:90/f:avif/%1").arg(encoded);
}

QString canonicalSourceUrl(const QString &url)
{
    const QString trimmed = url.trimmed();
    if (trimmed.isEmpty())
        return trimmed;

    if (trimmed.startsWith(QStringLiteral("https://imgproxy.flathub.org/")))
        return trimmed;

    if (trimmed.startsWith(QStringLiteral("https://dl.flathub.org/repo/screenshots/"))) {
        return QStringLiteral("https://dl.flathub.org/media/")
                + trimmed.mid(QStringLiteral("https://dl.flathub.org/repo/screenshots/").size());
    }
    return trimmed;
}

bool isFlathubHosted(const QString &url)
{
    return url.startsWith(QStringLiteral("https://dl.flathub.org/"))
            || url.startsWith(QStringLiteral("https://imgproxy.flathub.org/"));
}

} // namespace

QString pickScreenshotUrlFromSizes(const QJsonArray &sizes)
{
    QString bestUrl;
    int bestDistance = std::numeric_limits<int>::max();
    QString fallbackUrl;
    int fallbackWidth = -1;
    QString origUrl;

    for (const QJsonValue &sizeVal : sizes) {
        const QJsonObject sizeObj = sizeVal.toObject();
        const QString src = sizeObj.value(QStringLiteral("src")).toString().trimmed();
        if (src.isEmpty())
            continue;

        if (src.contains(QStringLiteral("_orig."))) {
            origUrl = src;
            continue;
        }

        bool ok = false;
        const int width = sizeObj.value(QStringLiteral("width")).toString().toInt(&ok);
        if (!ok || width <= 0) {
            if (fallbackUrl.isEmpty())
                fallbackUrl = src;
            continue;
        }

        if (width > fallbackWidth) {
            fallbackWidth = width;
            fallbackUrl = src;
        }

        if (width < kGalleryMinWidth || width > kGalleryMaxWidth)
            continue;

        const int distance = std::abs(width - kGalleryIdealWidth);
        if (distance < bestDistance) {
            bestDistance = distance;
            bestUrl = src;
        }
    }

    if (!bestUrl.isEmpty())
        return bestUrl;
    if (!fallbackUrl.isEmpty())
        return fallbackUrl;
    return origUrl;
}

QString thumbnailUrl(const QString &sourceUrl)
{
    const QString canonical = canonicalSourceUrl(sourceUrl);
    if (canonical.isEmpty())
        return canonical;
    if (!isFlathubHosted(canonical))
        return canonical;
    if (canonical.startsWith(QStringLiteral("https://imgproxy.flathub.org/")))
        return canonical;
    return buildImgproxyUrl(canonical, 440, 280);
}

QString detailUrl(const QString &sourceUrl)
{
    const QString canonical = canonicalSourceUrl(sourceUrl);
    if (canonical.isEmpty())
        return canonical;
    if (!isFlathubHosted(canonical))
        return canonical;
    if (canonical.startsWith(QStringLiteral("https://imgproxy.flathub.org/")))
        return canonical;
    return buildImgproxyUrl(canonical, 0, 0);
}

QString proxyFlathubScreenshotUrl(const QUrl &url)
{
    return detailUrl(url.toString());
}

} // namespace FlathubMediaUtils
