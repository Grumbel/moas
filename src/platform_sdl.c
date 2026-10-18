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
extern uChar *Display;
extern unsigned char *Palette;   /* 768 bytes, VGA 0-63 */
extern char KeyAbortState;
extern unsigned char KeyScanCode;


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
static int           moas_shift_down = 0;
static int           shake_ox = 0, shake_oy = 0;
static int           screen_blank = 0;
static Uint32        sdl_palette[256];

int PlatformShiftDown(void)
{
  return moas_shift_down;
}


/* DOS-style keyboard: scancode make/break + conio queue */
#define KEYQ_SIZE 32
static unsigned char key_queue[KEYQ_SIZE];
static int key_qhead = 0, key_qtail = 0;

static void key_enqueue(unsigned char c)
{
  int next = (key_qtail + 1) % KEYQ_SIZE;
  if (next == key_qhead) return; /* full */
  key_queue[key_qtail] = c;
  key_qtail = next;
}

int PlatformKbhit(void)
{
  return key_qhead != key_qtail;
}

int PlatformGetch(void)
{
  unsigned char c;
  /* Blocks like conio getch(); keeps the window alive while waiting. */
  while (key_qhead == key_qtail) {
    PlatformPumpEvents();
    if (PlatformShouldQuit())
      return 27; /* ESC: lets DOS-style menu loops exit */
    PlatformPresent();
    SDL_Delay(10);
  }
  c = key_queue[key_qhead];
  key_qhead = (key_qhead + 1) % KEYQ_SIZE;
  return (int)c;
}

/* Map common SDL scancodes to XT/AT set-1 make codes used by LookKeys. */
static int sdl_to_dos_scancode(SDL_Scancode sc)
{
  switch (sc) {
    case SDL_SCANCODE_ESCAPE: return 1;
    case SDL_SCANCODE_1: return 2;
    case SDL_SCANCODE_2: return 3;
    case SDL_SCANCODE_3: return 4;
    case SDL_SCANCODE_4: return 5;
    case SDL_SCANCODE_5: return 6;
    case SDL_SCANCODE_6: return 7;
    case SDL_SCANCODE_7: return 8;
    case SDL_SCANCODE_8: return 9;
    case SDL_SCANCODE_9: return 10;
    case SDL_SCANCODE_0: return 11;
    case SDL_SCANCODE_TAB: return 15;
    case SDL_SCANCODE_Q: return 16;
    case SDL_SCANCODE_W: return 17;
    case SDL_SCANCODE_E: return 18;
    case SDL_SCANCODE_R: return 19;
    case SDL_SCANCODE_T: return 20;
    case SDL_SCANCODE_Y: return 21;
    case SDL_SCANCODE_U: return 22;
    case SDL_SCANCODE_I: return 23;
    case SDL_SCANCODE_O: return 24;
    case SDL_SCANCODE_P: return 25;
    case SDL_SCANCODE_RETURN: return 28;
    case SDL_SCANCODE_A: return 30;
    case SDL_SCANCODE_S: return 31;
    case SDL_SCANCODE_D: return 32;
    case SDL_SCANCODE_F: return 33;
    case SDL_SCANCODE_G: return 34;
    case SDL_SCANCODE_H: return 35;
    case SDL_SCANCODE_J: return 36;
    case SDL_SCANCODE_K: return 37;
    case SDL_SCANCODE_L: return 38;
    case SDL_SCANCODE_Z: return 44;
    case SDL_SCANCODE_X: return 45;
    case SDL_SCANCODE_C: return 46;
    case SDL_SCANCODE_V: return 47;
    case SDL_SCANCODE_B: return 48;
    case SDL_SCANCODE_N: return 49;
    case SDL_SCANCODE_M: return 50;
    case SDL_SCANCODE_SPACE: return 57;
    case SDL_SCANCODE_F1: return 59;
    case SDL_SCANCODE_F2: return 60;
    case SDL_SCANCODE_F3: return 61;
    case SDL_SCANCODE_F4: return 62;
    case SDL_SCANCODE_F5: return 63;
    case SDL_SCANCODE_F6: return 64;
    case SDL_SCANCODE_F7: return 65;
    case SDL_SCANCODE_F8: return 66;
    case SDL_SCANCODE_F9: return 67;
    case SDL_SCANCODE_F10: return 68;
    case SDL_SCANCODE_HOME: return 71;
    case SDL_SCANCODE_UP: return 72;
    case SDL_SCANCODE_PAGEUP: return 73;
    case SDL_SCANCODE_LEFT: return 75;
    case SDL_SCANCODE_KP_5: return 76;
    case SDL_SCANCODE_RIGHT: return 77;
    case SDL_SCANCODE_END: return 79;
    case SDL_SCANCODE_DOWN: return 80;
    case SDL_SCANCODE_PAGEDOWN: return 81;
    case SDL_SCANCODE_INSERT: return 82;
    case SDL_SCANCODE_DELETE: return 83;
    default: return -1;
  }
}

