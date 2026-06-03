#pragma once

#include <QString>

namespace FlatpakRemoteCatalog {

QString displayNameForOrigin(const QString &origin);
QString catalogPageUrlForApp(const QString &origin, const QString &appId);
QString catalogPageLabelForOrigin(const QString &origin);
bool isCatalogOrigin(const QString &origin);
/** Bundle / sideload install origin (e.g. orcaslicer-origin), not a user-managed repo. */
bool isBundleInstallOrigin(const QString &origin);

} // namespace FlatpakRemoteCatalog
