# DOS `#ifdef` inventory and port status

Every `#ifdef DOS` / `#ifndef DOS` in `src/`, reviewed against the SDL2 platform layer.
Status tags:

- **PORTED** — non-DOS path exists and is intended to be equivalent
- **STUB/NO-OP** — DOS side disabled; behaviour intentionally absent or harmless
- **GAP** — DOS behaviour not replaced; visible or functional difference possible
- **DEAD/DEBUG** — only used for developer tools or copy-protection; low priority
- **HEADER-ONLY** — declarations / macros, no runtime gap by itself

Last reviewed with tip that includes the Bewege arrow cursor fix (`VMouse` + `Phase[1]`).

---

## `src/moas.h`

| Lines | What | Status | Notes |
|------:|------|--------|-------|
| 39–44 | `VGA_ADRESSE` → `0xa0000` vs `Display` | PORTED | Composited frame buffer is the VGA plane stand-in. |
| 56–60 | `Clearscreen()` | PORTED | Clears `Screen`. |
| 98–102 | `OutpWait` (SoundBlaster busy-wait) | STUB/NO-OP | No SB ports on SDL. |
| 345–347 | `MySoundIrq` decl | HEADER-ONLY | Body only under DOS in `digi_io.c`. |
| 399–403 | Saved IRQ vector pointers | HEADER-ONLY | Used only by DOS init/teardown. |
| 543–545 | `KeyboardIrq` decl | HEADER-ONLY | |
| 547–550 | `MySoundIrq` / `MyTickerIrq` decls | HEADER-ONLY | |

---

## `src/compat.h`

| Lines | What | Status | Notes |
|------:|------|--------|-------|
| 10–34 | `inp`/`outp`, `_disable`/`_enable`, `kbhit`/`getch`, min/max | PORTED | Port I/O stubs return 0; keyboard via SDL queue. |
| 38–61 | `itoa`/`ltoa`/`filelength`/`_setcolor`/`_memmax` | PORTED | `_setcolor` is a no-op (only used with DOS `_rectangle` debug). |

---

## `src/moas.c`

| Lines | What | Status | Notes |
|------:|------|--------|-------|
| 58–71 | Arg parse, Android assets, data path | PORTED | DOS had no argv path setup. |
| 76–78 | `PlatformPresent()` after init | PORTED | |
| 92–102 | Main loop + quit + shutdown | PORTED | |

---

## `src/init.c`

| Lines | What | Status | Notes |
|------:|------|--------|-------|
| 48–50 | DigiStartSdlAudio decl | PORTED | |
| 54–58 | PlatformInit before heavy init | PORTED | |
| 61–65 | Allocate `Display` buffer | PORTED | |
| 71–73 | Start SDL audio after PlatformInit | PORTED | |
| 81–86 | `_setvideomode(_MRES256COLOR)` | PORTED | Replaced by PlatformInit/SDL window. |
| 91–112 | `DosGetDiskIdNum` (INT 21h disk serial) | DEAD/DEBUG | Only used by DOS copy-check. |
| 146–151 | BIOS timer seed for `rand` | STUB/NO-OP | Non-DOS relies on default/`srand` elsewhere if any. |
| 162–175 | Install KeyboardIrq / MyTickerIrq, ignore SIGBREAK | PORTED | Timer/keyboard via SDL; no IRQ install. |
| 240–242 | Clear `Display` | PORTED | |
| 253–255 | Missing GAME.XXX report | PORTED | |
| 365–367 | Disk-id copy protection | DEAD/DEBUG | Skipped on non-DOS (intentional). |
| 372–401 | CP850→UTF-8 console print helper | PORTED | Console only. |

---

## `src/mouse.c`

| Lines | What | Status | Notes |
|------:|------|--------|-------|
| 49–… | MouseCheck: pump events, restore CursorState, present/delay | PORTED | |
| 115–… | Present/delay at end of MouseCheck | PORTED | |
| 122–… | `IsMousePresent` INT 33h | PORTED | Always present under SDL. |
| 141–… | `MouseRead` INT 33h relative mouse | PORTED | Position/buttons from PlatformPumpEvents. |
| 532–541 | `SetMousePos` INT 33h vs `PlatformSetMousePos` | PORTED | |

`VMouse` (no longer `#ifdef DOS`) draws both `Phase[0]` and `Phase[1]` (Bewege arrow) — **was GAP, now PORTED**.

---

## `src/grafix.c`

| Lines | What | Status | Notes |
|------:|------|--------|-------|
| 30–32 | include platform.h | PORTED | |
| 176–179 | `VMouse(3)` before redraw | PORTED | Soft-cursor must leave Screen clean. |
| 331–366 | Bake cursor into redraw (Phase[0] + Phase[1]/Bewege) | PORTED (via VMouse) | DOS baked into dirty region; SDL uses save/restore VMouse after blit. Functionally covered. |
| 372–376 | `VMouse(1)` after blit | PORTED | |
| 380–414 | `Shake` CRTC start-address vs PlatformSetShake | PORTED | Timing uses ~100 ms units like Wait. |
| 423–435 | `QuickSetSinglePal` VGA DAC vs Palette[] + PlatformSetPalette | PORTED | |
| 441–447 | `QuickSetPalette` | PORTED | |
| 476–… / 491–… | Blend loop pump/present/delay | PORTED | |

