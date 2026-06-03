#pragma once

#include <QString>

namespace AppDisplayNames {

/** Human-readable title derived from a Flatpak app id (last segment, title-cased). */
QString fromAppId(const QString &appId);

/** Prefer metadata name; fall back to id-derived title when missing or equal to id. */
QString displayName(const QString &name, const QString &appId);

} // namespace AppDisplayNames
