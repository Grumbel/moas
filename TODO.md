# TODO / Handoff

## Base
- `8d399e6`

## Program flow
```
main
  MoasInitDataPath
  InitAll
    ReadLicence (stdout CP850→UTF-8)
    alloc Screen/Palette/...
    InitMapData (GAME.2 catalog)
    InitFont (GAME.XXX)
    LoadPack (GAME.PCK)
    PlatformInit (SDL window)
  PlatformPresent
  Execute(3,0)   // intro/title script
  Execute(0,0)   // main game script
  while !PlatformShouldQuit: MouseCheck
  PlatformShutdown
```

Scripts use GetClickOrEnter/Wait/MouseCheck for input. On Linux these were empty
#ifdef DOS stubs — title never waited for clicks.

## Fixes in this bundle
- GetClickOrEnter + Wait implemented for Linux (pump via MouseCheck)
- Button edges: latch Last* at start of MouseCheck (before pump) so release is visible after return
- IveGotTooMuchTime uses GetTicker() not frozen TimeCounter
- PlatformShutdown uses SDL_QuitSubSystem(VIDEO) only (avoids abort in SDL_Quit on corrupted heap)
- PrintSentence strncpy bounds

## Still open
- Root heap corruption (soft quit masks exit abort; ASAN next)
- Dirty-rect / full room graphics
- Sound
