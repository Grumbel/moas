
## Windows MinGW follow-up (2026-10-20)
- setenv was only in compat.h; debug.c did not include it → implicit decl.
- inp/outp stubs conflicted with MinGW intrin.h (pulled via SDL).
- Fix: include compat.h in debug.c; gate inp/outp on !_WIN32; InitDSP only under DOS.

# TODO / Handoff

## Game named `manick`, engine stays MOAS (2026-10-27)
- Shipped game is Mänick Mänschion (original distribution `Manick.zip`), so
  binary, data dir and packages are `manick`; the full name is for GUIs.
  Flake packages/apps are `manick-*` too (`nix build .#manick-wasm`,
  `nix run .#manick`). Engine side keeps MOAS: `moas-util`, `MOAS_*` env
  vars, dev-shell scripts `moas-configure`/`moas-build`/`moas-run`.
- Binary `manick` (`manick.exe` on Windows/DOS), `share/manick`,
  `data/manick.desktop`, icon `manick`, AppStream/Android id
  `org.grumbel.manick` (APK no longer upgrades the old `org.grumbel.moas`).
- Save/config dirs `~/.local/state/manick`, `~/.config/manick`,
  `%LOCALAPPDATA%\manick\state`, `%APPDATA%\manick`; no fallback to the old
  `moas` dirs (move them by hand). Android internal storage and web IDBFS
  use `manick-state` / `manick-config` too, web save export is
  `manick-saves.json` (format `manick-saves`); old browser saves and
  exports are not read (game not announced yet, so no migration).
- Artifacts: `manick-<ver>-win64.zip`, `manick-<ver>.apk`,
  `manick-<ver>-dos.zip`, web `manick.html/.js/.wasm/.data`. DOSBox runner
  state moved to `~/.local/state/manick-dos{,-original}`.
- PortMaster: `manick.zip` with `Mänick Mänschion.sh` and `ports/manick/`.
  Untested on device: check the umlaut file name survives PortMaster
  install on ArkOS (fallback: `Manick Manschion.sh`). The zip is written
  with Python's zipfile so that name carries the UTF-8 flag; Info-ZIP in
  the sandbox left it unflagged (read as CP437 `M├ñnick`).
- Icon master renamed to `data/icons/manick-master.png`.
- TODO website repo: update flake package names (`manick-*`) and
  download/wasm file names in its flake.nix, index.html, README.

## Gamepad remap, 4:3 aspect ratio (2026-10-27)
- Pad: both sticks move the cursor and add up (both = faster), time-based
  at `PAD_CURSOR_SPEED` 150 logical px/s per stick (was ~14 px per pump,
  far too fast). D-pad walks (left stick no longer does). A/R1 use, B/L1
  look, X held = hotspots (like H), Y held = skip text, Back = Esc, Start =
  options menu, R2/L2 = save/load (edge on the trigger axis, same queue and
  checks as the overlay buttons), R3/L3 = game speed +/- (1..8, no wrap,
  "GAME SPEED n" notice on the picture, saved to settings.ini).
- Start screen key hints show pad buttons while a pad is connected.
- Aspect: the picture was shown with square pixels (8:5); mode 13h on a
  CRT is 4:3 (pixels 1.2x tall). New setting `aspect=4:3|8:5` (default
  4:3), `--aspect`, SETTINGS row ASPECT RATIO (`ui.opt.aspect`, all .po).
  `letterbox_geom` uses `display_h()` (240/200); the window opens at
  960x720 and a plain window changes height on toggle.
- 4:3 scaling: nearest to an integer multiple in a target texture, then
  linear to the final size (`present_source`), so rows are not uneven.
  Exact integer scales skip it.
- Web: the canvas is always 1240x720 (shell.html slot aspect 31/18); 8:5
  is letterboxed inside.
- Tested: build, ctest, Xvfb screenshots at 4:3 and 8:5. Gamepad changes
  untested on hardware (no pad here); check cursor speed and R36S L3/R3.
- Website (separate repo) gamepad line updated as well.

## Port diagnostics go through MoasLog (2026-10-26)
- `src/log.{h,c}`: `MoasLog(fmt, ...)`, one line per call. Desktop/wasm →
  stderr, Android → SDL_Log, DOS → `MOAS.LOG` in the CWD (truncated per run,
  flushed per line), so nothing draws over the VGA screen.
