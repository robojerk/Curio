#include "FlatpakInstallationService.h"
#include "AppDisplayNames.h"
#include "FlatpakRefMapper.h"
#include "FlatpakRemoteCatalog.h"
#include "FlatpakGlibInclude.h"

#include <QDir>
#include <QMutexLocker>
#include <QSet>
#include <QStandardPaths>
#include <QDebug>

namespace {

void setGErrorString(GError *error, QString *errorOut)
{
    if (errorOut && error)
        *errorOut = QString::fromUtf8(error->message);
}

/** Locale/Debug/GL/VAAPI/extension runtimes — not shown in the summary row count. */
bool isRuntimeExtensionId(const QString &id)
{
    return id.endsWith(QStringLiteral(".Locale"))
            || id.endsWith(QStringLiteral(".Debug"))
            || id.contains(QStringLiteral(".Platform.GL."))
            || id.contains(QStringLiteral(".Platform.VAAPI."))
            || id.contains(QStringLiteral(".Platform.codecs-extra"))
            || id.contains(QStringLiteral(".Platform.openh264"));
}

void mergeNewestRuntimeBranch(QHash<QString, QString> *newestById, const QString &id, const QString &branch)
{
    if (id.isEmpty() || branch.isEmpty())
        return;
    const auto it = newestById->constFind(id);
    if (it == newestById->cend() || QString::compare(branch, it.value()) > 0)
        newestById->insert(id, branch);
}

void collectNewestInstalledRuntimeBranches(FlatpakInstallation *installation,
                                           QHash<QString, QString> *newestById)
{
    if (!installation || !newestById)
        return;

    g_autoptr(GError) error = nullptr;
    g_autoptr(GPtrArray) refs =
            flatpak_installation_list_installed_refs(installation, nullptr, &error);
    if (!refs)
        return;

    for (guint i = 0; i < refs->len; ++i) {
        auto *installedRef = FLATPAK_INSTALLED_REF(g_ptr_array_index(refs, i));
        if (flatpak_ref_get_kind(FLATPAK_REF(installedRef)) != FLATPAK_REF_KIND_RUNTIME)
            continue;

        const QString id = QString::fromUtf8(flatpak_ref_get_name(FLATPAK_REF(installedRef)));
        const char *branch = flatpak_ref_get_branch(FLATPAK_REF(installedRef));
        if (id.isEmpty() || !branch || branch[0] == '\0')
            continue;
        mergeNewestRuntimeBranch(newestById, id, QString::fromUtf8(branch));
    }
}

bool isSupersededRuntimeBranch(const QString &id,
                               const QString &branch,
                               const QHash<QString, QString> &newestById)
{
    const auto it = newestById.constFind(id);
    if (it == newestById.cend())
        return false;
    return branch != it.value();
}

QVector<AppInfo> dedupeRuntimeUpdatesByRef(QVector<AppInfo> updates)
{
    QHash<QString, AppInfo> byRef;
    for (const AppInfo &info : updates) {
        const QString key = info.installedFlatpakRef.trimmed();
        if (key.isEmpty())
            continue;
        const auto it = byRef.find(key);
        if (it == byRef.end()) {
            byRef.insert(key, info);
            continue;
        }
        if (info.installationScope == QStringLiteral("user"))
            it.value() = info;
    }
    return byRef.values().toVector();
}

} // namespace

FlatpakInstallationService::FlatpakInstallationService()
{
    openInstallationPair(FlatpakScope::System, &m_systemSilent, &m_systemInteractive);
    openInstallationPair(FlatpakScope::User, &m_userSilent, &m_userInteractive);

    m_valid = m_systemSilent || m_userSilent;
    if (!m_valid)
        m_lastError = QStringLiteral("Failed to initialize any Flatpak installation");
}

FlatpakInstallationService::~FlatpakInstallationService()
{
    g_clear_object(&m_systemSilent);
    g_clear_object(&m_systemInteractive);
    g_clear_object(&m_userSilent);
    g_clear_object(&m_userInteractive);
}

