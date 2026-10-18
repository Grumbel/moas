# TODO / Handoff

## Fixed DOS leftovers (this bundle)
- Palette cycle on Linux tick path (was IRQ-only)
- SetMousePos → SDL_WarpMouseInWindow
- Shake → soft present offset
- ToggleScreen → blank flag
- QuickSetSinglePal → Palette + SDL
- Person save/load via PersonDisk (236-byte records)

## Still open
- Full savegame: Marker/Dialog/SayWindow still native sizeof (pointers)
- Sound (SB/DMA stack inert; InitDigi commented out)
- PowerBlend/WalkNLayers
