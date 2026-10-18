# TODO / Handoff

## Base
- `8d399e6`

## Status
### Heap / quit crash
`corrupted size vs. prev_size` on SDL_Quit was heap damage from earlier:

- `AddSayWindow`: `MALLOC(strlen(Text))` without `+1` for NUL (and similar strcat paths)
- `UnPack`: unbounded `strcpy`/`strcat` into 300-byte static buffer
- Tiny script stubs: pad allocation by 4 bytes

`PlatformShutdown` is idempotent and tears down SDL before freeing `pixels`.

### Still open
- Dirty-rect / room graphics
- Sound
- More ASAN-driven cleanup if needed