void FlatpakInstallationService::openInstallationPair(FlatpakScope scope,
                                                      FlatpakInstallation **silentOut,
                                                      FlatpakInstallation **interactiveOut)
{
    g_autoptr(GError) error = nullptr;
    FlatpakInstallation *silent = nullptr;

#ifdef CURIO_SANDBOXED_LIBFLATPAK
    if (scope == FlatpakScope::User) {
        // Host ~/.local/share/flatpak is bind-mounted at $XDG_DATA_HOME/flatpak in the sandbox.
        const QString userPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                + QStringLiteral("/flatpak");
        g_autoptr(GFile) userInstall = g_file_new_for_path(userPath.toUtf8().constData());
        silent = flatpak_installation_new_for_path(userInstall, TRUE, nullptr, &error);
    } else {
        silent = flatpak_installation_new_system(nullptr, &error);
    }
#else
    if (scope == FlatpakScope::User)
        silent = flatpak_installation_new_user(nullptr, &error);
    else
        silent = flatpak_installation_new_system(nullptr, &error);
#endif

    if (!silent) {
        if (error)
            qWarning() << "FlatpakInstallationService: failed to open"
                       << flatpakScopeToString(scope) << "installation:" << error->message;
        g_clear_error(&error);
        return;
    }

    flatpak_installation_set_no_interaction(silent, TRUE);
    *silentOut = silent;

    g_autoptr(GFile) installPath = flatpak_installation_get_path(silent);
    FlatpakInstallation *interactive = flatpak_installation_new_for_path(
            installPath,
            scope == FlatpakScope::User,
            nullptr,
            &error);
    if (!interactive) {
        if (error)
            qWarning() << "FlatpakInstallationService: failed to open interactive"
                       << flatpakScopeToString(scope) << "installation:" << error->message;
        return;
    }

    flatpak_installation_set_no_interaction(interactive, FALSE);
    *interactiveOut = interactive;
}

FlatpakInstallation *FlatpakInstallationService::silentInstallation(FlatpakScope scope) const
{
    QMutexLocker lock(&m_mutex);
    return scope == FlatpakScope::System ? m_systemSilent : m_userSilent;
}

FlatpakInstallation *FlatpakInstallationService::interactiveInstallation(FlatpakScope scope) const
{
    QMutexLocker lock(&m_mutex);
    return scope == FlatpakScope::System ? m_systemInteractive : m_userInteractive;
}

bool FlatpakInstallationService::remoteExistsOnInstallation(FlatpakInstallation *installation,
                                                            const QString &remoteName) const
{
    if (!installation || remoteName.trimmed().isEmpty())
        return false;

    g_autoptr(GError) error = nullptr;
    g_autoptr(GPtrArray) remoteList = flatpak_installation_list_remotes(installation, nullptr, &error);
    if (!remoteList)
        return false;

    const QByteArray needle = remoteName.trimmed().toUtf8();
    for (guint i = 0; i < remoteList->len; ++i) {
        auto *remote = FLATPAK_REMOTE(g_ptr_array_index(remoteList, i));
        const char *name = flatpak_remote_get_name(remote);
        if (name && g_strcmp0(name, needle.constData()) == 0)
            return true;
    }
    return false;
}

QVector<AppInfo> FlatpakInstallationService::listInstalledAppsForScope(FlatpakScope scope)
{
    QVector<AppInfo> apps;
    FlatpakInstallation *installation = silentInstallation(scope);
    if (!installation)
        return apps;

    g_autoptr(GError) error = nullptr;
    g_autoptr(GPtrArray) refs = flatpak_installation_list_installed_refs(installation, nullptr, &error);
    if (!refs) {
        if (error)
            qWarning() << "FlatpakInstallationService: list installed failed:" << error->message;
        return apps;
    }

    for (guint i = 0; i < refs->len; ++i) {
        auto *installedRef = FLATPAK_INSTALLED_REF(g_ptr_array_index(refs, i));
        if (flatpak_ref_get_kind(FLATPAK_REF(installedRef)) != FLATPAK_REF_KIND_APP)
            continue;

        AppInfo info = FlatpakRefMapper::appFromInstalledRef(installedRef, scope);
        if (info.id.isEmpty())
            continue;

        m_scopeByAppId.insert(info.id, scope);
        const QString ref = FlatpakRefMapper::refString(FLATPAK_REF(installedRef));
        if (!ref.isEmpty())
            m_refByAppId.insert(info.id, ref);
        apps.append(info);
    }
    return apps;
}

