#!/usr/bin/env bash
# libflatpak invokes bwrap to create sandboxes; nested bwrap cannot use user
# namespaces inside a Flatpak app. Delegate to the host bwrap via flatpak-spawn.
# apply_extra passes ro-bind sources as /proc/self/fd/N — those fds only exist in
# this process, so use the bundled in-sandbox bwrap when fd binds are detected.
set -euo pipefail

export DBUS_SESSION_BUS_ADDRESS="${DBUS_SESSION_BUS_ADDRESS:-unix:path=/run/flatpak/bus}"

sandbox_bwrap=/app/bin/bwrap
declare -a fd_set=()
use_sandbox_bwrap=false

add_fd() {
    local n=$1
    [[ "$n" =~ ^[0-9]+$ ]] || return 0
    for existing in "${fd_set[@]}"; do
        [[ "$existing" == "$n" ]] && return 0
    done
    fd_set+=("$n")
}

scan_text() {
    local text=$1
    if grep -q '/proc/self/fd/' <<< "$text"; then
        use_sandbox_bwrap=true
    fi
    if grep -q 'apply_extra' <<< "$text"; then
        use_sandbox_bwrap=true
    fi
    local match
    while IFS= read -r match; do
        add_fd "${match#/proc/self/fd/}"
    done < <(grep -oE '/proc/self/fd/[0-9]+' <<< "$text" || true)
}

scan_args_fd() {
    local arg_fd=$1
    add_fd "$arg_fd"
    local scan_file
    scan_file=$(mktemp)
    if cp "/proc/self/fd/${arg_fd}" "$scan_file" 2>/dev/null && [[ -s "$scan_file" ]]; then
        scan_text "$(tr '\0' ' ' < "$scan_file")"
    fi
    rm -f "$scan_file"
}

scan_argv() {
    local -a argv=("$@")
    local i=0
    while (( i < ${#argv[@]} )); do
        local arg=${argv[i]}
        if [[ "$arg" == --args ]] && (( i + 1 < ${#argv[@]} )); then
            scan_args_fd "${argv[i+1]}"
            ((i += 2))
            continue
        fi
        scan_text "$arg"
        ((++i))
    done
}

scan_argv "$@"

if [[ "$use_sandbox_bwrap" == true && -x "$sandbox_bwrap" ]]; then
    exec "$sandbox_bwrap" "$@"
fi

spawn=(flatpak-spawn --host)
for fd in "${fd_set[@]}"; do
    spawn+=(--forward-fd="$fd")
done
spawn+=(bwrap "$@")
exec "${spawn[@]}"
