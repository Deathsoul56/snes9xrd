# snes9xrd
*Snes9x 1.63 — Portable Super Nintendo Entertainment System (TM) emulator, "sex edition" fork*

This is `snes9xrd`, a personal fork of [Snes9x](https://github.com/snes9xgit/snes9x) 1.63 maintained by DeAtSoUl56. It tracks upstream `master` closely — there's no separate feature branch — but rebuilds the Qt front-end and fixes bugs found along the way. Everything else (the emulation core, and the GTK/win32/macOS/libretro front-ends) is stock Snes9x.

## What's different from upstream

- **Qt front-end overhaul**: Display, Sound, Emulation and Controllers settings now have real parity with the legacy win32 dialogs (stretch/transparency/shader parameters, frame skipping, software filters, per-channel volume/mute, etc.).
- **Controller binding widget**: the on-screen SNES controller image now highlights exactly the button that's bound and pressed — no more pre-lit buttons or mismatched highlights — with a background that matches the settings theme.
- **Binding fixes**: axis/hat bindings are correctly disambiguated from each other and now survive a save/load round-trip; joystick button highlighting matches the same raw button index the bindings are captured from.
- **Simplified main window**: the sidebar added early in the fork's life was removed in favor of just the top menu bar; the Help → About dialog was restored with the fork's own credits.

For the full breakdown of the fork's layout, build systems, and conventions, see [AGENTS.md](AGENTS.md).

## Building

`git submodule update --init --recursive` is required before building anything (see [AGENTS.md](AGENTS.md#submodules) for the exact list of submodules).

### Qt (recommended for this fork)
```
cmake -G Ninja -B build -S qt -DCMAKE_BUILD_TYPE=Release
ninja -C build
```
On Windows, build from an MSYS2 CLANG64 shell. Qt6 and SDL3 are fetched automatically via CMake if not found on the system.

### Other front-ends (unix/X11, GTK, win32, macOS, libretro)
These are unmodified from upstream Snes9x — see [AGENTS.md](AGENTS.md#build-commands) for the build command for each.

## Upstream project

Please check the [Snes9x Wiki](https://github.com/snes9xgit/snes9x/wiki) for general information about the emulator, and the [upstream repository](https://github.com/snes9xgit/snes9x) for nightly builds and CI status of the base project this fork tracks.