QVector<AppInfo> FlatpakInstallationService::listInstalledApps()
{
    QMutexLocker lock(&m_mutex);
    m_scopeByAppId.clear();
    m_refByAppId.clear();

    QVector<AppInfo> apps;
    apps += listInstalledAppsForScope(FlatpakScope::System);
    apps += listInstalledAppsForScope(FlatpakScope::User);
    return apps;
}

QVector<AppInfo> FlatpakInstallationService::listAvailableUpdatesForScope(FlatpakScope scope)
{
    QVector<AppInfo> apps;
    FlatpakInstallation *installation = silentInstallation(scope);
    if (!installation)
        return apps;

    g_autoptr(GError) error = nullptr;
    g_autoptr(GPtrArray) refs =
            flatpak_installation_list_installed_refs_for_update(installation, nullptr, &error);
    if (!refs) {
        if (error)
            qWarning() << "FlatpakInstallationService: list updates failed:"
                       << flatpakScopeToString(scope) << error->message;
        return apps;
    }

    for (guint i = 0; i < refs->len; ++i) {
        auto *installedRef = FLATPAK_INSTALLED_REF(g_ptr_array_index(refs, i));
        if (flatpak_ref_get_kind(FLATPAK_REF(installedRef)) != FLATPAK_REF_KIND_APP)
            continue;

        AppInfo info = FlatpakRefMapper::appFromInstalledRef(installedRef, scope);
        if (info.id.isEmpty())
            continue;
        info.remoteUpdateAvailable = true;
        apps.append(info);
    }
    return apps;
}

QVector<AppInfo> FlatpakInstallationService::listAvailableUpdates()
{
    QMutexLocker lock(&m_mutex);
    QVector<AppInfo> apps;
    apps += listAvailableUpdatesForScope(FlatpakScope::System);
    apps += listAvailableUpdatesForScope(FlatpakScope::User);
    return apps;
}

QVector<AppInfo> FlatpakInstallationService::listAvailableRuntimeUpdatesForScope(
        FlatpakScope scope, const QHash<QString, QString> &newestBranches)
{
    QVector<AppInfo> apps;
    FlatpakInstallation *installation = silentInstallation(scope);
    if (!installation)
        return apps;

    g_autoptr(GError) error = nullptr;
    g_autoptr(GPtrArray) refs =
            flatpak_installation_list_installed_refs_for_update(installation, nullptr, &error);
    if (!refs) {
        if (error)
            qWarning() << "FlatpakInstallationService: list runtime updates failed:"
                       << flatpakScopeToString(scope) << error->message;
        return apps;
    }

    for (guint i = 0; i < refs->len; ++i) {
        auto *installedRef = FLATPAK_INSTALLED_REF(g_ptr_array_index(refs, i));
        const auto kind = flatpak_ref_get_kind(FLATPAK_REF(installedRef));
        if (kind != FLATPAK_REF_KIND_RUNTIME)
            continue;

        AppInfo info = FlatpakRefMapper::appFromInstalledRef(installedRef, scope);
        if (info.id.isEmpty() || info.installedFlatpakRef.isEmpty())
            continue;
        if (isRuntimeExtensionId(info.id))
            continue;

        const QString branch = info.version.trimmed();
        if (isSupersededRuntimeBranch(info.id, branch, newestBranches))
            continue;

        info.isRuntimeUpdate = true;
        info.remoteUpdateAvailable = true;
        if (info.name.isEmpty() || info.name == info.id) {
            const QString branch = info.version.trimmed();
            info.name = branch.isEmpty()
                    ? AppDisplayNames::displayName(QString(), info.id)
                    : QObject::tr("%1 version %2")
                              .arg(AppDisplayNames::displayName(QString(), info.id), branch);
        }
        apps.append(info);
    }
    return apps;
}

