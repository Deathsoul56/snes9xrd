# AGENTS.md

Snes9x 1.63 — portable Super Nintendo emulator. This is a fork (`Deathsoul56/snes9xrd`, the `rd` is the fork author's suffix; upstream is `snes9xgit/snes9x` 1.63). The fork tracks upstream `master` closely; there is no separate `rd` feature branch and no fork-specific code to learn first.

The repo contains **six independent front-ends** that all link against one shared C++ emulation core at the repository root.

## Layout

- Root (`*.cpp`, `*.h`): the **emulation core**. Entry points in `snes9x.cpp` / `snes9x.h`; memory map in `memmap.cpp`; CPU in `cpu.cpp`, `cpuexec.cpp`, `cpuops.cpp`; PPU in `ppu.cpp`; APU in `apu/`; cartridge coprocessors (`sa1`, `c4`, `dsp1-4`, `bsx`, `sdd1`, `spc7110`, `obc1`, `seta*`, `msu1`, `crtc`, `srtc`) each in their own file; save states in `snapshot.cpp`; cheats in `cheats.cpp`/`cheats2.cpp`; movies in `movie.cpp`; freeze-file rewind in `statemanager.cpp`/`statemanager.h`. This is **the only directory most porting/bug work happens in**.
- `unix/`: X11 port, autotools (`configure.ac`, `Makefile.in`, committed `configure` script). Entry: `unix.cpp` (`main`).
- `gtk/`: GTK3/gtkmm port, CMake (`gtk/CMakeLists.txt`). UI in `gtk/src/`; needs `gtk-po/` translations + `snes9x.ui`.
- `qt/`: Qt6 port, CMake (`qt/CMakeLists.txt`). UI in `qt/src/`; needs prebuilt Qt6 (`qt/scripts/cmake-qt-*.sh` builds Qt6 from source when needed).
- `win32/`: Win32 (Win32 API) port, Visual Studio (`win32/snes9xw.sln` + `win32/snes9xw.vcxproj`, configurations `Debug|Release Unicode` × `Win32|x64`).
- `macosx/`: macOS Cocoa port, Xcode (`macosx/snes9x.xcodeproj`).
- `libretro/`: libretro core (`libretro/libretro.cpp` defines `retro_*` callbacks; `libretro/Makefile` builds for many platforms; `libretro/libretro-win32.vcxproj` for Windows DLL).
- `apu/bapu/`: APU DSP/SMP cores (used by every front-end).
- `filter/`: software scalers (2xsai, epx, hq2x, xbrz, snes_ntsc).
- `jma/`: JMA archive support (built as a `jma` library by GTK and as object files elsewhere).
- `common/`: shared backends used across front-ends — `common/audio/s9x_sound_driver*.cpp` (ALSA/Pulse/PortAudio/OSS/SDL/SDL3/cubeb) and `common/video/opengl`, `common/video/vulkan`, `common/video/wayland`.
- `external/`: vendored libs (fmt, imgui, glad, stb, VulkanMemoryAllocator-Hpp) plus git submodules (cubeb, glslang, SPIRV-Cross, vulkan-headers).
- `data/`: runtime data — `cheats.bml` is shipped with every binary.
- `.opencode/`: per-repo agent config — skills (ponytail family), commands, plugins. OpenCode loads these automatically; do not edit unless you're extending them.

## Submodules

`git submodule update --init --recursive` is **required** before building anything. Required submodules (`.gitmodules`):

- `external/SPIRV-Cross` — used by GTK/Qt for Vulkan `.slangp` shaders
- `external/glslang` — used by GTK/Qt for Vulkan pipeline compile
- `external/vulkan-headers` — Vulkan API headers
- `external/cubeb` — used by Qt for audio (`qt/CMakeLists.txt` adds it)
- `win32/libpng/src`, `win32/zlib/src` — Win32 dependency libs

`external/fmt`, `external/glad`, `external/imgui`, `external/stb`, `external/VulkanMemoryAllocator-Hpp` are **vendored copies**, not submodules.

## Build commands

### Unix/X11 (autotools)
```
git submodule update --init --recursive
cd unix && touch configure && ./configure && make -j$(nproc)
```
- The committed `unix/configure` is generated; **`touch configure` is required** (regenerates timestamps; `configure.ac` is the source).
- Notable flags: `--enable-debugger`, `--enable-netplay`, `--enable-gzip`, `--enable-zip`, `--with-system-zip`, `--enable-jma`, `--enable-screenshot`, `--enable-sse41`, `--enable-avx2`, `--enable-neon`, `--enable-mtune=<value>`. Run `unix/configure --help` for the full list.
- Produces `unix/snes9x`.
- Rewind is `-rwbuffersize` (MB) and `-rwgranularity` (frames); default buffer is 0 (disabled). See `unix/unix.cpp:381-498,1820-1905`.

### GTK (Linux)
```
git submodule update --init --recursive
cmake -G Ninja -B build -S gtk -DCMAKE_BUILD_TYPE=Release
ninja -j$(nproc) -C build
```
Requires `pkg-config` for: `sdl2`, `gtkmm-3.0`, `gthread-2.0`, `libpng`, `xv` (optional), `wayland-client wayland-egl` (optional), `libpulse`, `portaudio-2.0`, `alsa`, `minizip`. CI uses `apt-get install meson gettext libsdl2-dev libgtkmm-3.0-dev libgtk-3-dev libminizip-dev portaudio19-dev glslang-dev cmake` — see `.cirrus.yml`.
CMake options: `USE_SLANG`, `USE_XV`, `USE_PORTAUDIO`, `USE_ALSA`, `USE_PULSEAUDIO`, `USE_OSS`, `DEBUGGER`, `USE_HQ2X`, `USE_XBRZ`, `USE_SYSTEMZIP`, `USE_WAYLAND`, `DANGEROUS_HACKS`. All default ON; turn off with `-DUSE_X=OFF`.
GTK `.ui` is converted to C++ by `src/sourcify.c` at build time (custom command in `gtk/CMakeLists.txt:373-384`).
PO translations: `gtk/po/updatepot.sh` regenerates `snes9x-gtk.pot`. Add new languages to the `foreach(lang …)` in `gtk/CMakeLists.txt:52`.

### Qt
```
git submodule update --init --recursive
cmake -G Ninja -B build -S qt -DCMAKE_BUILD_TYPE=Release
ninja -j$(nproc) -C build
```
- On Windows, requires Qt6. `qt/CMakeLists.txt:81-95` checks for `external/qt6-mingw-clang-bin` and either uses it or fetches via `FetchContent` from `bearoso/qt6-mingw-clang-bin`. The `qt/scripts/cmake-qt-base.sh` script configures a minimal static Qt6 build.
- Optionally uses system SDL3 (`-DUSE_SYSTEM_SDL3=ON`); otherwise fetches `release-3.4.8` via `FetchContent`.
- Pulls in cubeb, glslang, SPIRV-Cross, vulkan-headers, stb, imgui.
- Always sets `SDL_MAIN_HANDLED`.

### Windows (Win32)
Open `win32/snes9xw.sln` in Visual Studio 2022. Configurations: `Debug|Release Unicode` × `Win32|x64`. Output: `win32/snes9x.exe` and `win32/snes9x-x64.exe`. Build deps (libpng, zlib, glslang, SPIRV, HLSL, OSDependent) are sub-projects in the same solution — they must build first; just `Build Solution` from IDE.
Libretro DLL on Windows: `libretro/libretro-win32.vcxproj`, configurations `libretro Debug|Release` × `Win32|x64`, outputs `libretro/{Win32,x64}/libretro Release/snes9x_libretro.dll`.

### macOS
```
xcodebuild -project macosx/snes9x.xcodeproj -target Snes9x -configuration Release build CODE_SIGN_IDENTITY="" CODE_SIGNING_REQUIRED=NO
```

### libretro core
```
make -j$(nproc) -C libretro                            # auto-detects unix/win/osx
make -j$(nproc) -C libretro platform=unix-armv7-neon-hardfloat
make -j$(nproc) -C libretro platform=wii
make -j$(nproc) -C libretro platform=libnx
```
Known platforms in `libretro/Makefile`: `unix`, `win`, `osx`, `libnx`, `classic_armv7_a7`, `classic_armv8_a35`, ios variants, tvos-arm64, qnx, vita, ps3/sncps3/psl1ght, xenon (Xbox 360), ngc/wii/wiiu, emscripten, xbox1_msvc2003, xbox360_msvc2010, `windows_msvc2010_x64`, `windows_msvc2010_x86`, `windows_msvc2003_x86`, `windows_msvc2005_x86`. Endianness on big-endian targets is set with `-DBLARGG_BIG_ENDIAN=1`.

## Tests, lint, format

There are **no automated tests in this repo** beyond a single hand-rolled binary, `common/video/vulkan/slang_preset_test.cpp`, built only when `USE_SLANG=ON` and invoked manually as `slang_test <shaderpreset>` to introspect a `.slangp`. There is no test runner, no CI test stage, no lint config (no `.clang-format`, no `.clang-tidy`, no `.editorconfig`, no pre-commit), and no formatter the project enforces. Don't add speculative ones.

## Coding rules (this fork)

This is a personal fork; the rules below are the user's directives and override generic "be helpful" defaults. They are non-negotiable.

### 1. Reuse before you write
Before adding a function, struct, or constant, search the repo for an existing one. The same fact duplicated in two places is a future bug farm. **Single source of truth, period.** Examples of what the fork already got wrong and how it was fixed (do not regress):

- `EmuApplication::supportedRomExtensions()` + `EmuApplication::romFileDialogFilter()` are the **only** ROM-extension lists. `QDirIterator` scans, `QFileDialog::nameFilters` opens, both pull from these helpers. Adding a new extension is a one-line change there.
- `BIOS_DIR` is resolved through `Snes9xController::bios_folder`; never hardcode paths in the core's callsites.

If you need a value that's already exposed elsewhere, expose it via the existing helper. If a helper does not exist, add it once and use it from every callsite.

### 2. Don't add front-end code the core already has
The core (`snes9x.cpp`, `memmap.cpp`, `movie.cpp`, `snapshot.cpp`, `cheats.cpp`, `port.h`, `fscompat.h`) exposes:
- `S9xMovieCreate` / `S9xMovieOpen` / `S9xMovieStop` / `S9xMoviePlaying` / `S9xMovieRecording` for movie recording
- `S9xGetFilename(ext, dirtype)` / `S9xGetDirectory(dirtype)` / `S9xGetFilenameInc` for paths
- `S9xDumpSPCSnapshot` for SPC dumping
- `Memory.LoadROM` / `Memory.LoadMultiCart` / `Memory.SaveSRAM` / `Memory.LoadSRAM` for ROMs
- `Memory.ROMName`, `Memory.MapType()`, `Memory.Country()`, `Memory.StaticRAMSize()`, `Memory.PublishingCompany()`, `Memory.KartContents()` for ROM info

Anything in the list of "File menu" items that has a core function behind it **must** call that function via the `Snes9xController` facade in `qt/src/Snes9xController.cpp`, not reimplement it. If the Qt frontend needs a wrapper that the controller doesn't yet expose, add the wrapper in `Snes9xController.{hpp,cpp}` and route through it.

### 3. Don't paper over the root cause
The recent stream of bugs in this fork traced to: hardcoded paths in dialogs, multi-cart dialog duplicating the QFileDialog filter, BIOS directory returning an empty string because the switch had no `case`, ZIP filter missing from one of three sites. **Each was a "fix the symptom" patch that hid a structural mistake.** When something is wrong:
1. Find the function the call routes through.
2. Fix the routing function.
3. Only then update individual callsites if needed.

Adding a `if (foo) return;` at one callsite to dodge a missing case elsewhere is debt. Stop and route it correctly.

### 4. Comments only when asked
Existing in-tree code comments are part of the codebase — leave them. Don't add new ones. Exception: a `ponytail:` marker on a deliberate shortcut (see Skill rules below) is allowed.

## Quirks that bite

- `port.h` defines a custom type system (`int8/16/32/64`, `uint8/16/32/64`, `bool8`) and the byte-order/endianness (`LSB_FIRST` vs `MSB_FIRST`) and `READ_WORD`/`WRITE_DWORD` macros. The unix `configure` actively probes `RIGHTSHIFT_*_IS_SAR` and adds defines — do not hard-code them.
- **`UNZIP_SUPPORT` requires the `unzip/` include path in `qt/CMakeLists.txt`.** `stream.h:54` does `#include "unzip.h"` (the local minizip copy, not the system one). When you turn `UNZIP_SUPPORT` on in the Qt build, you must also `target_include_directories(snes9x-core PRIVATE ../ ../unzip)` or every translation unit that pulls `snes9x.h` will fail with `fatal error: 'unzip.h' file not found`. The GTK frontend already has this in its CMakeLists; the Qt frontend does not. Same trap for `JMA_SUPPORT` if a future change pulls `jma.h` into `snes9x.h`. Lesson: **adding a `#ifdef`-gated `#include` family means auditing the CMake include paths too, not just the compile defines.**
- The core is written as "C++ as a better C": most `cpp` files do not use classes, exceptions, or RTTI. The unix Makefile adds `-fno-exceptions -fno-rtti` when the compiler accepts them (`unix/configure.ac:88-89`).
- `Snes9x-1.63` is wired up by every front-end as a translation unit; any new core `.cpp` must be added to **all** of: `gtk/CMakeLists.txt` (lines 243-352), `qt/CMakeLists.txt` (`SNES9X_CORE_SOURCES`, lines 17-76), `unix/Makefile.in` (`OBJECTS`), `libretro/Makefile.common` (`SOURCES_C`/`SOURCES_CXX`), `win32/snes9xw.vcxproj.filters`, and `macosx/snes9x.xcodeproj/project.pbxproj`. Forgetting one means it silently doesn't build there.
- `apu/` and `jma/` are linked separately from the rest of the core in GTK (`jma` is an `add_library` in `gtk/CMakeLists.txt:367`); Qt builds the core into a static `snes9x-core` lib (`qt/CMakeLists.txt:77`); libretro and the unix port just compile the sources directly.
- Vulkan paths assume Vulkan headers from `external/vulkan-headers`; **do not** rely on system Vulkan headers — define `VK_USE_PLATFORM_XLIB_KHR` / `VK_USE_PLATFORM_WAYLAND_KHR` / `VK_USE_PLATFORM_WIN32_KHR` per platform.
- The `win32/ddraw/ddraw_{x86,x64}.lib` stubs and `macosx/libz_u.a`, `macosx/libHIDUtilities_u.a` are prebuilt and intentionally checked in — `.gitignore` whitelists them.
- ROMs (`*.smc`, `*.sfc`), SRAM (`*.srm`), freeze slots (`*.00[0-9]`), patches (`*.ips/ups/bps/fig`), movies (`*.smv`), screenshots, cheats, BIOS, and `win32/snes9x.conf` are user data — `.gitignore` excludes them.
- Front-end specific defines (set by each build system, NOT in the source): `SNES9X_GTK`, `SNES9X_QT`, `__LIBRETRO__`, `__WIN32__`, `__MACOSX__`. `port.h` and `memmap.cpp` use these for conditional code.
- Compiled-in defaults are baked into each front-end; see `unix/unix.cpp:1629-1648` for the canonical `Settings` defaults (sound rate, frame timings, etc.).

## Conventions

- File headers: most files start with the Snes9x license comment block. Do not strip it when editing.
- `do not add comments unless asked` applies; existing in-tree code comments are part of the codebase — leave them.
- Project version is `1.63`, set in `snes9x.h` (`VERSION`), `unix/configure.ac`, `gtk/CMakeLists.txt`, `qt/CMakeLists.txt`, and `appveyor.yml`. Bump them together if you bump it.
- C++ standard: GTK and Qt require C++20 (`set(CMAKE_CXX_STANDARD 20)`). libretro and the unix port stay on older standards (`-std=c++14` is set in libretro's `osx` block). Win32 uses MSVC's `stdcpp17` (see `libretro-win32.vcxproj`).

## CI

- `.cirrus.yml` covers `snes9x-gtk` (Linux amd64), `snes9x-x11` (Linux/FreeBSD amd64), `Snes9x` (macOS amd64), and `libretro` for many platforms. Each task is self-contained — find a working command by reading the relevant task block rather than guessing.
- `appveyor.yml` covers Windows (`Release Unicode` and `libretro Release` × `Win32` and `x64`) on Visual Studio 2022.

## Reference docs in-tree

- `docs/porting.html` — what a port must implement (`S9xInitUpdate`, `S9xDeinitUpdate`, `S9xOpenSoundDevice`, `S9xMixSamples`, etc.); authoritative for new front-ends.
- `docs/controls.txt`, `docs/control-inputs.txt` — controller binding system.
- `docs/snapshots.txt` — save-state format details.
- `docs/changes.txt` — release notes (worth skimming before claiming a regression is new).

## Agent skills (`.opencode/skills/`)

The user runs this fork with OpenCode + the **ponytail** family of skills installed under `.opencode/`. Use them as described in each `SKILL.md`. The full set:

- **`/ponytail`** (or `@ponytail`) — the default mode. Forces the simplest solution that actually works: YAGNI → reuse existing → stdlib → native → one line. The user has flagged the fork as a place where I was over-building; this skill keeps me honest. Default intensity is `full`.
- **`/ponytail-review`** — review diffs for over-engineering only. Output: one line per finding (`L42: yagni: factory, one product. Inline.`). Use before opening a PR.
- **`/ponytail-audit`** — whole-repo over-engineering scan. One-shot report, ranked biggest cut first. Use periodically to keep the codebase from rotting.
- **`/ponytail-debt`** — harvest every `ponytail:` comment into a ledger. Use when the user says "what did ponytail defer".
- **`/ponytail-gain`** — measured-impact scoreboard (benchmark medians, not per-repo numbers). One-shot.
- **`/ponytail-help`** — quick reference card for the above.

Configuration:
- Default mode = `full`, set globally.
- Override per-session with `/ponytail lite` (build what's asked, name the lazier alt) or `/ponytail ultra` (YAGNI extremist, deletion before addition).
- Disable with `stop ponytail` or `/ponytail off`.

### Marker convention for deliberate shortcuts
When ponytail makes a deliberate simplification (e.g. "global lock, upgrade when throughput matters", "naive O(n²) scan, profile first"), mark it inline with:
```
// ponytail: <ceiling>, upgrade: <trigger to revisit>.
```
Anything marked with `ponytail:` gets harvested by `/ponytail-debt`. If the marker names no upgrade trigger, it's flagged as `no-trigger` rot risk. Use the marker only on non-trivial shortcuts, not trivial one-liners.