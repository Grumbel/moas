# MOAS - Mouse Orientiertes Adventure System

Point & click adventure game engine / game from ~1992-2000 by Thomas Holz.
Original target: DOS (VGA Mode 13h 320x200x256, SoundBlaster, custom mouse/keyboard IRQs).

## Current goal
Port to modern Linux using:
- SDL2 for video, input, timer, (eventually audio)
- CMake for the build system
- Nix flake for reproducible builds / NixOS packaging

## Standing rules
- GPLv2+ (keep existing license; COPYING is GPLv2)
- Prefer clean abstractions over `#ifdef` soup. Introduce a small platform layer.
- No parallel histories. Agents with repository access commit directly;
  only Grok Web, which can't, delivers cumulative git bundles instead.
- Author commits as: Ingo Ruhnke <grumbel@gmail.com>
- Trailer: one Co-authored-by line naming the model that actually did the
  work, e.g. `Co-authored-by: Claude Opus 5.5 <noreply@anthropic.com>` or
  `Co-authored-by: Grok <grok@x.ai>`.
- Document progress and next steps in TODO.md after every meaningful change.
- Always commit when a piece of work is done, without waiting to be asked.
  It can be amended later; uncommitted work just causes trouble.
- Keep the original game logic intact; only replace the DOS hardware layer.

## Architecture notes (as of port start)
- Core is a bytecode / script interpreter driving rooms, persons, inventory, dialogs.
- Graphics: software framebuffer `Screen` (and Info1/Info2) at 320x200, 256-color palette.
  PowerRedrawArea / similar copy dirty regions to the physical VGA plane.
- Input: polled mouse struct + IRQ keyboard.
- Sound: SoundBlaster DSP + DMA sample playback (complex; stub initially).
- Timing: custom timer IRQ ticking `TimeCounter`.

## Platform layer (planned)
- `platform.h` / `platform_sdl.c`: SDL window + texture (or surface), event pump,
  mouse state mapping into the existing `Mouse` struct, keyboard scancode map,
  `GetTicker()` based on SDL ticks, present function that converts palette + Screen
  to RGB and uploads.
- Replace all `inp`/`outp`/`_dos_*`/`int386` with either no-ops or platform calls.
- `D32DosMemAlloc` → ordinary `malloc` (already partially prepared).
