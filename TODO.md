
## Windows MinGW follow-up (2026-10-20)
- setenv was only in compat.h; debug.c did not include it → implicit decl.
- inp/outp stubs conflicted with MinGW intrin.h (pulled via SDL).
- Fix: include compat.h in debug.c; gate inp/outp on !_WIN32; InitDSP only under DOS.

# TODO / Handoff

## Sanitizers, touch SKIP fix, key names on buttons (2026-10-22)
- `nix develop` builds use `-fsanitize=address,undefined` (CMake
  `MOAS_SANITIZE`, env `MOAS_SANITIZE` in moas-configure; empty = off;
  `-fno-sanitize=alignment` because bytecode params are unaligned shorts).
  ASAN_OPTIONS=detect_leaks=0. ASan's dlopen interceptor loses sdl2-compat's
  RUNPATH, so the shell adds that exact SDL3 (`compatSdl3`, from
  `pkgs.SDL2.buildInputs`, `.lib` output) to LD_LIBRARY_PATH.
- First sanitizer run (title, F12 menu, player choice, F1 menu): only
  finding was PowerSelectFromWindow reading RefTable[-99] (LastY starts at
  -99) and recolouring garbage rows. Fixed with `LastY>=0`.
- Touch SKIP did nothing: the synthetic Shift was a 20-iteration counter
  decremented in the event pump, which SayWait spins in a busy loop, so it
  expired before UpDateSayWindows (every 4 ticks) saw it. Speech is dismissed
  on the Shift release edge. Now a 120 ms time window (also gamepad A).
- Overlay buttons show the key (F5/F7/SHIFT/F12) top-right; not on touch
  devices. Tooltips skipped: no hover on touch, and the label is cheaper.
- Next: run longer sanitized play sessions (rooms, inventory, saves).

## Key legend, Options entry in the game menu, touch lockout (2026-10-22)
- Key legend (F5/F7/F12/Shift/Esc) drawn by `keyhints_draw()` in
  platform_sdl.c, bottom right of the game picture, on the start screen
  (`CurrentProc == 4`, the title room) and while the F12 menu is open (skipped
  if it would overlap the panel). Not drawn on touch devices.
- Game menu (prog 1, opened with F1): `Execute` injects a host option before
  "Quit" (add_option at prog 1 @ 0x0151, RefNum 5). It uses RefNum 7, which the
  script doesn't use, so its answer chain falls through to the
  `goto select_from_dialog` at 0x07f2 and the menu shows again after the F12
  menu closes. Text comes from `Ui(UI_MENU_OPTIONS)` ("ui.menu.options") via
  the `DLG_HOST_TEXT` TextAdd sentinel. DLG_ALWAYS keeps it from toggling off.
- F12 menu: TOUCH BUTTONS row hidden while the buttons are on and the device
  is touch (Android, or any SDL finger event seen), so a phone player can't
  remove the only way back into the menu. Android also ignores
  `touch_buttons` from settings.ini.
- README: master/debug keys are Alt+letter/F8 (extended scan codes), `;` was
  really F1 (game menu), master toggle is Alt+T.
- Tested under Xvfb: legend on the start screen and in the F12 menu; F1 menu
  shows "Optionen (F12)" / "Options (F12)", opens the F12 menu, Esc returns to
  the game menu, Back closes it. The hidden touch row is untested (no finger
  input under Xvfb); check on Android / phone browser.

## Persistent settings (2026-10-22)
- `src/settings.c`: `<config dir>/settings.ini` (key=value) keeps language,
  fullscreen, touch buttons, game speed, speech speed and volume. Written when
  the F12 menu or the game's F10 FileMenu closes and at `PlatformShutdown`;
  only rewritten when the content changed (tmp file + rename; IDBFS sync on web).
- Load order: `MoasSettingsLoad()` after `MoasInitDataPath`, before
  `LangInit`/`PlatformInit`; `MoasSettingsApplyGame()` after `InitAll`
  (InitVars resets the speeds).
- `--lang`/`MOAS_LANG`, `-f`/`--windowed` (new), `--touch-controls`/
  `--no-touch-controls` win over the file. `moas_fullscreen` is now -1 = unset.
  The file stores the state at exit, so a CLI override also ends up saved.
