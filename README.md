# Curio

A GUI Flatpak store implemented in C++ and Qt 6.

This project is inspired by Bazaar, but it is a separate Qt application. It also includes the ability to track flatpak packages on sites like github and update.

It's an experiment.

## Features 

- Browse installed Flatpak applications.
- Search for applications via `flatpak search`.
- View a basic operations list for recent actions.

## Building

Requirements:

- C++17-capable compiler
- CMake ≥ 3.16
- Qt 6 (Widgets, Network, Concurrent modules)
- **libflatpak** development package (`flatpak` / `libflatpak-dev` on Debian; `flatpak` on Arch provides pkg-config)
- **Optional:** AppStream Qt (`appstream-qt` on Arch) for rich app metadata (long descriptions, screenshots). If not found, the app builds and runs with libflatpak data only. On Arch, the package does not ship pkg-config; CMake must find `AppStreamQt`. If detection fails, check the target name: `grep -E 'add_library|ALIAS' /usr/lib/cmake/AppStreamQt/*.cmake`.

**Recommended workflow:** build and run Curio as a Flatpak (see [Releases (Flatpak)](#releases-flatpak)). Native builds are supported for development against the host libflatpak.

Configure and build (native dev):

```bash
cmake -S . -B build -DCURIO_SANDBOXED_LIBFLATPAK=OFF
cmake --build build
```

Run:

```bash
./build/curio
```

To install into `/usr/local/bin` (or your chosen prefix):

```bash
cmake --install build
```

## Releases (Flatpak)

CI builds a Flatpak bundle **only when a GitHub Release is published** (planned pre-releases or full releases). Pushes and pull requests do not build.

For the first planned pre-release (`alpha_1`):

1. Push your changes to the default branch.
2. Create a tag (for example `alpha_1`) pointing at that commit.
3. On GitHub: **Releases → Draft a new release**, choose tag `alpha_1`, check **Set as a pre-release**, then **Publish release**.
4. The workflow uploads to that release:
   - `io.github.curio.Curio-x86_64.flatpak`
   - `io.github.curio.Curio-x86_64.flatpak.sha256`

Install from a release asset:

```bash
flatpak install --user --bundle io.github.curio.Curio-x86_64.flatpak
```

The bundle is built for **x86_64** today. **aarch64** is reserved in the workflow matrix for a later enablement.

### Test the workflow with act (Podman)

[act](https://github.com/nektos/act) uses the Docker API; Podman provides that via a socket (no Docker install needed).

**Easiest:** run the helper script (bash — works from fish too):

```bash
./scripts/act-flatpak-release.sh
```

Enable a Podman API socket once. Your system unit listens on `/run/podman/podman.sock`; the user unit uses `$XDG_RUNTIME_DIR/podman/podman.sock`:

```bash
# rootless (typical)
systemctl --user enable --now podman.socket

# or rootful system socket
sudo systemctl enable --now podman.socket
```

Check the socket: `podman ps` should succeed without errors.

**Bash** — `export` must be on its own line (or use `env` on one line). Do not paste `export … act release …` as a single command:

```bash
export DOCKER_HOST="unix://${XDG_RUNTIME_DIR}/podman/podman.sock"
act release \
  -e .github/events/release-published.json \
  -W .github/workflows/flatpak-build.yml \
  -P ubuntu-latest=ghcr.io/flathub-infra/flatpak-github-actions:kde-6.9 \
  --container-options "--privileged"
```

One-liner (bash):

```bash
DOCKER_HOST="unix://${XDG_RUNTIME_DIR}/podman/podman.sock" act release \
  -e .github/events/release-published.json \
  -W .github/workflows/flatpak-build.yml \
  -P ubuntu-latest=ghcr.io/flathub-infra/flatpak-github-actions:kde-6.9 \
  --container-options "--privileged"
```

**Fish** — no `${VAR}`; use `$XDG_RUNTIME_DIR`:

```fish
set -gx DOCKER_HOST unix://$XDG_RUNTIME_DIR/podman/podman.sock
act release \
  -e .github/events/release-published.json \
  -W .github/workflows/flatpak-build.yml \
  -P ubuntu-latest=ghcr.io/flathub-infra/flatpak-github-actions:kde-6.9 \
  --container-options "--privileged"
```

Rootless Podman may need extra capabilities for `flatpak-builder` (namespaces). If the build fails inside act, re-run with `--privileged` as above, or use the rootful system socket.

## Notes

- Curio uses **libflatpak** (not the `flatpak` CLI) for listing, searching, and installing applications. When run inside its own Flatpak sandbox, it talks to `org.freedesktop.Flatpak.SystemHelper` for system-scope changes (one PolicyKit prompt per transaction).
- Curio is a standalone GUI Flatpak store distributed via GitHub releases.


