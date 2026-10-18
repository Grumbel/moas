# TODO / Handoff

## Base
- Original tip: `8d399e6`
- Work line base for bundles: `8d399e6`

## Status (2026-10-18)
### Encoding / umlauts
Root cause: Latin1→UTF-8 conversion of sources mangled `CharAvail[]`. Game data and font use **CP850 single bytes** (Ä=0x8E, Ö=0x99, Ü=0x9A, ß=0xE1, ä=0x84, ö=0x94, ü=0x81).

Fixed:
- `CharAvail` restored as explicit `\x..` CP850 bytes
- `CharNum` no longer aborts on unknown glyphs; casts use `unsigned char`
- Remaining BIOS peeks (`0x417`, `0x46C`) guarded — were segfaulting after text path progressed further

Console licence line may still look wrong on UTF-8 terminals (file is CP850); in-game font path is what matters.

### Still open
1. PlatformPresent on dirty redraws (visible rooms)
2. SDL shift modifiers → Extra for say-window timing
3. Sound stub
4. Optional: convert licence printf via iconv for the terminal

## Bundle
`moas-011.1-cp850-charavail-8d399e6.bundle`
