# TODO / Handoff

## Base
- `8d399e6`

## Status
### Mouse / title screen
- `MouseCheck` now calls `PlatformPumpEvents` + Present + Delay (wait loops never pumped events before)
- Button edge: latch `Last*` at end of `MouseCheck` (not on SDL event)
- Mouse coords via `SDL_RenderWindowToLogical` (window → 320×200)

### Stdout umlauts
- `ReadLicence` prints CP850 → UTF-8 for the console only
- In-game CharAvail/font path remains CP850 bytes

### Still open
- Dirty-rect / room graphics completeness
- Sound, modifiers
