# TODO / Handoff

## Struct audit
- MAPFILE: fixed (30-byte disk)
- PERSON: fixed (236-byte disk, stride 240)
- Save/load still uses native sizeof(Person)/Dialog — broken for old saves on 64-bit

## Trails
Screen was also VGA_ADRESSE → PowerRedraw baked sprites into the background.
Now: Screen = clean BG, Display = composite (VGA_ADRESSE), SDL presents Display.

## Timing
- Guard PhaseSpeed/BaseSpeed == 0
- Single Walker step per DOS tick (removed duplicate Walker++)
- Ticker still ms/55 (~18.2 Hz)
