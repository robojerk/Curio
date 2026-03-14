# Curio

A GUI Flatpak store implemented in C++ and Qt 6.

This project is inspired by Bazaar, but it is a separate Qt application.

## Features (MVP)

- Browse installed Flatpak applications.
- Search for applications via `flatpak search`.
- Install, remove, and update applications from a simple Qt Widgets UI.
- View a basic operations list for recent actions.

## Building

Requirements:

- C++17-capable compiler
- CMake ≥ 3.16
- Qt 6 (Widgets, Network modules)
- `flatpak` CLI available in `PATH`
- **Optional:** AppStream Qt (`appstream-qt` on Arch) for rich app metadata (long descriptions, screenshots). If not found, the app builds and runs with CLI-only data. On Arch, the package does not ship pkg-config; CMake must find `AppStreamQt`. If detection fails, check the target name: `grep -E 'add_library|ALIAS' /usr/lib/cmake/AppStreamQt/*.cmake`.

Configure and build:

```bash
cmake -S . -B build
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

## Notes

- This project uses the Flatpak CLI (`flatpak`) via `QProcess` rather than libflatpak directly.
- Curio is a standalone GUI Flatpak store.

