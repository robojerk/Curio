#pragma once

class QJsonArray;
class QString;
class QUrl;

namespace FlathubMediaUtils {

/** Pick a gallery-sized screenshot URL from Flathub appstream API size variants. */
QString pickScreenshotUrlFromSizes(const QJsonArray &sizes);

/** imgproxy URL sized for the details-page thumbnail strip (220×140). */
QString thumbnailUrl(const QString &sourceUrl);

/** imgproxy URL for fullscreen view (AVIF, not full PNG original). */
QString detailUrl(const QString &sourceUrl);

/** Legacy helper used when enriching from AppStream metadata. */
QString proxyFlathubScreenshotUrl(const QUrl &url);

} // namespace FlathubMediaUtils
