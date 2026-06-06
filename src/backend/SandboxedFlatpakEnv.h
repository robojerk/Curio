#pragma once

#include <QString>

/** Host-facing flatpak/bwrap setup when Curio runs inside its own Flatpak sandbox. */
namespace SandboxedFlatpakEnv {

/** Set FLATPAK_BWRAP, FLATPAK_BINARY, etc. when running inside Curio's Flatpak. */
void configure();

/** flatpak CLI for QProcess: host wrapper in sandbox, plain name on host. */
QString flatpakExecutable();

/** Host flatpak path for exported .desktop Exec lines (e.g. /usr/bin/flatpak). */
QString hostFlatpakBinary();

/** Rewrite /app/bin/flatpak in user export .desktop files after sandboxed installs. */
void repairExportedDesktopExecPaths();

} // namespace SandboxedFlatpakEnv
