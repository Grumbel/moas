# TODO / Handoff

## Base
- `8d399e6`

## Status
### Quit
- `SDL_QUIT` was setting `running=0` but main was `while(1)` — never exited
- Now: `while (!PlatformShouldQuit())`, then `PlatformShutdown()`
- Quit via: window close, **Q**, **Ctrl+C**, **Alt+F4**
- **Esc** still only sets `KeyAbortState=2` (script abort), as in original

### Still open
- Dirty-rect / full room graphics
- Sound
