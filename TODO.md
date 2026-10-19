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
- **Exit resume:** `AUTOSAVE.SAV` in state dir — write on clean quit (`MoasAutosaveOnExit`),
  restore on next start (`MoasTryLoadAutosave`) unless `--no-autosave` or explicit `--load`.
  Separate from script slot 9 (`GAME9.SAV` / `AutoSaveGame`). Refactored `SaveGameToFile` /
  `SaveGameSlot` so path saves share one writer.

## Android overlay SAVE/LOAD feed (2026-10-19)
- Opening on press + residual mouse edge closed LoadSaveMenu in one frame.
- SAVE/LOAD queue on *release*; LookKeys runs SaveGame/LoadGame.
- LoadSaveMenu drains Left/Right before waiting for slot click.

## Android overlay (2026-10-19)
- Hit-test used window coords vs letterbox in renderer output → hard to press on hi-DPI.
  Fixed via `window_to_output`; `mouse_to_logical` shares the mapping.
- Larger buttons (fill side letterbox), 14px touch pad, pressed state (inset + gold border + darker face).
- Icons: floppy / folder+arrow / stop+X / double-chevron with more detail.

## Still open
- Android asset data path
- Verify Windows MinGW and R36S device builds
- PowerBlend/WalkNLayers: no data path and op 11 still commented (upstream); inert by design
- In-game confirm: Bewege arrow (drag person/object 0–999 or ≥2000 away from click point; inventar 1000–1999 uses Phase[0] icon embed, not arrow)

## Bewege arrow (2026-10-19)
- Not missing from the port: DOS baked Phase[1] only in PowerRedrawArea; SDL uses VMouse + Phase[1] (commit 130d221).
- Bug: MouseRead called VMouse(2) *before* DisplayMouseText set BewegeMode / inventar embed, so the arrow (and inventar grab icon) could lag one frame or never refresh when frame_off was unchanged.
- Fix: MouseIntelligence forces VMouse(2) on BewegeMode change and VMouse(3)+VMouse(1) after inventar Phase[0] texel embed/clear.
- Host Phase[0]/[1] size checks now log to stderr (DOS still InternalError).

## Done this stack
- Android quit: finishAndRemoveTask + InternalError(0)/Alt+Q → RequestQuit; MOAS_STATEDIR for autosave
- LoadGameFromFile: re-init cursor/control so hover works after autosave/load
- Android quit: Activity.finish() + SDL_Quit; AC_BACK quits
- Touch USE/LOOK modes; both walk on empty; LOOK object = SIEHE (no Benutze/Bewege)
- Touch walk: non-DOS left-click on empty ground → WalkClick + idle C_WALK (no D-pad)
- Android overlay: DPI hit-test fix, pressed feedback, richer icons
- Exit AUTOSAVE.SAV + `--no-autosave`; SaveGameToFile/SaveGameSlot refactor
- Esc only sets KeyAbortState=2 when armed (KeyAbortState==1), matching DOS IRQ
- Bewege arrow cursor (VMouse + Phase[1]) + same-frame refresh after mode/embed change
- ToggleScreen → PlatformSetBlank (op 75)
- ShowMarker: draw Label/Jumper outlines on framebuffer (Master key `2`)
- ShowHercBak: stderr dump instead of 0xB8000 (Master key `#`)
- TeakEffekt (op 6): SDL palette + present; fixed Info1 index via pointer delta
- PowerBlend: present each depth step; documented WalkNLayers never loaded
- Speech/audio left alone (not used by this game data)