### Not under `#ifdef DOS` but still DOS-only behaviour

| Location | What | Status | Notes |
|----------|------|--------|-------|
| `ToggleScreen` (~500) | VGA sequencer blank bit via `outp(0x3c4/0x3c5)` | **PORTED** | Non-DOS: `PlatformSetBlank(Mode != 1)` + present. Mode==1 = screen on. |

---

## `src/io.c`

| Lines | What | Status | Notes |
|------:|------|--------|-------|
| 32–34 | (empty / include guard area) | — | |
| 38–40 | RealIrq* definitions | HEADER-ONLY | |
| 42–278 | Packed disk layouts for save/load (Write/Read*Disk) | PORTED | Avoids ABI packing differences vs DOS structs. |
| 294–298 / 301–305 | open data file / missing-data report | PORTED | `MoasOpenData`. |
| 470–472 | Log data dir | PORTED | |
| 486–490 | Path naming helper | PORTED | |
| 518–520 / 638–640 | `O_BINARY` on open | PORTED | Not needed on POSIX. |
| 571–702 | Save/load use packed disk helpers | PORTED | |
| 773–775 | `_rectangle` fill in some UI | STUB/NO-OP | Visual debug-ish; `_rectangle` only under DOS. |
| 812–868 | `MyTickerIrq` body (PIT, TimeCounter, palette cycle call) | PORTED | Logic moved to `misc.c` / `PlatformGetTicker` path (`IveGotTooMuchTime` etc.). |
| 870–883 | `KeyboardIrq` (scancode → Key buffer) | PORTED | SDL scancode map in platform_sdl.c. |
| 887–892 | `GetTicker` DOS TimeCounter vs PlatformGetTicker | PORTED | |

---

## `src/misc.c`

| Lines | What | Status | Notes |
|------:|------|--------|-------|
| 45–47 | Restore text mode on InternalError | STUB/NO-OP | |
| 67–71 | Restore PIT + IRQ vectors on error | STUB/NO-OP | |
| 107–111 | BIOS timer + Ctrl flag for LookKeys | PORTED | Platform ticker / PlatformCtrlDown equivalents in `#else`. |
| 139–141 | Ctrl key check | PORTED | |
| 247–… | Various Wait / timer loops | PORTED | PlatformGetMillis / MouseCheck. |
| 262, 272, 287, 315 | DOS timer spin vs platform waits | PORTED | |
| 342–… | Non-DOS Wait helpers | PORTED | |
| 407–… | Wait(Milliseconds) | PORTED | 100 ms units. |
| 441–… | GetClickOrEnter DOS clock vs platform | PORTED | |
| 831–842 | GetRealPersonNum debug: text mode + getch | DEAD/DEBUG | Still prints; video mode toggles skipped. |
| 873–… | `ShowMarker` outline Label/Jumper rects | PORTED | Master key `2`: draws into `VGA_ADRESSE` + present. |
| 943–945 | Quit check in wait loops | PORTED | |

---

## `src/digi_io.c`

| Lines | What | Status | Notes |
|------:|------|--------|-------|
| 32–36 | SDL/datapath includes | PORTED | |
| 43–80 | SDL audio device + DigiStart/Stop | PORTED | |
| 119–124 | Skip GetEnvirenment/InitDSP (SB detect) | PORTED | No hardware. |
| 254–267 | D32DosMemAlloc DPMI vs malloc | PORTED | |
| 274–283 | Packed SampleDisk struct | PORTED | |
| 289–293 | Open music file | PORTED | |
| 296–298 | Optional music missing is OK | PORTED | |
| 313–337 | Load sample table (raw DOS struct vs packed disk) | PORTED | |
| 341–344 | LoadMusic log | PORTED | |
| 347–429 | DigiMixFrame + SDL callback (was IRQ body) | PORTED | Tracker mix without DMA. |
| 431–555 | Full `MySoundIrq` + SB/DMA programming | STUB/NO-OP | Correctly left DOS-only; replaced by DigiMixFrame. |
| 568–611 | StartSoundProcessing DMA kick vs SDL_PauseAudioDevice | PORTED | |

### Related (not always under `#ifdef DOS`)

| Topic | Status | Notes |
|-------|--------|-------|
| Speech channel (`SpeechData` / channels 8–9 in digicore) | **GAP** (possible) | Globals exist; loader/wiring if scripts use speech still listed open in TODO. |
| Music banks (`GAME*.TSS` / PIZZZA*.TSS) | Data, not code | Silent until files present. |

---

## `src/chk.c`

| Lines | What | Status | Notes |
|------:|------|--------|-------|
| 90–94 | open READ → MoasOpenData | PORTED | |
| 96–100 | open WRITE without O_BINARY | PORTED | |

---

## `src/print.c`

| Lines | What | Status | Notes |
|------:|------|--------|-------|
| 452–456 | Quit during print/wait | PORTED | |
| 678–682 | Shift state from BIOS 0x417 vs PlatformShiftDown | PORTED | |
| 744–746 | Quit check | PORTED | |