- All port-side `fprintf(stderr, ...)` and the local helpers (`data_log`,
  `MOAS_IO_LOG`, `platform_log`) now use it.
- Original DOS console output kept where it runs in text mode: licence line
  at startup, `InternalError` after `_setvideomode(_DEFAULTMODE)`, master-mode
  dumps (`GetRealPersonNum`, label export). Missing-data-file and
  unknown-opcode messages printed over the graphics screen and waited for a
  key; they now go to the log and InternalError reports directly.
- CLI output (`--help`, `--version`, option errors) stays on stdout/stderr.
- `MoasDbg` is still a no-op on DOS; could route to MoasLog if needed.

## Game data built from manick-src/ (2026-10-25)
- `manick-src/` = `moas-util unpack` of `manick/`, PNGs recompressed with
  `oxipng -o 4 --nx --strip safe` (6.8 MB → 1.7 MB; `--nx` keeps palettes
  and indices). `manick/` stays untouched as the original/reference.
- CMake target `gamedata` (ALL) packs it into `<build>/gamedata/` with
  `--reference manick`; output is byte-identical to `manick/` (all 235
  graphics reused). `-DMOAS_GAMEDATA_DIR=` uses already packed data instead
  (required for wasm; other cross builds just skip installing data).
- Nix: `nix/gamedata.nix` (`.#moas-gamedata`, unversioned) builds a host
  moas-util with `$CC` and packs. Desktop, Windows, R36S, Android
  (`GAME_DATA_DIR` in build-apk.sh), wasm and the DOS rebuild use it.
  `moas-dos-original` still ships `manick/` (and DOS4GW.EXE comes from there).
- Runtime lookup: `$exe/gamedata` is tried after env/built-in and before
  `./manick`. Dev shell `MOAS_DATADIR` → `$MOAS_BUILD_DIR/gamedata`.
- `moas-util-roundtrip` ctest still checks unpack+pack of `manick/`. No test
  that `manick-src/` equals `manick/` (it is meant to be edited).
- Re-running `unpack` would write uncompressed PNGs again; run oxipng after.

## Options menu: Save/Load, Settings submenu (2026-10-25)
- R36S has no other way to reach Save/Load, so the F12 / OPTIONS / pad Back
  menu now has two pages. Main: RESUME, SAVE, LOAD, SETTINGS, RESTART, QUIT.
  SETTINGS: LANGUAGE, FULLSCREEN, TOUCH BUTTONS, GAME SPEED, SPEECH SPEED, BACK.
- SAVE/LOAD close the menu and queue the same command as the overlay
  buttons (`overlay_pending_cmd`, run by LookKeys). Greyed out and skipped
  by navigation when `MoasCanOpenSaveMenu()` is false or (LOAD) no save exists;
  checked once when the menu opens (the game is frozen meanwhile).
- Esc / Backspace / pad B / right-click: back to the main page from
  SETTINGS, close from the main page. F12, pad Start/Back, tap outside close.
- New strings `ui.opt.settings`, `ui.opt.back` (all .po files); SAVE/LOAD
  reuse `ui.key.save`/`ui.key.load`.
- Hint line still says "ESC/F12 CLOSES" on the SETTINGS page, where Esc
  goes back. Tested on desktop under Xvfb; not tested on the R36S yet.

## Per-language picture overrides (2026-10-22)
- `lang/<code>/<kind>/<kind>_NNN.png` (kind bild/inv/phase, same names and
  format as `moas-util unpack`: 8-bit indexed, game palette indices)
  replace the decoded pixels at load time (`src/langgfx.c`, hooks in
  LadePhase/LadeBild/LadeInventar). The PNG palette is ignored; size must
  match the original or the file is skipped with a warning. Index 0 is
  transparent in phases/inventory. `--debug=load` logs each override.
- Language switch (F12): cached phases/inventory with an old or new
  override are decoded again in place; the current room picture changes on
  the next LadeBild.
- PNG reader moved from tools/mu_png.c to src/pngread.c (shared).
- Shipped on all targets: wasm (CMake GLOB_RECURSE), Windows/R36S (copy
  lang subdirs), Android (build-apk.sh writes assets/lang/index.txt, the app
  extracts what it lists; replaces the hardcoded .po list).
