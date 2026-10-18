# TODO / Handoff

## Base
- Original tip: `8d399e6`
- Work line base for bundles: `8d399e6`

## Status (2026-10-18)
### Done
- SDL2 platform layer, CMake, flake (earlier)
- manick data load on 64-bit (packed MAPFILE, uint32 CHKSUM, case-insensitive open)
- **Install + data path integration**
  - `src/datapath.c`: resolves data dir from `MOAS_DATADIR` env → compile-time `MOAS_DATADIR` → `./manick` → `$exe/../share/moas`
  - CMake installs binary to `bin/` and game data to `share/moas/` (excludes DOS EXEs)
  - flake `packages.default` bakes store data path; `apps.default` / `apps.moas` run the game
  - `devShell` sets `MOAS_DATADIR=$PWD/manick` for local work

### Run
```bash
# from source tree
cmake -B build && cmake --build build
./build/moas                    # finds ./manick automatically
# or
MOAS_DATADIR=$PWD/manick ./build/moas

# install prefix test
cmake -B build -DCMAKE_INSTALL_PREFIX=/tmp/moas-prefix
cmake --build build && cmake --install build
/tmp/moas-prefix/bin/moas

# Nix
nix run .#                    # apps.default
nix build
./result/bin/moas
```

### Still open
1. Font / Latin1 encoding vs UTF-8 source strings
2. PlatformPresent on dirty redraws for visible graphics
3. Sound stub
4. Move header globals out of moas.h
5. Optionally soft-fail more InternalErrors

## Bundle
Next: `moas-009.1-install-datapath-8d399e6.bundle`
