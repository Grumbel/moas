# MOAS — Mouse Orientiertes Adventure System

MOAS is a point-and-click adventure system originally written in C by Thomas
Holz around 1992–2000 for DOS. This repository is a work-in-progress port
using SDL2, intended to preserve the original game logic while replacing DOS
hardware and services with a modern platform layer.

The included game data is for **Mänick Mänschion**. The port is still under
active development; timing, audio, and some legacy effects may not yet match
the original DOS release.

## Screenshot

![Mänick Mänschion running with the SDL2 port](docs/screenshot.png)

## Current status

- SDL2 window, display, mouse, keyboard, and event handling
- Original 320×200, 256-colour software-rendered graphics
- Original script/bytecode interpreter and game data loading
- SDL audio output for digitized tracker samples (14 kHz, unsigned 8-bit mono)
- DOS save-game structures adapted to a packed on-disk layout
- Debug logging for scripts, input, waits, windows, and data loading
- FreeDesktop integration (`.desktop`, hicolor icons, AppStream metainfo)
- Cross builds: Windows (MinGW), Android (APK), WebAssembly, R36S/PortMaster

Known limitations include unfinished speech-channel wiring and legacy effects
such as `PowerBlend` and `WalkNLayers`. The bundled data does not include the
`PIZZZA*.TSS` music files, so that music will be silent unless the files are
available separately. See [TODO.md](TODO.md) for the current porting notes.

## Build and run

### Requirements

- C compiler with C99 support (GCC or Clang)
- CMake 3.16 or newer
- SDL2 development libraries
- `pkg-config` (useful for dependency discovery)

On NixOS, the supplied flake provides a build environment and package:

```sh
nix develop
cmake -S . -B build
cmake --build build
./build/moas
```

The development shell sets `MOAS_DATADIR` to the repository's `manick/`
directory by default. Outside that shell, run from the repository root or set
the data directory explicitly:

```sh
MOAS_DATADIR="$PWD/manick" ./build/moas
```

You can also configure and build directly with CMake on other Linux systems:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --parallel
MOAS_DATADIR="$PWD/manick" ./build/moas
```

To build and run the Nix package directly:

```sh
nix run .
# or: nix build .#moas && ./result/bin/moas
```

The CMake install target installs the executable, game data, FreeDesktop
`.desktop` entry, hicolor icons, and AppStream metainfo under the configured
prefixes. After `cmake --install`, the game appears in application menus on
typical Linux desktops.

## Cross-platform packages (Nix)

All of the following require Linux host tools and the flake inputs (SDL2
source, Android SDK where needed).

### Windows (MinGW)

```sh
nix build .#moas-win64          # 64-bit
nix build .#moas-win64-zip      # zip with SDL2.dll + data
nix build .#moas-win32
nix build .#moas-win32-zip
```

See `nix/windows.nix`.

### Android

```sh
nix build .#moas-android         # moas-<version>.apk
nix run .#install-android-moas   # install on a device with adb
```

NDK build of `libmain.so` against a prebuilt SDL2, packaged with SDL’s Java
activity (`nix/android.nix`, `mk/android/`). Game data from `manick/` is
packaged as APK assets and extracted to internal storage on first run
(`MOAS_DATADIR`). Useful logcat filter:

```sh
adb logcat -s SDL/APP:V MOAS:V *:E
```

### WebAssembly (Emscripten)

```sh
nix build .#moas-wasm
nix run .#moas-wasm              # local HTTP server + open browser
```

Static SDL2 for wasm32 and a self-contained site (`moas.html` / `.js` /
`.wasm` plus preloaded game data). Header globals use a single definition
TU under Emscripten; the blocking DOS-style game loop uses ASYNCIFY so
`Wait` / `getch` can yield to the browser (`nix/wasm.nix`, `mk/wasm/`).

### R36S and other PortMaster handhelds (ArkOS)

```sh
nix build .#moas-r36s-portmaster-zip   # for PortMaster autoinstall
nix build .#moas-r36s-portmaster       # or copy contents to /roms/ports/
```

Cross-compiled against an Ubuntu 19.10 arm64 sysroot (`nix/r36s.nix`) so the
binary runs on ArkOS’s glibc and SDL2. Controls and install notes:
`mk/r36s/README.md`. Launch with `--fullscreen`.

## Command-line options

```text
-h, --help           Show help
-d, --debug=LIST     Enable comma-separated debug categories
--skip-intro         Skip the intro/title script
--proc=N             Start at program N
--addr=N             Start at an address in that program
--datadir=PATH       Set the game-data directory (same as MOAS_DATADIR)
--fullscreen         Start in desktop-fullscreen mode
```

Debug categories are `script`, `input`, `wait`, `window`, `load`, and `all`.
For example:

```sh
./build/moas --debug=wait,input,window
./build/moas --skip-intro --debug=script
./build/moas --fullscreen
MOAS_DEBUG=all ./build/moas
```

## Repository layout

- `src/` — C game engine and SDL2 platform layer
- `manick/` — game data files; original DOS executables are not used by the port
- `data/` — FreeDesktop `.desktop` file, hicolor icons, AppStream metainfo
- `mk/android/` — Android NDK/APK packaging scripts and templates
- `mk/r36s/` — PortMaster launcher, `port.json`, screenshot
- `mk/wasm/` — Emscripten HTML shell and helper scripts
- `nix/` — Windows, Android, R36S, and WASM derivations
- `docs/screenshot.png` — screenshot of the current port
- `TODO.md` — current implementation notes and outstanding work
- `flake.nix` — Nix development shell, packages, and apps

## Contributing / development notes

The main goal is to keep the original game logic intact and isolate platform-
specific behavior behind the platform layer. Consult `AGENTS.md` and `TODO.md`
before making changes; `TODO.md` records known gaps and suggested next steps.

## License

The source code is distributed under the GNU General Public License, version 2
or (at your option) any later version. See [COPYING](COPYING). Game data may be
subject to separate terms; consult the included `LICENCE.DAT` and the original
rights holders before redistributing it.