---

## `src/powerbld.c`

| Lines | What | Status | Notes |
|------:|------|--------|-------|
| 50–55 / 65–70 | Wait on BIOS timer tick vs PlatformDelay(16) | PORTED | Timing of blend steps approximate. |

`PowerBlend` / `WalkNLayers`: WalkNLayers is never allocated/loaded and script op 11 is commented out in upstream `moas.c`. Effect is inert. Loop presents each step on SDL when data exists.

---

## `src/hercbak.c`

| Lines | What | Status | Notes |
|------:|------|--------|-------|
| 36–… | Text-mode video for debug HUD | PORTED | Master key `#`: DOS keeps 0xB8000 HUD; non-DOS dumps person/say-window table to stderr, then waits for a key. |

---

## `src/debug.c`

| Lines | What | Status | Notes |
|------:|------|--------|-------|
| 118–120 | `--datadir=` via setenv | PORTED | |

---

## `src/platform_sdl.c` / `platform.h`

Not DOS ifdefs; the replacement layer. Notable ready hooks not yet wired from game code:

- `PlatformSetBlank` — wired from `ToggleScreen`
- `PlatformSetShake` — already used from `Shake`
- `PlatformGetTicker` / `PlatformGetMillis` — used from timer/Wait paths

---

## Summary: remaining real gaps

1. **Copy-protection / disk serial** — intentionally not ported.
2. Speech/audio — not required for this game data (skipped).
3. **PowerBlend** — upstream-disabled (op 11 commented, no WalkNLayers data).

ToggleScreen, ShowMarker, ShowHercBak, Bewege cursor, and TeakEffekt are ported.
Everything else under `#ifdef DOS` is either replaced by the platform layer, intentionally stubbed (SB/DMA/IRQ), or debug/error-path only.

## Mouse under DOS / DOSBox (2026-10)

Original `MouseRead` pins the INT 33 cursor at (100,100), reads the delta,
and divides by adaptive `MouseDivX/Y` (starts 30000, shrinks to min nonzero
delta). On real hardware that yields a usable scale. Under DOSBox/DOSBox-X the
host reports large pixel-like deltas so Div collapses to **1** → hyperspeed
(same behaviour with the historical `GAME.EXE`).

Mitigations in this tree:
- Floor `MouseDiv` at 4 so emulators stay playable
- Init `x,y` to 0 when the driver is absent (was uninitialised)
- DOSBox-X runner: `sensitivity=40`, `autolock=true`, `mouse_emulation=locked`

Soft cursor is still drawn by **PowerRedrawArea** (baked into the blit) when
`Window.Status==None`, not by a hardware sprite. Capture the mouse in the
emulator (click the window / Ctrl-F10 depending on build) so INT 33 receives
motion.

## Graphics corruption under DOS rebuild

Original `InitAll` had `//InitDigi()` commented out. The port called `InitDigi()`
unconditionally, which under DOS allocates large **DOS conventional** buffers
via DPMI `0x0100` and talks to the Sound Blaster. That can exhaust low memory
or interfere with the VGA path under DOSBox and produce scrambled graphics.

DOS builds again skip `InitDigi()` (SDL audio remains for non-DOS).

## Watcom globals (cursor corruption)

Open Watcom does not merge tentative common definitions the way GCC `-fcommon`
does. The linker warned `redefinition of _Phase` / `_Mouse` ignored — each
object file could keep a private BSS copy. `LadePhase` then filled one `Phase`
table while `PowerRedrawArea` in grafix drew the cursor from another (empty)
table → corrupted soft cursor; input still used mouse.c’s `Mouse` (coordinates OK).

DOS builds now use the same pattern as Emscripten: `moas_globals.c` defines
storage (`MOAS_DEFINE_GLOBALS`); every other TU sees `extern` via `MOAS_G`.

## Soft-cursor garbage (Glass=254)

`UniversalUnpack(..., Glass=254)` does **not** write transparent texels; it
leaves `Target[tCnt]` unchanged. `LadePhase` allocated with `malloc` and did
not zero the buffer. On Linux the heap is often already zero; Open Watcom’s
DOS heap is not — Phase[0]/[1] kept random bytes, and `if (PrsData[x])` in
`PowerRedrawArea` drew them as multicoloured horizontal bars.

Fix: `memset` the destination to 0 before Glass=254 unpack (phases, inventar,
sprite Bilder).

## DOS MAPFILE load (64-bit port divergence)

Host builds cannot `ChkRead(sizeof(MAPFILE))` because `long` is 64-bit and
`sizeof(MAPFILE) != 30`. They use `MapFileDisk` (fixed int32 fields).

DOS/Watcom must keep **`#pragma pack(1)` on `struct MAPFILE`** so
`sizeof(MAPFILE)==30` and use the **original** load:

```c
ChkRead(ifil, Phase, AnzPhasen * sizeof(struct MAPFILE));
```

Field-by-field MapFileDisk should be equivalent if packing is correct; the
native path removes that variable. After `LadePhase(0)` we also check that
frame 0 has a plausible non-zero texel count (~148 expected).
