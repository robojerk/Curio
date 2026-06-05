#pragma once

#include "FlatpakScope.h"
#include "models/AppInfo.h"

#include <QHash>
#include <QStringList>
#include <QRecursiveMutex>
#include <QPair>
#include <QVector>

struct _FlatpakInstallation;
typedef struct _FlatpakInstallation FlatpakInstallation;

class FlatpakInstallationService
{
public:
    FlatpakInstallationService();
    ~FlatpakInstallationService();

    FlatpakInstallationService(const FlatpakInstallationService &) = delete;
    FlatpakInstallationService &operator=(const FlatpakInstallationService &) = delete;

    bool isValid() const { return m_valid; }
    QString lastError() const { return m_lastError; }

    FlatpakInstallation *silentInstallation(FlatpakScope scope) const;
    FlatpakInstallation *interactiveInstallation(FlatpakScope scope) const;

    QVector<AppInfo> listInstalledApps();
    /** App IDs with newer versions on their configured remotes (libflatpak update scan). */
    QVector<AppInfo> listAvailableUpdates();
    /** Installed runtimes/SDKs with available updates (libflatpak update scan). */
    QVector<AppInfo> listAvailableRuntimeUpdates();
    QVector<QPair<QString, QString>> listRemotes();
    QVector<AppInfo> searchApps(const QString &query);
    QVector<AppInfo> listRemoteApps(const QString &remoteName);

    FlatpakScope scopeForAppId(const QString &appId) const;
    QString refForAppId(const QString &appId, FlatpakScope scope) const;
    /** Full ref string embedded in a .flatpak bundle (e.g. app/id/arch/stable). */
    QString bundleFormattedRef(const QString &bundlePath) const;
    /** All installed app refs for @p appId on the given scope (branch/arch variants). */
    QStringList installedRefStringsForAppId(const QString &appId, FlatpakScope scope) const;

    bool addRemoteUser(const QString &name, const QString &url, QString *errorOut = nullptr);
    bool removeRemoteUser(const QString &name, QString *errorOut = nullptr);
    /** Add a remote from a .flatpakrepo file payload on the given scope. */
    bool addRemoteFromRepoFile(FlatpakScope scope,
                               const QString &remoteName,
                               const QByteArray &flatpakRepoData,
                               QString *errorOut = nullptr);
    bool hasRemote(FlatpakScope scope, const QString &remoteName) const;
    /** Ensure runtime repo from bundle metadata exists (no-op when already present). */
    bool ensureRuntimeRemoteForBundle(FlatpakScope scope,
                                      const QString &bundlePath,
                                      const QByteArray &flatpakRepoData,
                                      QString *errorOut = nullptr);
    QString bundleRuntimeRepoUrl(const QString &bundlePath) const;

private:
    void openInstallationPair(FlatpakScope scope,
                              FlatpakInstallation **silentOut,
                              FlatpakInstallation **interactiveOut);
    bool remoteExistsOnInstallation(FlatpakInstallation *installation,
                                    const QString &remoteName) const;
    QVector<AppInfo> listInstalledAppsForScope(FlatpakScope scope);
    QVector<AppInfo> listAvailableUpdatesForScope(FlatpakScope scope);
    QVector<AppInfo> listAvailableRuntimeUpdatesForScope(
            FlatpakScope scope, const QHash<QString, QString> &newestInstalledBranches);
    QVector<AppInfo> searchAppsOnInstallation(FlatpakInstallation *installation,
                                              const QString &query,
                                              const QString &remoteLabel);
    QVector<AppInfo> listRemoteAppsOnInstallation(FlatpakInstallation *installation,
                                                  const QString &remoteName);

    FlatpakInstallation *m_systemSilent = nullptr;
    FlatpakInstallation *m_systemInteractive = nullptr;
    FlatpakInstallation *m_userSilent = nullptr;
    FlatpakInstallation *m_userInteractive = nullptr;

    QHash<QString, FlatpakScope> m_scopeByAppId;
    QHash<QString, QString> m_refByAppId;
    mutable QRecursiveMutex m_mutex;
    bool m_valid = false;
    QString m_lastError;
};
