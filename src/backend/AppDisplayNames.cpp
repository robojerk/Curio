#include "AppDisplayNames.h"

namespace AppDisplayNames {

QString fromAppId(const QString &appId)
{
    if (appId.isEmpty())
        return appId;
    const int lastDot = appId.lastIndexOf(QLatin1Char('.'));
    QString segment = lastDot >= 0 ? appId.mid(lastDot + 1) : appId;
    if (segment.isEmpty())
        return appId;
    segment.replace(QLatin1Char('_'), QLatin1Char(' '));
    segment.replace(QLatin1Char('-'), QLatin1Char(' '));
    if (!segment.isEmpty())
        segment[0] = segment.at(0).toUpper();
    return segment;
}

QString displayName(const QString &name, const QString &appId)
{
    const QString trimmed = name.trimmed();
    if (!trimmed.isEmpty() && trimmed != appId)
        return trimmed;
    return fromAppId(appId);
}

} // namespace AppDisplayNames
