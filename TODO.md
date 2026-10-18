# TODO / Handoff

## Base
- Original tip: `8d399e6` (Converted source from Latin1 to UTF-8)
- Work line base for bundles: `8d399e6`

## Status (2026-10-18)
SDL2 Linux port scaffold is in place and the binary links.

### Done
- AGENTS.md + TODO.md
- CMakeLists.txt (SDL2, -fcommon for old-style header globals)
- flake.nix (packages.default + devShell)
- src/platform.h + src/platform_sdl.c
  - 320x200 window (scaled 3x), palette conversion (VGA 0-63 → RGB), present, mouse events into `Mouse`, ESC/quit → KeyAbortState=2, ticker
- compat.h: inp/outp, min/max, kbhit/getch, itoa/ltoa, filelength, _setcolor, _memmax stubs
- VGA_ADRESSE / Clearscreen redirected to the software `Screen` buffer on non-DOS
- D32DosMemAlloc → malloc
- GetTicker → PlatformGetTicker
- InitAll calls PlatformInit
- QuickSetPalette pushes to SDL
- main → int main, event pump + present loop
- Hardcoded 0xA0000 cleaned up
- powerbld BIOS-timer and null-safety guards
- First successful link of `moas`

### Known limitations
- No game data (`PIZZZA.*` / `GAME.*`) in tree → InitAll / LoadPack / Execute will InternalError. Window should still open briefly if PlatformInit runs before the failure.
- SoundBlaster path still contains real hardware code behind incomplete guards; InitDigi is commented out in InitAll so it is not reached yet.
- Palette cycling / day-light still mostly DOS-only (outp paths).
- Mouse cursor drawing and some redraw paths write to VGA_ADRESSE (now Screen) but may need an explicit PlatformPresent after every dirty region for fluid updates.
- `gets` still present (warning only).
- Globals still defined in the header (kept working via `-fcommon`); a future cleanup should move them to a globals.c + externs.

## Next steps
1. Make LoadPack / data loading soft-fail so the window stays open and a test pattern or logo can be shown without assets.
2. Call PlatformPresent from PowerRedrawArea / RedrawCompleteScreen / key palette changes.
3. Stub or rewrite InitDigi / sound playback with SDL audio (or keep silent).
4. Clean globals out of moas.h.
5. Test with real game data when available.
6. Polish flake.nix / add a simple README.

## Bundle
Next: `moas-006.1-sdl-port-scaffold-8d399e6.bundle`