QVector<AppInfo> FlatpakInstallationService::listAvailableRuntimeUpdates()
{
    QMutexLocker lock(&m_mutex);
    QHash<QString, QString> newestBranches;
    if (FlatpakInstallation *system = m_systemSilent)
        collectNewestInstalledRuntimeBranches(system, &newestBranches);
    if (FlatpakInstallation *user = m_userSilent)
        collectNewestInstalledRuntimeBranches(user, &newestBranches);

    QVector<AppInfo> apps;
    apps += listAvailableRuntimeUpdatesForScope(FlatpakScope::System, newestBranches);
    apps += listAvailableRuntimeUpdatesForScope(FlatpakScope::User, newestBranches);
    return dedupeRuntimeUpdatesByRef(apps);
}

QVector<QPair<QString, QString>> FlatpakInstallationService::listRemotes()
{
    QMutexLocker lock(&m_mutex);
    QVector<QPair<QString, QString>> remotes;
    const FlatpakScope scopes[] = {FlatpakScope::System, FlatpakScope::User};

    for (FlatpakScope scope : scopes) {
        FlatpakInstallation *installation = silentInstallation(scope);
        if (!installation)
            continue;

        g_autoptr(GError) error = nullptr;
        g_autoptr(GPtrArray) remoteList = flatpak_installation_list_remotes(installation, nullptr, &error);
        if (!remoteList) {
            if (error)
                qWarning() << "FlatpakInstallationService: list remotes failed:" << error->message;
            continue;
        }

        for (guint i = 0; i < remoteList->len; ++i) {
            auto *remote = FLATPAK_REMOTE(g_ptr_array_index(remoteList, i));
            const char *name = flatpak_remote_get_name(remote);
            if (!name)
                continue;
            const QString remoteName = QString::fromUtf8(name);
            if (FlatpakRemoteCatalog::isBundleInstallOrigin(remoteName))
                continue;
            bool alreadyListed = false;
            for (const auto &existing : remotes) {
                if (existing.first == remoteName) {
                    alreadyListed = true;
                    break;
                }
            }
            if (alreadyListed)
                continue;

            g_autofree char *url = flatpak_remote_get_url(remote);
            remotes.append(qMakePair(remoteName, url ? QString::fromUtf8(url) : QString()));
        }
    }
    return remotes;
}

QVector<AppInfo> FlatpakInstallationService::searchAppsOnInstallation(FlatpakInstallation *installation,
                                                                      const QString &query,
                                                                      const QString &remoteLabel)
{
    QVector<AppInfo> apps;
    const QString trimmedQuery = query.trimmed();
    if (!installation || trimmedQuery.isEmpty())
        return apps;

    g_autoptr(GError) error = nullptr;
    g_autoptr(GPtrArray) remotes = flatpak_installation_list_remotes(installation, nullptr, &error);
    if (!remotes) {
        if (error)
            qWarning() << "FlatpakInstallationService: list remotes for search failed:" << error->message;
        return apps;
    }

    const QString queryLower = trimmedQuery.toLower();
    QSet<QString> seen;

    for (guint remoteIndex = 0; remoteIndex < remotes->len; ++remoteIndex) {
        auto *remote = FLATPAK_REMOTE(g_ptr_array_index(remotes, remoteIndex));
        const char *remoteName = flatpak_remote_get_name(remote);
        if (!remoteName)
            continue;

        const QString remoteNameStr = QString::fromUtf8(remoteName);
        if (FlatpakRemoteCatalog::isBundleInstallOrigin(remoteNameStr))
            continue;
        for (const AppInfo &app : listRemoteAppsOnInstallation(installation, remoteNameStr)) {
            if (seen.contains(app.id))
                continue;

            const QString idLower = app.id.toLower();
            const QString nameLower = app.name.toLower();
            const QString summaryLower = app.summary.toLower();
            if (!idLower.contains(queryLower)
                    && !nameLower.contains(queryLower)
                    && !summaryLower.contains(queryLower)) {
                continue;
            }

            AppInfo match = app;
            if (!remoteLabel.isEmpty())
                match.repoId = remoteLabel;
            seen.insert(match.id);
            apps.append(match);
        }
    }
    return apps;
}

