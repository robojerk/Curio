#include "SandboxedFlatpakEnv.h"

#include <QFileInfo>
#include <QProcessEnvironment>
#include <QtGlobal>

namespace {

bool sandboxedBuildEnabled()
{
#ifdef CURIO_SANDBOXED_LIBFLATPAK
    return true;
#else
    return false;
#endif
}

QString appBinaryPath(const char *name)
{
    return QStringLiteral("/app/bin/") + QString::fromUtf8(name);
}

} // namespace

namespace SandboxedFlatpakEnv {

void configure()
{
    if (!sandboxedBuildEnabled())
        return;

    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

    const QString bwrapWrapper = appBinaryPath("bwrap-host-wrapper");
    if (QFileInfo::exists(bwrapWrapper))
        qputenv("FLATPAK_BWRAP", bwrapWrapper.toLocal8Bit().constData());

    if (!env.contains(QStringLiteral("DBUS_SESSION_BUS_ADDRESS"))) {
        qputenv("DBUS_SESSION_BUS_ADDRESS",
                "unix:path=/run/flatpak/bus");
    }
}

QString flatpakExecutable()
{
    if (!sandboxedBuildEnabled())
        return QStringLiteral("flatpak");

    const QString wrapper = appBinaryPath("flatpak-host-wrapper");
    if (QFileInfo::exists(wrapper))
        return wrapper;

    return QStringLiteral("flatpak");
}

} // namespace SandboxedFlatpakEnv
