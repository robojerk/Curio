#pragma once

#include "models/AppInfo.h"
#include "FlatpakScope.h"

struct _FlatpakInstalledRef;
struct _FlatpakRef;
struct _FlatpakRemoteRef;
typedef struct _FlatpakInstalledRef FlatpakInstalledRef;
typedef struct _FlatpakRef FlatpakRef;
typedef struct _FlatpakRemoteRef FlatpakRemoteRef;

class FlatpakRefMapper
{
public:
    static AppInfo appFromInstalledRef(FlatpakInstalledRef *ref, FlatpakScope scope);
    static AppInfo appFromRemoteRef(FlatpakRemoteRef *ref, const QString &remoteName);
    static AppInfo appFromSearchRef(FlatpakRef *ref, const QString &remoteName);
    static QString refString(FlatpakRef *ref);
    /** Resolve icon file under Flatpak exports or deploy dir (bundle installs included). */
    static QString installedIconFilePath(const QString &appId);
    static QString installedIconName(const QString &appId);
};