QVector<AppInfo> FlatpakInstallationService::searchApps(const QString &query)
{
    QMutexLocker lock(&m_mutex);
    QVector<AppInfo> merged;
    QSet<QString> seen;

    const struct {
        FlatpakInstallation *installation;
        QString label;
    } sources[] = {
        {m_userSilent, QStringLiteral("flathub")},
        {m_systemSilent, QStringLiteral("flathub")},
    };

    for (const auto &source : sources) {
        for (const AppInfo &app : searchAppsOnInstallation(source.installation, query, source.label)) {
            if (seen.contains(app.id))
                continue;
            seen.insert(app.id);
            merged.append(app);
        }
    }
    return merged;
}

QVector<AppInfo> FlatpakInstallationService::listRemoteAppsOnInstallation(FlatpakInstallation *installation,
                                                                            const QString &remoteName)
{
    QVector<AppInfo> apps;
    if (!installation)
        return apps;

    const QString normalizedRemote = remoteName.trimmed();
    if (normalizedRemote.isEmpty() || !remoteExistsOnInstallation(installation, normalizedRemote))
        return apps;

    g_autoptr(GError) error = nullptr;
    g_autoptr(GPtrArray) refs = flatpak_installation_list_remote_refs_sync(
            installation,
            normalizedRemote.toUtf8().constData(),
            nullptr,
            &error);
    if (!refs) {
        if (error)
            qWarning() << "FlatpakInstallationService: remote-ls failed:" << error->message;
        return apps;
    }

    for (guint i = 0; i < refs->len; ++i) {
        auto *ref = FLATPAK_REMOTE_REF(g_ptr_array_index(refs, i));
        if (flatpak_ref_get_kind(FLATPAK_REF(ref)) != FLATPAK_REF_KIND_APP)
            continue;
        AppInfo info = FlatpakRefMapper::appFromRemoteRef(ref, normalizedRemote);
        if (!info.id.isEmpty())
            apps.append(info);
    }
    return apps;
}

QVector<AppInfo> FlatpakInstallationService::listRemoteApps(const QString &remoteName)
{
    QMutexLocker lock(&m_mutex);
    const QString normalized = remoteName.trimmed();
    if (normalized.isEmpty())
        return {};

    QVector<AppInfo> apps;
    QSet<QString> seen;

    // Prefer system installation; user scope often has no Flathub remote configured.
    FlatpakInstallation *installations[] = {m_systemSilent, m_userSilent};
    for (FlatpakInstallation *installation : installations) {
        for (const AppInfo &app : listRemoteAppsOnInstallation(installation, normalized)) {
            if (seen.contains(app.id))
                continue;
            seen.insert(app.id);
            apps.append(app);
        }
    }
    return apps;
}

FlatpakScope FlatpakInstallationService::scopeForAppId(const QString &appId) const
{
    const QString trimmed = appId.trimmed();
    {
        QMutexLocker lock(&m_mutex);
        if (m_scopeByAppId.contains(trimmed))
            return m_scopeByAppId.value(trimmed);
    }
    if (!installedRefStringsForAppId(trimmed, FlatpakScope::User).isEmpty())
        return FlatpakScope::User;
    if (!installedRefStringsForAppId(trimmed, FlatpakScope::System).isEmpty())
        return FlatpakScope::System;
    return FlatpakScope::User;
}

QString FlatpakInstallationService::refForAppId(const QString &appId, FlatpakScope scope) const
{
    QMutexLocker lock(&m_mutex);
    const QString trimmed = appId.trimmed();
    if (m_refByAppId.contains(trimmed))
        return m_refByAppId.value(trimmed);

    const QStringList refs = installedRefStringsForAppId(trimmed, scope);
    return refs.isEmpty() ? QString() : refs.first();
}