- Fullscreen from the file is ignored on web (needs a user gesture) and Android.
- Savegames still contain TotalSpeed/SpeechSpeed (format unchanged), but on
  non-DOS builds loading keeps the current speeds. DOS keeps the original.
- Web: shell.html mounts `/moas-config` on IDBFS next to `/moas-state`.
- Tested headless (dummy video): values load, survive an autosave restore,
  CLI overrides apply. Not yet checked in a browser or on Android.

## Format strings in translations (2026-10-21)
- `utf8_to_cp850()` (lang.c) replaced '%' with a space as a glyph the font
  lacks, so "USE %s" became "USE  s". The verbs, "%i von %i" and power-parse
  lines ("Leg %s in Briefkasten") are sprintf formats, so '%' now stays.
- Translator-supplied formats are checked: the conversions (`%s`/`%i`, in
  order) must match the German string, otherwise the entry is ignored with a
  warning and German is used. This guards sprintf against "%s %s" typos.
- UI ids for the overlay were `ov.*` in lang.c but `ui.overlay.*` in the
  .pot/.po and moas-util, so they never loaded; renamed to `overlay.*`.
  Added the missing `ui.slot.of` and `ui.overlay.menu` to moas-util, the .pot
  and en.po (`moas-util strings pot` reproduces lang/manick.pot exactly).
- Possible next: emit `#, c-format` for entries with '%' so `msgfmt -c`
  checks translations too.

## RESTART GAME menu entry, CLI "--opt VALUE" (2026-10-21)
- Options menu (F12) has RESTART GAME between RESUME and QUIT. The first press
  turns it into "RESTART - SURE?". The second press restarts, and moving off
  the row cancels. `?` glyph added to the menu font.
- Flow: `PlatformRequestRestart()` unwinds like a quit but skips the autosave.
  main() then sees `PlatformTakeRestart()`, deletes AUTOSAVE.SAV
  (`MoasRemoveAutosave`), closes any text window, calls `ResetGame()` (the
  body of the original `Restart()`, which now wraps it) and runs the intro +
  proc 0 again (`MoasStartNewGame`).
- `Execute()` (non-DOS) returns at the next opcode once quit/restart is
  pending, instead of fast-forwarding the script with all waits skipped.
- Tested under Xvfb+xdotool: resume from autosave → restart → intro; menu QUIT
  during the intro and in-game still exits cleanly. DOS build still compiles.
- CLI: every option that takes a value accepts `--opt VALUE` and `--opt=VALUE`
  (`opt_value()` in debug.c). `-d`/`--debug` with no list = all flags.
- Next: test restart on Android/WASM, where the menu runs from the overlay.

## Garbled say-window text (2026-10-21)
- `print.c` stripped "CLR ", " NW ", leading spaces and finished lines with
  `strcpy(s, s+n)`. The copy overlaps, which is undefined, and glibc's
  vectorised strcpy duplicates chunks ("The elevator is deaddead.", and the
  scrolling intro lines in prog 4). Now `StrDropFront()` (memmove).
- Hit English more often only because string lengths and alignment differ.
- Not yet checked visually in-game; headless smoke run (`SDL_VIDEODRIVER=dummy`) OK.
- "Failed loading SDL3 library." = binary linked against an older glibc than
  nixpkgs' SDL3 (stale CMake cache holding the host `cc`). Wipe the build dir
  and configure inside `nix develop`. devShell now replaces the host
  `LD_LIBRARY_PATH` (xanatoo pattern) but leaves `pkgs.sdl3` off it: that's a
  different build than the SDL3 in sdl2-compat's RUNPATH.

## Portrait overlay, greyed Save/Load, runtime language switch (2026-10-21)
- **Portrait:** `letterbox_geom` reserved side columns even when the window is
  taller than wide, shrinking the picture and leaving the space above/below
  empty. `ov_portrait()`: picture at full width, centred above two rows of four
  buttons (Save Load Esc Skip / Use Look - Menu) at the bottom.
- **Save/Load greyed out** when LookKeys would drop them (`MoasCanOpenSaveMenu`
  = Window.Status==None && PlayerHasControl==1); Load also when no GAME?.SAV
  exists (`MoasAnySaveExists`, checked at most once a second). Taps on a greyed
  button are ignored.