/* Extended getch second byte for arrows/function (DOS: getch()==0 then this). */
static int dos_ext_getch(SDL_Scancode sc)
{
  switch (sc) {
    case SDL_SCANCODE_UP: return 72;
    case SDL_SCANCODE_DOWN: return 80;
    case SDL_SCANCODE_LEFT: return 75;
    case SDL_SCANCODE_RIGHT: return 77;
    case SDL_SCANCODE_HOME: return 71;
    case SDL_SCANCODE_END: return 79;
    case SDL_SCANCODE_PAGEUP: return 73;
    case SDL_SCANCODE_PAGEDOWN: return 81;
    case SDL_SCANCODE_INSERT: return 82;
    case SDL_SCANCODE_DELETE: return 83;
    case SDL_SCANCODE_F1: return 59;
    case SDL_SCANCODE_F2: return 60;
    case SDL_SCANCODE_F3: return 61;
    case SDL_SCANCODE_F4: return 62;
    case SDL_SCANCODE_F5: return 63;
    case SDL_SCANCODE_F6: return 64;
    case SDL_SCANCODE_F7: return 65;
    case SDL_SCANCODE_F8: return 66;
    case SDL_SCANCODE_F9: return 67;
    case SDL_SCANCODE_F10: return 68;
    default: return -1;
  }
}

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

  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
  /* No SDL_RenderSetLogicalSize — we letterbox manually so mouse and video share math. */

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

int PlatformShouldQuit(void)
{
  return running ? 0 : 1;
}

void PlatformRequestQuit(void)
{
  running = 0;
  KeyAbortState = 2;
}