QString FlatpakInstallationService::bundleFormattedRef(const QString &bundlePath) const
{
    const QString path = bundlePath.trimmed();
    if (path.isEmpty())
        return QString();

    QMutexLocker lock(&m_mutex);
    g_autoptr(GFile) file = g_file_new_for_path(path.toUtf8().constData());
    g_autoptr(FlatpakBundleRef) bundleRef = flatpak_bundle_ref_new(file, nullptr);
    if (!bundleRef)
        return QString();
    return FlatpakRefMapper::refString(FLATPAK_REF(bundleRef));
}

QStringList FlatpakInstallationService::installedRefStringsForAppId(const QString &appId,
                                                                    FlatpakScope scope) const
{
    QStringList refs;
    const QString trimmed = appId.trimmed();
    if (trimmed.isEmpty())
        return refs;

    QMutexLocker lock(&m_mutex);
    FlatpakInstallation *installation = silentInstallation(scope);
    if (!installation)
        return refs;

    g_autoptr(GError) error = nullptr;
    g_autoptr(GPtrArray) installedRefs =
            flatpak_installation_list_installed_refs(installation, nullptr, &error);
    if (!installedRefs)
        return refs;

    const QByteArray appIdBytes = trimmed.toUtf8();
    for (guint i = 0; i < installedRefs->len; ++i) {
        auto *installedRef = FLATPAK_INSTALLED_REF(g_ptr_array_index(installedRefs, i));
        if (flatpak_ref_get_kind(FLATPAK_REF(installedRef)) != FLATPAK_REF_KIND_APP)
            continue;
        const char *name = flatpak_ref_get_name(FLATPAK_REF(installedRef));
        if (!name || g_strcmp0(name, appIdBytes.constData()) != 0)
            continue;
        const QString ref = FlatpakRefMapper::refString(FLATPAK_REF(installedRef));
        if (!ref.isEmpty())
            refs.append(ref);
    }
    return refs;
}

bool FlatpakInstallationService::addRemoteUser(const QString &name, const QString &url, QString *errorOut)
{
    QMutexLocker lock(&m_mutex);
    FlatpakInstallation *installation = interactiveInstallation(FlatpakScope::User);
    if (!installation) {
        if (errorOut)
            *errorOut = QStringLiteral("User Flatpak installation is unavailable");
        return false;
    }

    g_autoptr(GError) error = nullptr;
    g_autoptr(FlatpakRemote) remote = flatpak_remote_new(name.toUtf8().constData());
    flatpak_remote_set_url(remote, url.toUtf8().constData());
    if (!flatpak_installation_add_remote(installation, remote, TRUE, nullptr, &error)) {
        setGErrorString(error, errorOut);
        return false;
    }
    return true;
}

bool FlatpakInstallationService::removeRemoteUser(const QString &name, QString *errorOut)
{
    QMutexLocker lock(&m_mutex);
    FlatpakInstallation *installation = interactiveInstallation(FlatpakScope::User);
    if (!installation) {
        if (errorOut)
            *errorOut = QStringLiteral("User Flatpak installation is unavailable");
        return false;
    }

    g_autoptr(GError) error = nullptr;
    if (!flatpak_installation_remove_remote(installation, name.toUtf8().constData(), nullptr, &error)) {
        setGErrorString(error, errorOut);
        return false;
    }
    return true;
}

bool FlatpakInstallationService::hasRemote(FlatpakScope scope, const QString &remoteName) const
{
    QMutexLocker lock(&m_mutex);
    return remoteExistsOnInstallation(silentInstallation(scope), remoteName);
}