- EN pictures: bild 11/59 ("Meanwhile", "Hours later"), phase 23.
  Converted from truecolour sources: nearest colour (redmean) restricted to
  palette indices identical in all rooms (0-79, 160-175, 240-245) plus those
  the original used (phases), or the room palette minus the entries LadeBild
  changes (bild); transparent pixels became index 0.
- Phase text tool (2026-10-27): `tools/phasefont.py` + `tools/phasetext/`.
  The red titles (34-36), help/copyright pages (53-68) and credits
  (113-116) all use one font: text in one colour (yellow 51, red 19) and a
  shadow (index 1) that is a fixed dilation of the text pixels, with five
  styles (thick, thin, heavy, heavier, red; detected per line).
  `help.font` holds the glyphs as ASCII art (cut from the originals via
  de.txt, the German transcript; / 2 j fixed and 8 J Q X Y q ! ? ; drawn
  by hand). `en.txt` gives the English per original line index;
  `phasefont.py render` erases those lines (text, shadow, heading
  underline) and draws the new text in the same place and style, keeping
  the picture size. Rendering de.txt reproduces 34-36 exactly and the help
  pages up to hand-tuned spacing.
- EN phases 35, 36, 53-68 and 113 rendered with it (114-116 are already
  English). The graphics inside the help pages (system menu, command line
  bars, save slots, text windows) still show German.
- ES/FR/IT/NL phases rendered the same way from es/fr/it/nl.txt (same
  lines as en.txt, reflowed where the text hit graphics or the edge).
  `phasefont.py accents` composes the accented letters and ¡ ¿ from base
  glyphs plus mark templates. Still missing: ß " « » œ.
