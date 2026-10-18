# TODO / Handoff

## Cursor trail
On DOS, Screen (system) and VGA (0xa0000) were separate. Here VGA_ADRESSE==Screen,
so compositing the cursor into Screen made PowerRedrawArea re-copy old cursors → trail.

Fix: VMouse save/restore for soft cursor; PowerRedrawArea undraws cursor first and
does not bake cursor into Screen on Linux.

## Mouse coords
Letterbox-aware window→320×200 mapping (not relying on RenderWindowToLogical alone).

## Still open
- Title screen progression
- Heap corruption root