bool FlatpakInstallationService::addRemoteFromRepoFile(FlatpakScope scope,
                                                       const QString &remoteName,
                                                       const QByteArray &flatpakRepoData,
                                                       QString *errorOut)
{
    if (flatpakRepoData.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("Flatpak repo file is empty");
        return false;
    }

    const QString normalizedName = remoteName.trimmed();
    if (normalizedName.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("Remote name is empty");
        return false;
    }

    QMutexLocker lock(&m_mutex);
    FlatpakInstallation *installation = interactiveInstallation(scope);
    if (!installation) {
        if (errorOut)
            *errorOut = QStringLiteral("Flatpak installation (%1) is unavailable")
                                .arg(flatpakScopeToString(scope));
        return false;
    }

    if (remoteExistsOnInstallation(installation, normalizedName))
        return true;

    g_autoptr(GError) error = nullptr;
    g_autoptr(GBytes) bytes = g_bytes_new(flatpakRepoData.constData(), flatpakRepoData.size());
    g_autoptr(FlatpakRemote) remote = flatpak_remote_new_from_file(
            normalizedName.toUtf8().constData(),
            bytes,
            &error);
    if (!remote) {
        setGErrorString(error, errorOut);
        return false;
    }

    g_autoptr(GKeyFile) keyFile = g_key_file_new();
    if (g_key_file_load_from_bytes(keyFile, bytes, G_KEY_FILE_NONE, nullptr)) {
        g_autofree char *gpgKeyBase64 = g_key_file_get_string(keyFile, "Flatpak Repo", "GPGKey", nullptr);
        if (gpgKeyBase64) {
            gsize gpgKeyLen = 0;
            g_autofree guchar *gpgKeyData = g_base64_decode(gpgKeyBase64, &gpgKeyLen);
            if (gpgKeyData) {
                g_autoptr(GBytes) gpgKeyBytes = g_bytes_new(gpgKeyData, gpgKeyLen);
                flatpak_remote_set_gpg_verify(remote, TRUE);
                flatpak_remote_set_gpg_key(remote, gpgKeyBytes);
            }
        }
    }

    if (!flatpak_installation_add_remote(installation, remote, TRUE, nullptr, &error)) {
        setGErrorString(error, errorOut);
        return false;
    }
    return true;
}

bool FlatpakInstallationService::ensureRuntimeRemoteForBundle(FlatpakScope scope,
                                                              const QString &bundlePath,
                                                              const QByteArray &flatpakRepoData,
                                                              QString *errorOut)
{
    const QString path = bundlePath.trimmed();
    if (path.isEmpty())
        return true;

    QString remoteName;
    {
        QMutexLocker lock(&m_mutex);
        FlatpakInstallation *installation = silentInstallation(scope);
        if (!installation)
            return true;

        g_autoptr(GFile) file = g_file_new_for_path(path.toUtf8().constData());
        g_autoptr(FlatpakBundleRef) bundleRef = flatpak_bundle_ref_new(file, nullptr);
        if (!bundleRef)
            return true;

        g_autofree char *runtimeRepoUrl = flatpak_bundle_ref_get_runtime_repo_url(bundleRef);
        if (!runtimeRepoUrl || runtimeRepoUrl[0] == '\0')
            return true;

        remoteName = QString::fromUtf8(runtimeRepoUrl);
        const int slash = remoteName.lastIndexOf(QLatin1Char('/'));
        if (slash >= 0)
            remoteName = remoteName.mid(slash + 1);
        if (remoteName.endsWith(QStringLiteral(".flatpakrepo"), Qt::CaseInsensitive))
            remoteName.chop(QStringLiteral(".flatpakrepo").size());

        if (remoteName.isEmpty() || remoteExistsOnInstallation(installation, remoteName))
            return true;
    }

    if (flatpakRepoData.isEmpty()) {
        if (errorOut) {
            *errorOut = QStringLiteral("Runtime remote %1 is required but could not be fetched")
                                .arg(remoteName);
        }
        return false;
    }

    return addRemoteFromRepoFile(scope, remoteName, flatpakRepoData, errorOut);
}

QString FlatpakInstallationService::bundleRuntimeRepoUrl(const QString &bundlePath) const
{
    const QString path = bundlePath.trimmed();
    if (path.isEmpty())
        return QString();

    QMutexLocker lock(&m_mutex);
    g_autoptr(GFile) file = g_file_new_for_path(path.toUtf8().constData());
    g_autoptr(FlatpakBundleRef) bundleRef = flatpak_bundle_ref_new(file, nullptr);
    if (!bundleRef)
        return QString();

    g_autofree char *runtimeRepoUrl = flatpak_bundle_ref_get_runtime_repo_url(bundleRef);
    return runtimeRepoUrl ? QString::fromUtf8(runtimeRepoUrl) : QString();
}
