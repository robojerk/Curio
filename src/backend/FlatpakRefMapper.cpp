#include "FlatpakRefMapper.h"
#include "AppDisplayNames.h"
#include "FlatpakRemoteCatalog.h"
#include "FlatpakGlibInclude.h"
#include "SandboxedFlatpakEnv.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QStandardPaths>

namespace {

QString readDesktopIconName(const QString &desktopPath)
{
    QFile file(desktopPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    while (!file.atEnd()) {
        const QByteArray line = file.readLine().trimmed();
        if (line.startsWith("Icon="))
            return QString::fromUtf8(line.mid(5).trimmed());
    }
    return QString();
}

QString findIconInTree(const QString &iconsRoot, const QString &iconBaseName)
{
    if (iconsRoot.isEmpty() || iconBaseName.isEmpty())
        return QString();

    static const char *sizes[] = {"512x512", "256x256", "128x128", "64x64", "48x48", "32x32"};
    for (const char *size : sizes) {
        for (const QString &ext : {QStringLiteral(".png"), QStringLiteral(".svg")}) {
            const QString path = iconsRoot + QStringLiteral("/hicolor/") + QLatin1String(size)
                    + QStringLiteral("/apps/") + iconBaseName + ext;
            if (QFileInfo::exists(path))
                return path;
        }
    }
    const QString scalable = iconsRoot + QStringLiteral("/hicolor/scalable/apps/") + iconBaseName
            + QStringLiteral(".svg");
    if (QFileInfo::exists(scalable))
        return scalable;

    QDirIterator it(iconsRoot,
                    QStringList{iconBaseName + QStringLiteral(".png"),
                                iconBaseName + QStringLiteral(".svg")},
                    QDir::Files,
                    QDirIterator::Subdirectories);
    if (it.hasNext())
        return it.next();
    return QString();
}

} // namespace

QString FlatpakRefMapper::installedIconName(const QString &appId)
{
    if (appId.isEmpty())
        return QString();

    const QString desktopName = appId + QStringLiteral(".desktop");
    const QString userFlatpak = SandboxedFlatpakEnv::userFlatpakDataDir();
    const QStringList exportAppDirs = {
        userFlatpak + QStringLiteral("/exports/share/applications"),
        QStringLiteral("/var/lib/flatpak/exports/share/applications"),
    };
    for (const QString &dir : exportAppDirs) {
        const QString desktopPath = dir + QLatin1Char('/') + desktopName;
        if (!QFileInfo::exists(desktopPath))
            continue;
        const QString iconName = readDesktopIconName(desktopPath);
        if (!iconName.isEmpty())
            return iconName;
    }
    return appId;
}

QString FlatpakRefMapper::installedIconFilePath(const QString &appId)
{
    if (appId.isEmpty())
        return QString();

    const QString iconName = installedIconName(appId);
    const QString userFlatpak = SandboxedFlatpakEnv::userFlatpakDataDir();
    const QStringList exportIconRoots = {
        userFlatpak + QStringLiteral("/exports/share/icons"),
        QStringLiteral("/var/lib/flatpak/exports/share/icons"),
    };
    for (const QString &root : exportIconRoots) {
        const QString path = findIconInTree(root, iconName);
        if (!path.isEmpty())
            return path;
        if (iconName != appId) {
            const QString fallback = findIconInTree(root, appId);
            if (!fallback.isEmpty())
                return fallback;
        }
    }

    const QStringList appRoots = {
        userFlatpak + QStringLiteral("/app/") + appId,
        QStringLiteral("/var/lib/flatpak/app/") + appId,
    };
    for (const QString &appRoot : appRoots) {
        QDir rootDir(appRoot);
        if (!rootDir.exists())
            continue;
        const QStringList arches = rootDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &arch : arches) {
            QDir archDir(appRoot + QLatin1Char('/') + arch);
            const QStringList branches = archDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QString &branch : branches) {
                const QString filesIcons = appRoot + QLatin1Char('/') + arch + QLatin1Char('/')
                        + branch + QStringLiteral("/active/files/share/icons");
                const QString path = findIconInTree(filesIcons, iconName);
                if (!path.isEmpty())
                    return path;
                if (iconName != appId) {
                    const QString fallback = findIconInTree(filesIcons, appId);
                    if (!fallback.isEmpty())
                        return fallback;
                }
            }
        }
    }
    return QString();
}

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
    const char *branch = flatpak_ref_get_branch(FLATPAK_REF(ref));
    if (info.version.isEmpty() && branch && branch[0] != '\0')
        info.version = QString::fromUtf8(branch);

    info.installedFlatpakRef = refString(FLATPAK_REF(ref));

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

    info.iconName = installedIconName(info.id);
    const QString iconPath = installedIconFilePath(info.id);
    if (!iconPath.isEmpty())
        info.iconUrl = iconPath;
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