void PlatformShutdown(void)
{
  static int done = 0;
  if (done) return;
  done = 1;
  running = 0;

  /* Destroy SDL objects before freeing our buffers. */
  if (texture)  { SDL_DestroyTexture(texture);  texture  = NULL; }
  if (renderer) { SDL_DestroyRenderer(renderer); renderer = NULL; }
  if (window)   { SDL_DestroyWindow(window);     window   = NULL; }
  if (pixels)   { free(pixels);   pixels   = NULL; }
  /*
   * Full SDL_Quit() walks SDL's allocator and aborts if the game already
   * corrupted the heap. Tear down video only; process exit reclaims the rest.
   */
  SDL_QuitSubSystem(SDL_INIT_VIDEO);
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

/* Shared letterbox geometry for present + mouse (output-pixel space). */
static void letterbox_geom(int *out_x, int *out_y, int *out_w, int *out_h,
                           int *out_ww, int *out_wh)
{
  int ww = LOGICAL_W * SCALE, wh = LOGICAL_H * SCALE;
  if (renderer)
    SDL_GetRendererOutputSize(renderer, &ww, &wh);
  else if (window)
    SDL_GetWindowSize(window, &ww, &wh);
  if (ww < 1) ww = 1;
  if (wh < 1) wh = 1;

  float sx = (float)ww / (float)LOGICAL_W;
  float sy = (float)wh / (float)LOGICAL_H;
  float scale = (sx < sy) ? sx : sy;
  int dw = (int)(LOGICAL_W * scale + 0.5f);
  int dh = (int)(LOGICAL_H * scale + 0.5f);
  int dx = (ww - dw) / 2;
  int dy = (wh - dh) / 2;

  if (out_x) *out_x = dx;
  if (out_y) *out_y = dy;
  if (out_w) *out_w = dw;
  if (out_h) *out_h = dh;
  if (out_ww) *out_ww = ww;
  if (out_wh) *out_wh = wh;
}

void PlatformPresent(void)
{
  SDL_Rect dst;
  int dx, dy, dw, dh;

  if (!pixels || !texture || !renderer) return;
  {
    const unsigned char *src = Display ? Display : Screen;
    if (!src) return;
    Uint32 *d = pixels;
    for (int i = 0; i < LOGICAL_W * LOGICAL_H; i++)
      d[i] = sdl_palette[src[i]];
  }

  SDL_UpdateTexture(texture, NULL, pixels, LOGICAL_W * (int)sizeof(Uint32));
  letterbox_geom(&dx, &dy, &dw, &dh, NULL, NULL);
  dst.x = dx; dst.y = dy; dst.w = dw; dst.h = dh;

  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderClear(renderer);
  if (!screen_blank) {
    dst.x += shake_ox;
    dst.y += shake_oy;
    SDL_RenderCopy(renderer, texture, NULL, &dst);
  }
  SDL_RenderPresent(renderer);
}

void PlatformSetShake(int ox, int oy)
{
  shake_ox = ox;
  shake_oy = oy;
}

void PlatformSetBlank(int blank)
{
  screen_blank = blank ? 1 : 0;
}

void PlatformSetMousePos(short x, short y)
{
  int dx, dy, dw, dh, ww, wh;
  if (!window) return;
  if (x < 0) x = 0;
  if (y < 0) y = 0;
  if (x > LOGICAL_W - 1) x = LOGICAL_W - 1;
  if (y > LOGICAL_H - 1) y = LOGICAL_H - 1;
  Mouse.x = x;
  Mouse.y = y;
  letterbox_geom(&dx, &dy, &dw, &dh, &ww, &wh);
  {
    int wx = dx + (int)((float)x * (float)dw / (float)LOGICAL_W);
    int wy = dy + (int)((float)y * (float)dh / (float)LOGICAL_H);
    SDL_WarpMouseInWindow(window, wx, wy);
  }
}

static void mouse_to_logical(int wx, int wy, short *ox, short *oy)
{
  int dx, dy, dw, dh;
  float lx, ly;

  letterbox_geom(&dx, &dy, &dw, &dh, NULL, NULL);
  if (dw < 1) dw = 1;
  if (dh < 1) dh = 1;

  lx = ((float)wx - (float)dx) * (float)LOGICAL_W / (float)dw;
  ly = ((float)wy - (float)dy) * (float)LOGICAL_H / (float)dh;

  if (lx < 0) lx = 0;
  if (ly < 0) ly = 0;
  if (lx > LOGICAL_W - 1) lx = (float)(LOGICAL_W - 1);
  if (ly > LOGICAL_H - 1) ly = (float)(LOGICAL_H - 1);
  *ox = (short)lx;
  *oy = (short)ly;
}

/* Poll OS mouse in window/client coords and map to 320x200. */
static void sync_mouse_position(void)
{
  int wx, wy;
  short mx, my;
  SDL_GetMouseState(&wx, &wy);
  mouse_to_logical(wx, wy, &mx, &my);
  Mouse.x = mx;
  Mouse.y = my;
}

void PlatformPumpEvents(void)
{
  SDL_Event ev;
  while (SDL_PollEvent(&ev)) {
    switch (ev.type) {
      case SDL_QUIT:
        PlatformRequestQuit();
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
      case SDL_KEYUP: {
        SDL_Keycode k = ev.key.keysym.sym;
        SDL_Keymod mod = (SDL_Keymod)ev.key.keysym.mod;
        SDL_Scancode sc = ev.key.keysym.scancode;
        int dos_sc = sdl_to_dos_scancode(sc);
        int down = (ev.type == SDL_KEYDOWN);
        moas_shift_down = (mod & KMOD_SHIFT) ? 1 : 0;

        if (dos_sc >= 0) {
          /* XT make / break (+128) — LookKeys reads this every frame */
          KeyScanCode = (unsigned char)(down ? dos_sc : (dos_sc + 128));
        }

        if (down) {
          if (k == SDLK_ESCAPE)
            KeyAbortState = 2;
          if (k == SDLK_q ||
              (k == SDLK_c && (mod & KMOD_CTRL)) ||
              (k == SDLK_F4 && (mod & KMOD_ALT)))
            PlatformRequestQuit();

          /* Feed DOS conio queue for LookKeys kbhit/getch path */
          {
            int ext = dos_ext_getch(sc);
            if (ext >= 0) {
              key_enqueue(0);
              key_enqueue((unsigned char)ext);
            } else if (k >= 32 && k < 127) {
              key_enqueue((unsigned char)k);
            } else if (k == SDLK_RETURN) {
              key_enqueue(13);
            } else if (k == SDLK_BACKSPACE) {
              key_enqueue(8);
            } else if (k == SDLK_TAB) {
              key_enqueue(9);
            } else if (k == SDLK_ESCAPE) {
              key_enqueue(27);
            }
          }
        } else {
          /* On release, clear KeyScanCode if it still shows this make code */
          if (dos_sc >= 0 && KeyScanCode == (unsigned char)dos_sc)
            KeyScanCode = (unsigned char)(dos_sc + 128);
        }
        break;
      }
      default:
        break;
    }
  }
  /* Always re-sample mouse — event coords are unreliable with dpi/logical size. */
  sync_mouse_position();
}

short PlatformGetTicker(void)
{
  /*
   * IRQ0 / TimeCounter rate. InitBreakVector reprograms PIT channel 0 with
   * divisor 0x1FFF (the BIOS already left it in lo/hi access mode, so no
   * control word is needed): 1193182 / 8191 ≈ 145.67 Hz. The DOS IRQ handler
   * only lets the BIOS clock advance every 8th tick, and the game counts 146
   * ticks per second, confirming the 8× rate.
   */
  Uint64 ms = SDL_GetTicks() - start_ticks;
  return (short)((ms * 1193182u) / (8191u * 1000u));
}

unsigned int PlatformGetMillis(void)
{
  return SDL_GetTicks() - start_ticks;
}

void PlatformDelay(unsigned int ms)
{
  SDL_Delay(ms);
}
