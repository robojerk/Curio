#include "FlatpakRefMapper.h"
#include "AppDisplayNames.h"
#include "FlatpakRemoteCatalog.h"
#include "FlatpakGlibInclude.h"

#include <QObject>

AppInfo FlatpakRefMapper::appFromInstalledRef(FlatpakInstalledRef *ref, FlatpakScope scope)
{
    AppInfo info;
    if (!ref)
        return info;

    info.id = QString::fromUtf8(flatpak_ref_get_name(FLATPAK_REF(ref)));
    if (info.id.isEmpty())
        return info;

    const char *origin = flatpak_installed_ref_get_origin(ref);
    info.repoId = origin ? QString::fromUtf8(origin) : QString();
    info.installationScope = flatpakScopeToString(scope);
    info.installed = true;

    const char *appdataVersion = flatpak_installed_ref_get_appdata_version(ref);
    info.version = appdataVersion ? QString::fromUtf8(appdataVersion) : QString();

    g_autoptr(GBytes) appstream = flatpak_installed_ref_load_appdata(ref, nullptr, nullptr);
    if (appstream) {
        g_autoptr(GKeyFile) keyFile = g_key_file_new();
        if (g_key_file_load_from_bytes(keyFile, appstream, G_KEY_FILE_NONE, nullptr)) {
            g_autofree char *name = g_key_file_get_string(keyFile, "Application", "name", nullptr);
            if (name)
                info.name = QString::fromUtf8(name);
            g_autofree char *summary = g_key_file_get_string(keyFile, "Application", "summary", nullptr);
            if (summary)
                info.summary = QString::fromUtf8(summary);
        }
    }

    info.name = AppDisplayNames::displayName(info.name, info.id);
    if (info.summary.isEmpty() && !info.version.isEmpty())
        info.summary = QObject::tr("Version %1").arg(info.version);

    info.iconName = info.id;
    info.templateId = QStringLiteral("default");
    info.storePageUrl = FlatpakRemoteCatalog::catalogPageUrlForApp(info.repoId, info.id);
    return info;
}

AppInfo FlatpakRefMapper::appFromRemoteRef(FlatpakRemoteRef *ref, const QString &remoteName)
{
    AppInfo info;
    if (!ref)
        return info;

    info.id = QString::fromUtf8(flatpak_ref_get_name(FLATPAK_REF(ref)));
    if (info.id.isEmpty())
        return info;

    info.repoId = remoteName;
    info.iconName = info.id;
    info.templateId = QStringLiteral("default");
    info.storePageUrl = FlatpakRemoteCatalog::catalogPageUrlForApp(info.repoId, info.id);

    GBytes *metadata = flatpak_remote_ref_get_metadata(ref);
    if (metadata) {
        g_autoptr(GKeyFile) keyFile = g_key_file_new();
        if (g_key_file_load_from_bytes(keyFile, metadata, G_KEY_FILE_NONE, nullptr)) {
            g_autofree char *name = g_key_file_get_string(keyFile, "Application", "name", nullptr);
            if (name)
                info.name = QString::fromUtf8(name);
            g_autofree char *summary = g_key_file_get_string(keyFile, "Application", "summary", nullptr);
            if (summary)
                info.summary = QString::fromUtf8(summary);
        }
    }

    info.name = AppDisplayNames::displayName(info.name, info.id);
    return info;
}

AppInfo FlatpakRefMapper::appFromSearchRef(FlatpakRef *ref, const QString &remoteName)
{
    AppInfo info;
    if (!ref)
        return info;

    info.id = QString::fromUtf8(flatpak_ref_get_name(ref));
    if (info.id.isEmpty())
        return info;

    info.repoId = remoteName.isEmpty() ? QStringLiteral("flathub") : remoteName;
    info.iconName = info.id;
    info.templateId = QStringLiteral("default");
    info.storePageUrl = FlatpakRemoteCatalog::catalogPageUrlForApp(info.repoId, info.id);

    if (FLATPAK_IS_REMOTE_REF(ref)) {
        GBytes *metadata = flatpak_remote_ref_get_metadata(FLATPAK_REMOTE_REF(ref));
        if (metadata) {
            g_autoptr(GKeyFile) keyFile = g_key_file_new();
            if (g_key_file_load_from_bytes(keyFile, metadata, G_KEY_FILE_NONE, nullptr)) {
                g_autofree char *name = g_key_file_get_string(keyFile, "Application", "name", nullptr);
                if (name)
                    info.name = QString::fromUtf8(name);
                g_autofree char *summary = g_key_file_get_string(keyFile, "Application", "summary", nullptr);
                if (summary)
                    info.summary = QString::fromUtf8(summary);
            }
        }
    }

    info.name = AppDisplayNames::displayName(info.name, info.id);
    return info;
}

QString FlatpakRefMapper::refString(FlatpakRef *ref)
{
    if (!ref)
        return QString();
    g_autofree char *formatted = flatpak_ref_format_ref(ref);
    return formatted ? QString::fromUtf8(formatted) : QString();
}
