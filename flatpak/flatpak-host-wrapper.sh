#!/bin/sh
# Run the host flatpak CLI so installs and "flatpak run" use the real user/system
# installations instead of the in-sandbox flatpak binary.
set -eu
export DBUS_SESSION_BUS_ADDRESS="${DBUS_SESSION_BUS_ADDRESS:-unix:path=/run/flatpak/bus}"
exec flatpak-spawn --host flatpak "$@"
