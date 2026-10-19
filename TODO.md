# TODO / Handoff

- Android: APK assets + extract to internal storage; SDL_Log diagnostics

## Cross ports (this bundle)
- MS-DOS: `nix/dos.nix`, `mk/dos/Makefile` — Open Watcom → DOS/4GW `moas.exe`
- WebAssembly: `nix/wasm.nix`, `mk/wasm/` — Emscripten + ASYNCIFY + preload data
- Technique mirrored from Kurvenrausch (Nix flake packages + mk/ tree)
- Windows: `nix/windows.nix` — MinGW + SDL2-win32 flake, win32/win64 zip
- R36S / PortMaster: `nix/r36s.nix`, `mk/r36s/` — eoan arm64 sysroot, launcher
- Android: `nix/android.nix`, `mk/android/` — NDK + SDL2 APK scaffold
- `--fullscreen` / `-f` for handhelds and borderless desktop
- Android: game data (manick/) not yet packed into APK assets / internal storage
- Windows: first MinGW build may need small POSIX path fixes — untested here

## Desktop integration
- FreeDesktop `.desktop` file: `data/moas.desktop`
- hicolor icons (16–256 px): large red “M” with protagonist, night path scene
- AppStream metainfo: `data/metainfo/org.grumbel.moas.metainfo.xml`
- CMake install rules for applications/, icons/hicolor/, metainfo/

## Audio
- InitDigi() enabled: volume/step tables + SDL audio @ 14 kHz U8 mono
- DigiMixFrame reimplements tracker mix; SDL callback streams SampleBuffer2
- LoadMusic: SampleDisk (52-byte) load; data path under MoasDataDir
- Op 95 LoadMusic enabled
- No PIZZZA*.TSS in manick/ — music silent until data present

## Timing
- Ticker back to PIT divisor 0x1FFF (~145.67 Hz)
- IveGotTooMuchTime: original semantics, one step per call
- Wait()/shake use real 100 ms units (PlatformGetMillis)

## Input
- Cursor tracks inside windows; blocking getch(); click timeout in seconds
- Bewege (grab+pull) directional arrow cursor: VMouse now draws Phase[1]
  when Mouse.BewegeMode != -1 (was DOS-only in PowerRedrawArea)

## DOS port audit
- Full inventory: `docs/DOS_PORT_STATUS.md` (every `#ifdef DOS` / `#ifndef DOS`)
- Bewege arrow: ported in VMouse (Phase[1])


## Savegames (2026-10)
- Broken: wrote GAME*.SAV to process CWD; ChkOpen(READ) looked in data dir
- Fixed: MoasStateDir / MoasConfigDir / MoasStatePath
  - Linux: `$XDG_STATE_HOME/moas` (fallback `~/.local/state/moas`)
  - Config: `$XDG_CONFIG_HOME/moas` (reserved)
  - Override: `MOAS_STATEDIR` / `MOAS_CONFIGDIR`
  - DOS: CWD; DOSBox runners persist to `~/.local/state/moas-dos` and `moas-dos-original`
- Manual save: clearer errors if no slot clicked; thumbnail uses fixed loop vars

## Still open
- Android asset data path
- Verify Windows MinGW and R36S device builds
- Confirm Bewege arrow frames (Phase[1], 16 directions) look correct in-game
- PowerBlend/WalkNLayers: no data path and op 11 still commented (upstream); inert by design

## Done this stack
- Esc only sets KeyAbortState=2 when armed (KeyAbortState==1), matching DOS IRQ
- Bewege arrow cursor (VMouse + Phase[1])
- ToggleScreen → PlatformSetBlank (op 75)
- ShowMarker: draw Label/Jumper outlines on framebuffer (Master key `2`)
- ShowHercBak: stderr dump instead of 0xB8000 (Master key `#`)
- TeakEffekt (op 6): SDL palette + present; fixed Info1 index via pointer delta
- PowerBlend: present each depth step; documented WalkNLayers never loaded
- Speech/audio left alone (not used by this game data)