- Title cards bild 11 ("Inzwischen..."), bild 59 / phase 23 ("Stunden
  später...") for ES/FR/IT/NL via `tools/titlecard.py`: the hand-drawn
  letters are cut from the DE and EN cards and set again, the shadow is
  regenerated (exact extrusion: 6 left/2 down on bild 59, 4 left/3 down on
  bild 11), background from DE/EN where either has no text. Phase 23 is
  bild 59's text in the window at 34,50 (250x93). Texts: HORAS DESPUÉS /
  MIENTRAS TANTO, PLUS TARD / PENDANT CE TEMPS, ORE DOPO / INTANTO,
  UREN LATER / INTUSSEN. Not tested in game yet.
- Not tested in game yet.

## Later: more translations, accented glyphs (2026-10-22, not started)
- Game font (`CharAvail`, print.c) has A-Z, a-z, ÄÖÜß/äöü, digits and
  `- .!?,:;'"<>()[]`. `utf8_to_cp850()` turns anything else into a space,
  so "café" draws as "caf ".
- Possible with the current font: Dutch (only ë/ï lost, e.g. "ruïne"),
  Finnish (ä/ö only), Indonesian/Malay, Swahili.
- Need new glyphs: Spanish (ñ áéíóú ¿¡), French (éèêàçùôœ...), Portuguese
  (ãõç...), Italian (àèéìòù; "perche'" fallback is ugly), Nordic (åæø).
  Polish/Czech/Turkish need more.
- Plan: don't touch GAME.XXX (copying terms forbid changed data). Compose
  accented glyphs at load time: base letter + accent mark (acute, grave,
  circumflex, tilde, diaeresis, ring, cedilla), appended to the glyph tables
  of all 7 fonts; map them in `utf8_to_cp850` like äöü.
- First step: measure glyph heights. Lowercase likely has room above the
  x-height; capitals may fill the cell. Fallback: no accents on capitals.
- SDL 5x7 menu/overlay font has no lowercase or accents; needs the same.
- Size: 1446 strings, ~7900 words in manick.pot. Translators need context;
  some puzzles/dialogue may rely on German wordplay.

## Version from top-level VERSION file (2026-10-22)
- `VERSION` (`0.1.0-dev`) replaces the hardcoded `0.1.0-sdl` in flake.nix
  and CMakeLists.txt. The flake appends `.<revCount>+g<shortRev>` for `-dev`
  (`self.revCount or 0`; dirty trees get 0 and `-dirty`) and passes
  `-DPROJECT_VERSION_FULL` to every CMake build (desktop, win, r36s, wasm);
  Android still gets it via `GAME_VERSION`/`MOAS_VERSION` (ndk-build falls
  back to `unknown` outside Nix). `project()` gets the numeric part.
- `moas --version`; options menu shows `MOAS <version>` centred on the
  screen just below the panel (the bottom edge has the portrait touch
  buttons). The 5x7 font has no lowercase, so it reads `...-DEV.N+G...`.
- DOS (Watcom) build doesn't use the version string.

## Language packs on all targets, OS default language, key labels (2026-10-22)
- Android never got `lang/en.po`: `nix/android.nix`'s source fileset was
  `src` + `manick` only, so build-apk.sh found no `lang/` (the extract list
  already had `lang/en.po`). Windows, R36S and web didn't ship it either.
  Added `lang/` to all four; `lang.c` now searches `MoasDataDir()/lang`
  instead of only `$MOAS_DATADIR/lang`.
- Default language: `PlatformPreferredLanguage()` (SDL_GetPreferredLocales,
  first entry only): German → de, else en. settings.ini still wins, so an
  existing install that saved `language=de` under the old default stays
  German until changed in F12.
- Key names on the touch buttons only while the options menu is open,
  drawn after its dimming (`overlay_draw_keys`), one size for all buttons;
  Use/Look show LMB/RMB. Use/Look icons are now a hand and an eye.

## Options menu + key legend translatable, MENU button → OPTIONS (2026-10-22)
- SDL options menu (F12) and the start-screen key legend were hardcoded
  English. Now `UI_OPT_*` / `UI_KEY_*` with German fallbacks (msgid) and
  English in en.po, like the other UI strings; manick.pot regenerated
  (`moas-util strings pot` output identical to the file).
- Overlay MENU button renamed OPTIONS to match the window title
  (`ui.overlay.menu` → `ui.overlay.options`).
- The SDL bitmap font has A-Z, 0-9 and `<>+-:/.?` only: German strings use
  UE for Ü, and a translation with other characters would draw blanks.
- Overlay button labels still fall back to English (SAVE, LOAD, ...) in
  German mode; long German words would not fit the buttons.

## Touch overlay: Skip Scene / Skip Text, new order (2026-10-22)
- The touch Esc button only ever aborted armed sequences (`synth_esc` sets
  KeyAbortState/KeyScanCode=1; it does not queue a getch 27, so it never
  closed dialogue choices or waits). Renamed to **Skip Scene** (film-frame
  icon, key hint ESC); Skip is now **Skip Text** (speech-bubble icon).
  UI keys `ui.overlay.esc/skip` → `ui.overlay.skip_scene/skip_text`.
- Landscape right column: Menu, Hint (top) / Skip Scene, Skip Text (bottom).
- Portrait: `ov_portrait()` reserves a top row (Save Load -- Hint Menu) and a
  bottom block of two columns (Use/Look left, Skip Scene/Skip Text right);
  picture centred between them.
- Not checked on a device: the top row sits 8 px from the top edge, which
  may collide with a notch / status bar on Android.

## moas-util unpack / pack (2026-10-22)
- `unpack` writes a source tree (moas.txt catalog, scripts/*.mas, PNGs,
  pck.txt, persons.txt); `pack` rebuilds all data files. With
  `--reference manick` the output is byte-identical (ctest
  `moas-util-roundtrip`); scripts/PCK/persons/catalog are identical anyway.
- Code split into tools/mu_*.c (common, png incl. inflate, gfx codec,
  script asm/disasm, data). `strings` output unchanged (pot/json/tsv
  compared against the old tool).
- Graphics format: header byte 4, code table, tokens (see mu_gfx.c). The
  248..250 ops are never used. Codes = lowest unused pixel values, mapped
  to colours that occur in runs, first-use order (157/235 match exactly).
- Original packer not recovered. Encoder rules found: longest earlier match,
  earliest on ties, source ends >= 2 before and starts >= 7 before the
  current pixel, relative if encodable else absolute; run vs match by
  pixels per byte. 98/235 token streams match; 37/235 byte-identical
  (`moas-util gfx-test`). It's not an optimal parse (original is bigger).
  Unexplained: some matches are cut at a colour change before a run or
  another long match, and some d=L+2 matches next to a preceding run are
  skipped. Next step for full fidelity would be more of that analysis.
- Graphics checksums in GAME.2 don't match the data (engine ignores 1007,
  "FastLink"); pack keeps stored ones for reused graphics, computes new ones.
- PNG reader: indexed only, depth 1/2/4/8, non-interlaced. Walk maps use a
  false-colour palette (bright = walkable), depth maps a grey ramp.
- Checked: re-encoded data (no reference) runs; Xvfb screenshots of the
  intro identical to the original data. Edit test: inserted script line
  shifts labels correctly; edited walk map survives pack+unpack.
- Script edits shift addresses: saves (ontime/script positions) and the
  `ProcNum==1 && Adresse==0x0151` options hook in moas.c depend on them.
- The game's copying terms (intro text) say the data must not be changed
  when passing it on.


## UBSan: Tab320[200] in PowerRedrawArea (2026-10-22)
- A person whose top edge is at y==200 (just below the screen) gave Y1==200,
  Y2==200 after clipping: zero rows to draw, but `Tab320[Y1]` was read out of
  bounds. Skip persons with `Y2<=Y1`; all row loops ran Y2-Y1 times anyway.

## Crash entering the Keller: InternalError 1028 (2026-10-22)
- Diele idle timer (proc 15 @864: `adress 87`, `wait 2`, re-arm) kept running
  after the room changed. During `wait` the player keeps walking; reaching the
  Keller exit fires the Jumper (TestIfPersonIsInTrap) and enters the Keller
  inside the wait. The timer script then addressed person 87 (gone) →
  GetRealPersonNum blocks on getch() ("stuck"), then InternalError 1028.
  Likely latent in the DOS original too.
- Fix (non-DOS): `OntimeEpoch` bumped in InitOntimes(); an Execute started by
  UpDateOntimes aborts after Wait / WaitWhileSomeBodyIsWalking if the epoch
  changed underneath it (its own init_ontimes resyncs).

## moas-util extract-png (2026-10-22)
- `moas-util extract-png [all|bild|inv|phase]` writes `<kind>_NNNN.png`
  (8-bit indexed, own CRC/stored-deflate writer, no zlib dependency).
- Decoder is a bounds-checked copy of `UniversalUnpack`; all 235 graphics
  in Mänick decode without errors.
- Rooms (bild with x&16384) use their trailing palette. Bild 0 (options box)
  and info maps use glass 254; info maps get the preceding room palette.
  Sprites/inventory use the first room palette (or `--palette N`); index 0
  is transparent for them, 255 for bild 0. The engine's palette tweaks in
  `LadeBild` (random colours, 738.. range) are not applied.
- Possible next: per-person palette guess for phases, frame-split output.

## Hotspot highlight: H key and HINT touch button (2026-10-22)
- Holding **H** (or the overlay **HINT** button) outlines everything
  clickable in the room; it stays 1.5 s after release (fading over the last
  300 ms), so a tap is enough. Drawn by `hint_draw()` in platform_sdl.c on
  top of the game picture, not into the 320x200 buffer.
- Areas come from `MoasGetHotspots()` (mouse.c, next to `WhatsAt`): Label
  markers plus persons with a description (bounds of their current frame),
  minus the player, clipped below the open inventory bar. Empty while
  `Window.Status != None` or `PlayerHasControl != 1`. Jumpers (walk-in
  areas) only in master mode, see below.
- H still reaches the game (text entry); LookKeys ignores it. Not taken
  with Alt/Ctrl/GUI. Key legend has an "H SHOW HOTSPOTS" row.
- HINT button: right column above MENU in landscape, the free third slot of
  the lower row in portrait. Light-bulb icon, key "H" shown off-touch.
  New UI string `ui.overlay.hint` ("HINT") in lang.c, moas-util, .pot, en.po.
- Tested under Xvfb with save copies: H shows labels and an NPC, linger and
  fade-out work. The overlay button itself is untested (layout and touch);
  check landscape/portrait on a phone or with `--touch-controls`.
- Gamepad X shows it while held (2026-10-27).
- Master mode also shows Jumper (walk-in) areas in cyan (kind 1 from
  `MoasGetHotspots`). Not in normal play: jumpers are exits but also any
  other walk-in trigger, so rooms like the lab get covered in boxes. Telling
  real exits apart would need a look at each jumper's script.
- `-d`/`--debug` (any form) sets `moas_master`: master mode is on at start
  (`InitSystemVars`, so also after RESTART) and Alt+T works without THPRIV.
  On non-DOS builds a load keeps the current V_Master instead of the saved
  one. The original F1 fall-through into the Alt+T case still toggles it.

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
  (`CurrentProc == 4`, the title room) and while the F12 menu is open. If it
  would overlap the panel it is drawn at a smaller scale, and skipped only if
  it still overlaps at scale 1. (The German labels made the box wider, so it
  used to vanish in DE but show in EN.) Not drawn on touch devices.
- (2026-10-27) The first game click on the start screen (mouse, finger or
  pad trigger) hides the start-screen legend for the rest of the session
  (`keyhints_note_click`), so it no longer covers the help pages and intro
  text. It still shows in the F12 menu. Not tested in game yet.
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

## "NW" leaking into translated say windows (2026-10-22)
- `utf8_to_cp850()` also turned '_' into a space. In the scripts '_' is a hard
  space (drawn blank by CharNum, but not whitespace), and " NW ______ NW "
  is a pause line. With real spaces, ReTextSayWin dropped the first " NW ",
  stripped the leading blanks including the one before the next "NW", and
  printed "NW Hello, I..." (e.g. script.p13.0x02c0, the phone call).
  '_' and '@' now pass through unchanged, as they do in German.
- en.po: 38 msgids (e.g. script.p1.0x08b9 "Beamen") had been replaced by
  English text in 51cb4b0. Script strings are matched by msgid, so those
  lines showed German. Restored from lang/manick.pot; en.po and the .pot
  now agree on every msgctxt/msgid. Also restored dropped NW breaks in
  script.p1.0x0643 / p4.0x0269 and the last line of the sign (p5.0x0142).

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
- MS-DOS: `nix/dos.nix`, `mk/dos/Makefile` — Open Watcom → DOS/4GW `manick.exe`
- WebAssembly: `nix/wasm.nix`, `mk/wasm/` — Emscripten + ASYNCIFY + preload data
- Technique mirrored from Kurvenrausch (Nix flake packages + mk/ tree)
- Windows: `nix/windows.nix` — MinGW + SDL2-win32 flake, win32/win64 zip
- R36S / PortMaster: `nix/r36s.nix`, `mk/r36s/` — eoan arm64 sysroot, launcher
- Android: `nix/android.nix`, `mk/android/` — NDK + SDL2 APK scaffold
- `--fullscreen` / `-f` for handhelds and borderless desktop
- Android: game data packed into APK assets/; extracted to internal storage on start
- Windows: first MinGW build may need small POSIX path fixes — untested here

## Desktop integration
- FreeDesktop `.desktop` file: `data/manick.desktop`
- hicolor icons (16–256 px): large red “M” with protagonist, night path scene
- AppStream metainfo: `data/metainfo/org.grumbel.manick.metainfo.xml`
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
  - Linux: `$XDG_STATE_HOME/manick` (fallback `~/.local/state/manick`)
  - Config: `$XDG_CONFIG_HOME/manick`
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
- Gamepad: sticks cursor, D-pad walk, A/R1 use, B/L1 look, X hint, Y skip text, Back Esc, Start menu, R2/L2 save/load, R3/L3 speed
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

## More languages: es, fr, it, nl (2026-10-22)
- `lang_codes[]` in lang.c (de, en, es, fr, it, nl) is the one list:
  `LangStep()` cycles it in the F12 menu (both directions), `LangFind()` maps
  the OS locale (first choice only) to a pack, anything else stays `en`.
- Android asset list (`android_data_files[]`) now includes all packs; the
  other packages already copy `lang/*.po`.
- The game font has only ÄÖÜß/äöü besides ASCII, and `utf8_to_cp850()` used to
  turn every other accented letter into a space. `latin_fold()` folds
  U+00C0..U+00FF and Œ/œ/Ÿ to base letters (é→e, ñ→n, ç→c, œ→oe); ¡ ¿ are
  dropped, « » become '"'. Covers every non-ASCII char in the current packs.
- Next: proper accented glyphs would need new font data in GAME.* (and the
  5x7 SDL UI font); check long translations against say-window widths.