- **Language switch (F12) did not apply:** Description[] / PowerParse strings
  are translated once when prog0 registers them, so `LangInit()` alone only
  changed new dialogue lines. `MoasReloadLanguage()` = LangInit +
  MoasRegisterStaticNames, preserving UnPack's static buffer (the menu can
  open while a caller still holds it).
- Game speed stays 1..8 (original F10 menu range; `Walker>=10-TotalSpeed`
  makes 9 and 10 identical, and the original slider is drawn for 1..8).

### Next
- Persist language (and touch-buttons toggle) in the config dir; on Android
  the language choice is lost on restart.

## Safe autosave, touch overlay everywhere, palette fix (2026-10-21)
- **Autosave safety:** `.SAV` has no script PC / C stack, so saving mid-cutscene,
  dialog or Load/Save menu resumed into a broken room. `MoasCanSave()` (io.c) =
  main loop active + MouseCheck not nested + `Window.Status==None` +
  `PlayerHasControl==1` + `KeyAbortState==0`.
  - Quit/backgrounding saves only if safe; otherwise last checkpoint stays.
  - `MoasAutosaveCheckpoint()` from LookKeys: room change (CurrentProc), game's
    slot-9 AutoSaveGame, any Execute() since last save (≤ every 5 s).
  - Written via `AUTOSAVE.SAV.tmp` + rename (kill-safe).
  - Removed post-loop save in main() and InternalError(0): after a quit request
    scripts unwind with waits cut short, that state is not faithful.
- **Touch overlay on all SDL targets:** runtime `overlay_on`; default on for
  Android/web, `--touch-controls` / `--no-touch-controls`, F12 "Touch buttons"
  (desktop window grows/shrinks by 280 px). New **MENU** button (bottom right).
- Overlay graphics: 16×16 pixel-art icons (floppy, folder+arrow, stop sign,
  fast-forward, left/right mouse button for USE/LOOK, hamburger) rendered as
  textures at whole multiples; rounded gradient buttons, gold rim for active mode.
- Options menu (F12 / MENU / pad Back): mouse hover+click, wheel, right-click
  closes; touch tap rows, left half of value = step back, tap outside closes.
  Added Touch buttons row; Left arrow now decrements.
- Input fixes: synthetic touch→mouse events ignored (taps were seen twice and
  the cursor jumped into the letterbox); one finger drives the game; mouse
  release over a button no longer leaves the game's left button stuck;
  overlay/pad Esc sends a break code next frame; pad B/Start/Back edge-triggered.
- Keys: Alt+letter now produces BIOS `0,scancode` (Alt+Q quits as on DOS,
  master Alt+I/J/P work); plain **Q** no longer quits.
- **Porting bug fixed:** non-DOS `QuickSetPalette` memcpy'd its argument into
  `Palette` (Blend fade frames overwrote the master palette) and ignored
  DayLight/greyscale; palette cycling did the same. Now mirrors the DOS DAC
  path via `PlatformSetPaletteEntry` (6-bit masked like the VGA DAC).

### Next
- Verify touch on a real Android device / touchscreen laptop (only mouse and
  keyboard were exercised under Xvfb).
- Persist the touch-buttons toggle in the config dir.




## Windows MinGW (2026-10-20)
- Site build failed on win32: compat.h `itoa`/`ltoa`/`filelength` conflict with MinGW CRT;
  `setenv` implicit decl; moas-util `mkdir` arity.
- Fix: guard Borland helpers with `!_WIN32`; add `setenv` via `_putenv` (string retained);
  moas-util uses `_mkdir` on Windows.

## R36S SDL include (2026-10-20)
- Site build failed: `digi_io.c: fatal error: SDL.h: No such file or directory`
- Cause: `nix/r36s.nix` passed `SDL2_INCLUDE_DIRS=\${sysroot}/usr/include` but
  sources use `#include <SDL.h>` and eoan installs the header at `include/SDL2/SDL.h`.
  Kurvenrausch uses `#include <SDL2/SDL.h>` so the same path works there.
