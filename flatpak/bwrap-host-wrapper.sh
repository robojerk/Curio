#!/bin/sh
# libflatpak invokes bwrap to create sandboxes; nested bwrap cannot use user
# namespaces inside a Flatpak app. Delegate to the host bwrap via flatpak-spawn.
set -eu
export DBUS_SESSION_BUS_ADDRESS="${DBUS_SESSION_BUS_ADDRESS:-unix:path=/run/flatpak/bus}"
exec flatpak-spawn --host bwrap "$@"
