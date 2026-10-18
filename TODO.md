# TODO / Handoff

## Base
- `8d399e6` — work-line base for bundles

## Status
### Latest
- Clearer data-dir diagnostics: log chosen dir, list searched paths, `MoasReportMissingData`
- Reject `FileNum < 1` in `LadeBereich` with context (GAME.0 was a symptom)
- `PutCharAt` null/bounds guards (segfault during PrintCharacter)
- Debug builds: CMake default `RelWithDebInfo`; flake `dontStrip = true` + RelWithDebInfo

### Run
```bash
nix run .
# if data missing:
MOAS_DATADIR=/path/to/manick nix run .
gdb --args result/bin/moas   # symbols retained
```

### Still open
- Present-on-redraw for visible graphics
- SDL shift modifiers for say windows
- Sound
