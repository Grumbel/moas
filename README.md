# MOAS — Mouse Orientiertes Adventure System

MOAS is a point-and-click adventure system originally written in C by Thomas
Holz around 1992–2000 for DOS. This repository is a work-in-progress Linux port
using SDL2, intended to preserve the original game logic while replacing DOS
hardware and services with a modern platform layer.

The included game data is for **Mänick Mänschion**. The port is still under
active development; timing, audio, and some legacy effects may not yet match
the original DOS release.

## Screenshot

![MOAS running with the SDL2 Linux port](docs/screenshot.png)

## Current status

- SDL2 window, display, mouse, keyboard, and event handling
- Original 320×200, 256-colour software-rendered graphics
- Original script/bytecode interpreter and game data loading
- SDL audio output for digitized tracker samples (14 kHz, unsigned 8-bit mono)
- DOS save-game structures adapted to a packed on-disk layout
- Debug logging for scripts, input, waits, windows, and data loading

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
```

The CMake install target installs the executable and game data under the
configured install prefixes. The compiled-in default data path can be changed
with `-DMOAS_DATADIR=/path/to/data` at configure time; `MOAS_DATADIR` or the
`--datadir=PATH` command-line option can override it at runtime.

## Command-line options

```text
-h, --help           Show help
-d, --debug=LIST     Enable comma-separated debug categories
--skip-intro         Skip the intro/title script
--proc=N             Start at program N
--addr=N             Start at an address in that program
--datadir=PATH       Set the game-data directory
```

Debug categories are `script`, `input`, `wait`, `window`, `load`, and `all`.
For example:

```sh
./build/moas --debug=wait,input,window
./build/moas --skip-intro --debug=script
MOAS_DEBUG=all ./build/moas
```

## Repository layout

- `src/` — C game engine and SDL2 platform layer
- `manick/` — game data files; original DOS executables are not used by the port
- `docs/screenshot.png` — screenshot of the current port
- `TODO.md` — current implementation notes and outstanding work
- `flake.nix` — Nix development shell and package definition

## Contributing / development notes

The main goal is to keep the original game logic intact and isolate platform-
specific behavior behind the platform layer. Consult `AGENTS.md` and `TODO.md`
before making changes; `TODO.md` records known gaps and suggested next steps.

## License

The source code is distributed under the GNU General Public License, version 2
or (at your option) any later version. See [COPYING](COPYING). Game data may be
subject to separate terms; consult the included `LICENCE.DAT` and the original
rights holders before redistributing it.
