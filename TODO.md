# TODO / Handoff

## Base
- Original tip: `8d399e6` (Converted source from Latin1 to UTF-8)
- Work line base for bundles: `8d399e6`
- GitHub tip that included data + previous scaffold: `c92cb55`

## Status (2026-10-18)
Continued from GitHub tree (scaffold + `manick/` game data).

### Done in this session
- Case-insensitive file open (`open_ci`) for DOS-style names on Linux
- `CHKSUM` fixed to `uint32_t` (was 64-bit `unsigned long` → licence/checksum reads failed)
- `S_IWRITE` compatibility define
- BIOS keyboard flag write at 0x417 guarded (`InitVars`)
- Copy-protection disk-ID check skipped on non-DOS (`ChkAuthorisation`)
- **MAPFILE on-disk format**: packed 30-byte records loaded via `MapFileDisk` + conversion
  (`src/mapfile_disk.h`, `LoadMapFileTable` in `init.c`)
- Softened fatal `PrintWord` length check (1014) so script text issues don’t abort
- Binary runs under Xvfb with `manick/` data in cwd; reaches script execution / text output

### Still broken / next
1. Font / character encoding: source was converted to UTF-8; game strings and `CharNum` expect Latin1 / custom encoding → InternalError 1013 and nonsense lengths.
2. Present path: call `PlatformPresent` from `PowerRedrawArea` / palette updates for visible graphics.
3. Sound still stubbed.
4. Header globals still rely on `-fcommon`.
5. Install data path: document `cd manick && ../build/moas` or add a search path.
6. Soft-fail remaining InternalErrors where safe; improve diagnostics.

### How to run (current)
```bash
cmake -B build && cmake --build build
cp manick/* build/   # or run from manick/ with binary on PATH
cd build && xvfb-run -a ./moas   # or with a real DISPLAY
```

## Bundle naming
Next: `moas-008.1-data-load-fixes-8d399e6.bundle`
(revCount: original 5 + scaffold 1 + github data/lock 2 + this work)
