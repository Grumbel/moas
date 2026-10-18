/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * SDL2 platform layer for MOAS.
 * Provides 320x200x256 software framebuffer presentation, mouse/keyboard
 * mapping into the original structures, and a millisecond ticker.
 */

#include "platform.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Minimal declarations of the globals we touch (defined elsewhere). */
typedef unsigned char uChar;
extern uChar *Screen;
extern unsigned char *Palette;   /* 768 bytes, VGA 0-63 */
extern short KeyAbortState;

struct MOUSE {
  short x,y;
  short Left,Right,Middle;
  short LastLeft,LastRight,LastMiddle;
  short ClickX,ClickY;
  short ClickObject;
  short ReleaseX,ReleaseY;
  short CursorState;
  char BewegeMode;
};
extern struct MOUSE Mouse;

#define LOGICAL_W 320
#define LOGICAL_H 200
#define SCALE     3

static SDL_Window   *window   = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture  *texture  = NULL;
static Uint32       *pixels   = NULL;
static Uint32        start_ticks = 0;
static int           running  = 1;
static Uint32        sdl_palette[256];

int PlatformInit(void)
{
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS) < 0) {
    fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return -1;
  }

  window = SDL_CreateWindow("MOAS",
                            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                            LOGICAL_W * SCALE, LOGICAL_H * SCALE,
                            SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
  if (!window) {
    fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    return -1;
  }

  renderer = SDL_CreateRenderer(window, -1,
                                SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!renderer)
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
  if (!renderer) {
    fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
    return -1;
  }

  SDL_RenderSetLogicalSize(renderer, LOGICAL_W, LOGICAL_H);
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

  texture = SDL_CreateTexture(renderer,
                              SDL_PIXELFORMAT_ARGB8888,
                              SDL_TEXTUREACCESS_STREAMING,
                              LOGICAL_W, LOGICAL_H);
  if (!texture) {
    fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
    return -1;
  }

  pixels = (Uint32 *)malloc(LOGICAL_W * LOGICAL_H * sizeof(Uint32));
  if (!pixels) {
    fprintf(stderr, "Out of memory for pixel buffer\n");
    return -1;
  }

  for (int i = 0; i < 256; i++) {
    Uint8 v = (Uint8)i;
    sdl_palette[i] = 0xFF000000u | (v << 16) | (v << 8) | v;
  }

  start_ticks = SDL_GetTicks();
  running = 1;
  return 0;
}

void PlatformShutdown(void)
{
  if (pixels)   { free(pixels);   pixels   = NULL; }
  if (texture)  { SDL_DestroyTexture(texture);  texture  = NULL; }
  if (renderer) { SDL_DestroyRenderer(renderer); renderer = NULL; }
  if (window)   { SDL_DestroyWindow(window);     window   = NULL; }
  SDL_Quit();
}

void PlatformSetPalette(const unsigned char *pal768)
{
  if (!pal768) return;
  for (int i = 0; i < 256; i++) {
    Uint8 r = (Uint8)(pal768[i * 3 + 0] * 255 / 63);
    Uint8 g = (Uint8)(pal768[i * 3 + 1] * 255 / 63);
    Uint8 b = (Uint8)(pal768[i * 3 + 2] * 255 / 63);
    sdl_palette[i] = 0xFF000000u | (r << 16) | (g << 8) | b;
  }
}

void PlatformPresent(void)
{
  if (!Screen || !pixels || !texture || !renderer) return;

  const unsigned char *src = Screen;
  Uint32 *dst = pixels;
  for (int i = 0; i < LOGICAL_W * LOGICAL_H; i++)
    dst[i] = sdl_palette[src[i]];

  SDL_UpdateTexture(texture, NULL, pixels, LOGICAL_W * sizeof(Uint32));
  SDL_RenderClear(renderer);
  SDL_RenderCopy(renderer, texture, NULL, NULL);
  SDL_RenderPresent(renderer);
}

static void mouse_to_logical(int wx, int wy, short *ox, short *oy)
{
  float lx = (float)wx, ly = (float)wy;
  if (renderer)
    SDL_RenderWindowToLogical(renderer, wx, wy, &lx, &ly);
  if (lx < 0) lx = 0;
  if (ly < 0) ly = 0;
  if (lx > LOGICAL_W - 1) lx = (float)(LOGICAL_W - 1);
  if (ly > LOGICAL_H - 1) ly = (float)(LOGICAL_H - 1);
  *ox = (short)lx;
  *oy = (short)ly;
}

void PlatformPumpEvents(void)
{
  SDL_Event ev;
  while (SDL_PollEvent(&ev)) {
    switch (ev.type) {
      case SDL_QUIT:
        running = 0;
        KeyAbortState = 2;
        break;
      case SDL_MOUSEMOTION: {
        short mx, my;
        mouse_to_logical(ev.motion.x, ev.motion.y, &mx, &my);
        Mouse.x = mx;
        Mouse.y = my;
        break;
      }
      case SDL_MOUSEBUTTONDOWN:
      case SDL_MOUSEBUTTONUP: {
        /* Only update current button state. MouseCheck latches Last* each frame. */
        int down = (ev.type == SDL_MOUSEBUTTONDOWN);
        short mx, my;
        mouse_to_logical(ev.button.x, ev.button.y, &mx, &my);
        Mouse.x = mx;
        Mouse.y = my;
        if (ev.button.button == SDL_BUTTON_LEFT) {
          Mouse.Left = down ? 1 : 0;
          if (down) {
            Mouse.ClickX = Mouse.x;
            Mouse.ClickY = Mouse.y;
          } else {
            Mouse.ReleaseX = Mouse.x;
            Mouse.ReleaseY = Mouse.y;
          }
        } else if (ev.button.button == SDL_BUTTON_RIGHT) {
          Mouse.Right = down ? 1 : 0;
        } else if (ev.button.button == SDL_BUTTON_MIDDLE) {
          Mouse.Middle = down ? 1 : 0;
        }
        break;
      }
      case SDL_KEYDOWN:
        if (ev.key.keysym.sym == SDLK_ESCAPE)
          KeyAbortState = 2;
        break;
      default:
        break;
    }
  }
}

short PlatformGetTicker(void)
{
  Uint32 ms = SDL_GetTicks() - start_ticks;
  return (short)(ms / 55);
}

void PlatformDelay(unsigned int ms)
{
  SDL_Delay(ms);
}
