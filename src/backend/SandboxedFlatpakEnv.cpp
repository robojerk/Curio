#include "SandboxedFlatpakEnv.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QTextStream>
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

QString discoverHostFlatpakBinary()
{
    QProcess proc;
    proc.start(QStringLiteral("flatpak-spawn"),
               {QStringLiteral("--host"),
                QStringLiteral("sh"),
                QStringLiteral("-c"),
                QStringLiteral("command -v flatpak")});
    if (proc.waitForFinished(5000) && proc.exitCode() == 0) {
        const QString path = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        if (!path.isEmpty())
            return path;
    }
    return QStringLiteral("/usr/bin/flatpak");
}

QString &hostFlatpakBinaryCache()
{
    static QString cached;
    return cached;
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

    const QString hostFlatpak = hostFlatpakBinary();
    qputenv("FLATPAK_BINARY", hostFlatpak.toLocal8Bit().constData());

    repairExportedDesktopExecPaths();
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

QString hostFlatpakBinary()
{
    if (!sandboxedBuildEnabled())
        return QStringLiteral("flatpak");

    QString &cached = hostFlatpakBinaryCache();
    if (cached.isEmpty())
        cached = discoverHostFlatpakBinary();
    return cached;
}

QString userFlatpakDataDir()
{
    if (sandboxedBuildEnabled())
        return QDir::homePath() + QStringLiteral("/.local/share/flatpak");
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
            + QStringLiteral("/flatpak");
}

void repairExportedDesktopExecPaths()
{
    if (!sandboxedBuildEnabled())
        return;

    const QString hostFlatpak = hostFlatpakBinary();
    const QString wrongFlatpak = appBinaryPath("flatpak");
    if (hostFlatpak == wrongFlatpak)
        return;

    const QString exportDir = userFlatpakDataDir()
            + QStringLiteral("/exports/share/applications");
    const QDir dir(exportDir);
    if (!dir.exists())
        return;

    const QFileInfoList desktops = dir.entryInfoList({QStringLiteral("*.desktop")}, QDir::Files);
    for (const QFileInfo &desktopInfo : desktops) {
        QFile file(desktopInfo.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;

        QStringList lines;
        bool changed = false;
        while (!file.atEnd()) {
            QString line = QString::fromUtf8(file.readLine());
            if (line.startsWith(QStringLiteral("Exec="))
                    && line.contains(wrongFlatpak)) {
                line.replace(wrongFlatpak, hostFlatpak);
                changed = true;
            }
            lines.append(line);
        }
        file.close();

        if (!changed)
            continue;

        if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
            continue;

        QTextStream out(&file);
        for (const QString &line : lines)
            out << line;
    }
}

} // namespace SandboxedFlatpakEnv