- Also `find_package(SDL2)` always ran and could overwrite pre-set paths.
- Fix: r36s → `.../include/SDL2`; CMakeLists skips `find_package` when
  `SDL2_INCLUDE_DIRS` and `SDL2_LIBRARIES` are already set via `-D`.

## Cross ports (this bundle)
- MS-DOS: `nix/dos.nix`, `mk/dos/Makefile` — Open Watcom → DOS/4GW `moas.exe`
- WebAssembly: `nix/wasm.nix`, `mk/wasm/` — Emscripten + ASYNCIFY + preload data
- Technique mirrored from Kurvenrausch (Nix flake packages + mk/ tree)
- Windows: `nix/windows.nix` — MinGW + SDL2-win32 flake, win32/win64 zip
- R36S / PortMaster: `nix/r36s.nix`, `mk/r36s/` — eoan arm64 sysroot, launcher
- Android: `nix/android.nix`, `mk/android/` — NDK + SDL2 APK scaffold
- `--fullscreen` / `-f` for handhelds and borderless desktop
- Android: game data packed into APK assets/; extracted to internal storage on start
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
- Verify Windows MinGW and R36S device builds
- PowerBlend/WalkNLayers: no data path and op 11 still commented (upstream); inert by design
- In-game confirm: Bewege arrow (drag person/object 0–999 or ≥2000 away from click point; inventar 1000–1999 uses Phase[0] icon embed, not arrow)

## Bewege arrow (2026-10-19)
- Not missing from the port: DOS baked Phase[1] only in PowerRedrawArea; SDL uses VMouse + Phase[1] (commit 130d221).
- Bug: MouseRead called VMouse(2) *before* DisplayMouseText set BewegeMode / inventar embed, so the arrow (and inventar grab icon) could lag one frame or never refresh when frame_off was unchanged.
- Fix: MouseIntelligence forces VMouse(2) on BewegeMode change and VMouse(3)+VMouse(1) after inventar Phase[0] texel embed/clear.
- Host Phase[0]/[1] size checks now log to stderr (DOS still InternalError).


## Cold load / autosave (from script disasm)

Prog **0** (`moas-util disasm 0`) is global init, not per-room:

1. `add_description` ×81 + `add_power_parse` ×10 (interleaved with `set_object` inventory defaults)
2. fresh-game `var_set` / `say_properties`
3. `call_proc 4` → first story/room script

`.SAV` has persons, markers, vars, inventar x/y, dialogs, CurrentProc, pictures — **not** Description[].
Mid-session load keeps Description in RAM; cold load must replay prog0 name ops only
(`MoasRegisterStaticNames`: apply 65/62, skip 38/40/85, stop at call_proc 17).

Room progs (5–24) use `add_area_*` (markers → **in save**). No `add_walk_area` in any prog.
`set_object` defaults must **not** run on load (inventar x already in save).

Exit autosave = **AUTOSAVE.SAV** (dedicated; slot 9 stays in-game AutoSaveGame only).

## Done this stack
- Runtime language packs: Ui(id) + UnPack LangLookup, --lang / MOAS_LANG
- moas-util strings: export script/PCK/UI text as json|pot|tsv for translation
- Android/overlay: letterbox_geom reserves side columns so buttons never cover the game
- Gamepad: left stick/D-pad walk, right stick cursor, RT use, LT look, A skip, B/Start Esc
- WASM fullscreen/window: CSS aspect matches overlay window so buttons stay in letterbox
- WASM: link -lidbfs.js so FS.filesystems.IDBFS exists at preRun
- WASM shell: fullscreen/pause/download-upload saves (IDBFS), better key help; touch overlay on web
- Android swipe-away / background: autosave on SDL_APP_* lifecycle + RequestQuit; logcat for autosave path
- Android assets: version stamp re-extract on APK upgrade; SDL_Log diagnostics; MOAS_VERSION in NDK flags
- Android quit: finishAndRemoveTask + InternalError(0)/Alt+Q → RequestQuit; MOAS_STATEDIR for autosave
- LoadGameFromFile: re-init cursor/control so hover works after autosave/load
- Exit autosave = AUTOSAVE.SAV; cold resume MoasRegisterStaticNames; no .SAV format change
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
