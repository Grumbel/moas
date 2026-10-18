# TODO / Handoff

## Input / cursor
- MouseRead was empty on Linux → software cursor never redrawn (DOS used PowerRedrawArea/VMouse)
- Now redraws on move; SpaceState ORs into left button
- V_CursorState=1 and BewegeMode=-1 defaults after init
- GetClickOrEnter accepts Space/Enter via SpaceState

## Still open
- Title may still need a specific click target / script path
- Heap corruption root
- Graphics completeness
