# TODO / Handoff

## Base
- `8d399e6`

## Status
### Latest
- Soft-skip empty map stubs (`Length<=0` / `FileNum<1 && Length<=1`)
- `LadePhase`/`LadeProgramm` ignore `Num < 0` (unused persons had PhaseNum=-1 → 1022)
- Dialog structs fully zeroed on init
- Data-dir diagnostics + debug symbols (prior)

Smoke test with `MOAS_DATADIR=manick` runs past licence without fatal load errors.

### Still open
- Visible graphics (present on redraw)
- Confirm in-game umlauts / no more FileNum=0 from real loads
- Sound, SDL modifiers

## Bundle
`moas-013.1-empty-load-skip-8d399e6.bundle`
