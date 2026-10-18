# TODO / Handoff

## Base
- Original tip: `8d399e6`
- Work line base for bundles: `8d399e6`

## Status (2026-10-18)
### Latest
Fixed Nix build failure under hardened flags (`-Werror=format-security`):
- `mouse.c`: non-literal `sprintf` formats from game data → `snprintf` + `%s` / diagnostic ignore for intentional dynamic format
- `misc.c`: `gets` → `fgets`
- CMake: `-Wno-error=format-security` as belt-and-suspenders for hardened toolchains

### Prior
- SDL2 platform, datapath, install to `share/moas`, flake apps
- 64-bit MAPFILE / CHKSUM / case-insensitive open

### Still open
1. Font / Latin1 encoding vs UTF-8 source strings (InternalError 1013)
2. PlatformPresent on dirty redraws
3. Sound stub
4. Header globals / `-fcommon`

### Run
```bash
nix build
nix run .
# or
cmake -B build && cmake --build build && ./build/moas
```

## Bundle
Next delivered: `moas-010.1-format-security-8d399e6.bundle`
