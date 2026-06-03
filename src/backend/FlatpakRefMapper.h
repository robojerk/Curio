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
};
