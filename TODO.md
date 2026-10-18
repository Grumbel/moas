# TODO / Handoff

## Audio (this bundle)
- InitDigi() enabled: volume/step tables + SDL audio @ 14 kHz U8 mono
- DigiMixFrame reimplements tracker mix; SDL callback streams SampleBuffer2
- LoadMusic: SampleDisk (52-byte) load; data path under MoasDataDir
- Op 95 LoadMusic enabled
- No PIZZZA*.TSS in manick/ — music silent until data present

## Timing
- Ticker back to PIT divisor 0x1FFF (~145.67 Hz). Evidence: IRQ lets the BIOS
  clock advance every 8th tick, game second = 146 ticks, intro waits 17*64 ticks.
- IveGotTooMuchTime: original semantics, one step per call (no catch-up);
  speech bubbles every 4th tick again. Palette cycle still runs per tick.
- Wait()/shake use real 100 ms units (PlatformGetMillis).
- Verify by playing: walking speed vs TotalSpeed, intro star/fade timing.

## Still open
- Speech channel (SpeechData) wiring if used
- PowerBlend / WalkNLayers
