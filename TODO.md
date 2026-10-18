# TODO / Handoff

## Mouse scaling
Removed SDL_RenderSetLogicalSize. Present and mouse share letterbox_geom() based on
SDL_GetRendererOutputSize. Mouse position re-sampled every pump via SDL_GetMouseState.

## Still open
- Title screen
- Heap root cause
