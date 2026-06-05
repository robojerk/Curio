#pragma once

#include <QString>

/** Host-facing flatpak/bwrap setup when Curio runs inside its own Flatpak sandbox. */
namespace SandboxedFlatpakEnv {

/** Set FLATPAK_BWRAP (and related) so libflatpak uses host bubblewrap. */
void configure();

/** flatpak CLI for QProcess: host wrapper in sandbox, plain name on host. */
QString flatpakExecutable();

} // namespace SandboxedFlatpakEnv
