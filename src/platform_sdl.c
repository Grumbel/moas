/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * SDL2 platform layer for MOAS.
 * Provides 320x200x256 software framebuffer presentation, mouse/keyboard
 * mapping into the original structures, and a millisecond ticker.
 */

#include "platform.h"
#include "lang.h"
#include "debug.h"

#include <SDL.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
#ifdef __ANDROID__
#include <SDL_system.h>
#include <jni.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#ifdef __ANDROID__
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <android/log.h>
#define MOAS_ALOG(...) __android_log_print(ANDROID_LOG_ERROR, "MOAS", __VA_ARGS__)
#else
#define MOAS_ALOG(...) ((void)0)
#endif

#ifndef DOS
/* io.c — write AUTOSAVE.SAV; safe once Screen is allocated. */
extern void MoasAutosaveOnExit(void);
#endif

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
static SDL_GameController *pad = NULL;
static int           pad_use = 0;   /* RT → left click */
static int           pad_look = 0;  /* LT → right click */
static int           pad_walk_cooldown = 0;
static int           pad_aiming = 0; /* 1: soft cursor owned by right stick */
static int           phys_left = 0, phys_right = 0, phys_middle = 0;
#ifndef DOS
extern void MoasPadWalk(int ax, int ay);
#endif
#ifdef __EMSCRIPTEN__
static volatile int  wasm_paused = 0;
static void wasm_pause_spin(void);
#endif
static int           moas_shift_down = 0;
static int           overlay_shift_hold = 0; /* synthetic Shift from overlay SKIP */
static int           overlay_pressed = 0;    /* OV_* while finger/mouse held on button */
static int           overlay_pending_cmd = 0; /* SAVE/LOAD after release → LookKeys */
static int           touch_mode = 0;          /* 0=USE 1=LOOK */
static int           shake_ox = 0, shake_oy = 0;
static int           screen_blank = 0;
static Uint32        sdl_palette[256];

int PlatformShiftDown(void)
{
  /* Overlay SKIP button and gamepad A synthesize Shift for speech skip. */
  if (overlay_shift_hold > 0)
    return 1;
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


static void platform_log(const char *fmt, ...)
{
  char buf[512];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
#ifdef __ANDROID__
  SDL_Log("%s", buf);
#else
  fprintf(stderr, "%s\n", buf);
#endif
}

#ifdef __ANDROID__
#ifndef MOAS_VERSION
#define MOAS_VERSION "unknown"
#endif

/* Game data files staged under APK assets/ by mk/android/scripts/build-apk.sh.
 * Optional licence name variants cover case differences on case-sensitive FS. */
static const char *const android_data_files[] = {
  "GAME.1", "GAME.2", "GAME.3", "GAME.4",
  "GAME.PCK", "GAME.XXX",
  "LICENCE.DAT", "Licence.Dat", "licence.dat",
  /* optional language packs (assets/lang/…) */
  "lang/en.ui", "lang/en.script",
  NULL
};

static int copy_asset_to(const char *asset_name, const char *dest_path)
{
  SDL_RWops *rw;
  FILE *out;
  char buf[8192];
  size_t n;
  long size;

  rw = SDL_RWFromFile(asset_name, "rb");
  if (!rw) {
    SDL_Log("asset missing: %s (%s)", asset_name, SDL_GetError());
    return -1;
  }
  size = (long)SDL_RWsize(rw);
  out = fopen(dest_path, "wb");
  if (!out) {
    SDL_Log("cannot write %s: %s", dest_path, strerror(errno));
    SDL_RWclose(rw);
    return -1;
  }
  while ((n = SDL_RWread(rw, buf, 1, sizeof(buf))) > 0) {
    if (fwrite(buf, 1, n, out) != n) {
      SDL_Log("short write to %s", dest_path);
      fclose(out);
      SDL_RWclose(rw);
      return -1;
    }
  }
  fclose(out);
  SDL_RWclose(rw);
  SDL_Log("extracted %s (%ld bytes) -> %s", asset_name, size, dest_path);
  return 0;
}

/* True if stamp matches this APK's MOAS_VERSION (re-extract after upgrade). */
static int assets_version_current(const char *dest_dir)
{
  char path[640];
  char line[128];
  FILE *f;
  size_t n;

  snprintf(path, sizeof(path), "%s/.moas_assets_version", dest_dir);
  f = fopen(path, "rb");
  if (!f)
    return 0;
  if (!fgets(line, sizeof(line), f)) {
    fclose(f);
    return 0;
  }
  fclose(f);
  n = strlen(line);
  while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
    line[--n] = 0;
  return strcmp(line, MOAS_VERSION) == 0;
}

static void write_assets_version(const char *dest_dir)
{
  char path[640];
  FILE *f;

  snprintf(path, sizeof(path), "%s/.moas_assets_version", dest_dir);
  f = fopen(path, "wb");
  if (!f) {
    SDL_Log("cannot write version stamp %s: %s", path, strerror(errno));
    return;
  }
  fprintf(f, "%s\n", MOAS_VERSION);
  fclose(f);
  SDL_Log("wrote asset version stamp %s (%s)", path, MOAS_VERSION);
}

void PlatformAndroidPrepareData(void)
{
  const char *base;
  char dest_dir[512];
  char dest_file[640];
  int i, ok = 0, fail = 0, force = 0;
  struct stat st;

  /* Verbose so logcat shows path resolution and extract progress. */
  SDL_LogSetAllPriority(SDL_LOG_PRIORITY_VERBOSE);
  MOAS_ALOG("MOAS Android starting (version %s)", MOAS_VERSION);
  SDL_Log("MOAS Android starting (version %s)", MOAS_VERSION);

  base = SDL_AndroidGetInternalStoragePath();
  if (!base || !base[0]) {
    SDL_Log("SDL_AndroidGetInternalStoragePath failed: %s", SDL_GetError());
    return;
  }
  SDL_Log("internal storage: %s", base);

  snprintf(dest_dir, sizeof(dest_dir), "%s/data", base);
  if (stat(dest_dir, &st) != 0) {
    if (mkdir(dest_dir, 0755) != 0 && errno != EEXIST) {
      SDL_Log("mkdir %s failed: %s", dest_dir, strerror(errno));
      return;
    }
  }
  SDL_Log("data directory: %s", dest_dir);

  if (!assets_version_current(dest_dir)) {
    force = 1;
    SDL_Log("asset version mismatch or missing — will re-extract");
  }

  for (i = 0; android_data_files[i]; i++) {
    snprintf(dest_file, sizeof(dest_file), "%s/%s", dest_dir, android_data_files[i]);
    {
      /* Ensure subdirs (e.g. lang/) exist. */
      char *slash = strrchr(dest_file, '/');
      if (slash) {
        *slash = 0;
        if (stat(dest_file, &st) != 0)
          mkdir(dest_file, 0755);
        *slash = '/';
      }
    }
    if (!force && stat(dest_file, &st) == 0 && st.st_size > 0) {
      SDL_Log("already present: %s (%ld bytes)", dest_file, (long)st.st_size);
      ok++;
      continue;
    }
    if (copy_asset_to(android_data_files[i], dest_file) == 0)
      ok++;
    else
      fail++;
  }
  SDL_Log("asset extract: %d ok, %d failed/missing (force=%d)", ok, fail, force);
  if (ok > 0)
    write_assets_version(dest_dir);

  if (setenv("MOAS_DATADIR", dest_dir, 1) != 0)
    SDL_Log("setenv MOAS_DATADIR failed");
  else {
    MOAS_ALOG("MOAS_DATADIR=%s", dest_dir);
    SDL_Log("MOAS_DATADIR=%s", dest_dir);
  }
  /* Savegames / AUTOSAVE under internal storage (writable). */
  {
    char state_dir[512];
    snprintf(state_dir, sizeof(state_dir), "%s/moas-state", dest_dir);
    if (mkdir(state_dir, 0755) != 0 && errno != EEXIST)
      SDL_Log("mkdir state %s failed: %s", state_dir, strerror(errno));
    if (setenv("MOAS_STATEDIR", state_dir, 1) != 0)
      SDL_Log("setenv MOAS_STATEDIR failed");
    else {
      MOAS_ALOG("MOAS_STATEDIR=%s", state_dir);
      SDL_Log("MOAS_STATEDIR=%s", state_dir);
    }
  }
}
#else
void PlatformAndroidPrepareData(void) { /* no-op on non-Android */ }
#endif

int PlatformInit(void)
{
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER) < 0) {
    platform_log("SDL_Init failed: %s", SDL_GetError());
    return -1;
  }

  {
    Uint32 flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
    if (moas_fullscreen)
      flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    {
      int win_w = LOGICAL_W * SCALE;
      int win_h = LOGICAL_H * SCALE;
#if defined(MOAS_OVERLAY_BUTTONS) && !defined(__ANDROID__)
      /*
       * Side letterbox for Save/Load/USE/LOOK columns (web / desktop overlay).
       * overlay_rects needs dx >= ~136 (btn up to 120 + margins); +280 → 140/side
       * when the framebuffer is height-limited (SCALE 3 → 960×600 game area).
       */
      win_w += 280;
#endif
      window = SDL_CreateWindow("Mänick Mänschion",
                                SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                win_w, win_h,
                                flags);
    }
  }
  if (!window) {
    platform_log("SDL_CreateWindow failed: %s", SDL_GetError());
    return -1;
  }

  renderer = SDL_CreateRenderer(window, -1,
                                SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!renderer)
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
  if (!renderer) {
    platform_log("SDL_CreateRenderer failed: %s", SDL_GetError());
    return -1;
  }

  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
  /* No SDL_RenderSetLogicalSize — we letterbox manually so mouse and video share math. */

  texture = SDL_CreateTexture(renderer,
                              SDL_PIXELFORMAT_ARGB8888,
                              SDL_TEXTUREACCESS_STREAMING,
                              LOGICAL_W, LOGICAL_H);
  if (!texture) {
    platform_log("SDL_CreateTexture failed: %s", SDL_GetError());
    return -1;
  }

  pixels = (Uint32 *)malloc(LOGICAL_W * LOGICAL_H * sizeof(Uint32));
  if (!pixels) {
    platform_log("Out of memory for pixel buffer");
    return -1;
  }

  for (int i = 0; i < 256; i++) {
    Uint8 v = (Uint8)i;
    sdl_palette[i] = 0xFF000000u | (v << 16) | (v << 8) | v;
  }

  start_ticks = SDL_GetTicks();
  running = 1;
  {
    int w = 0, h = 0;
    SDL_GetWindowSize(window, &w, &h);
    platform_log("PlatformInit ok: window %dx%d scale=%d fullscreen=%d",
                 w, h, SCALE, moas_fullscreen);
  }
  /* Prefer XInput/standard mappings when present. */
  SDL_GameControllerEventState(SDL_ENABLE);
  {
    int i, n = SDL_NumJoysticks();
    for (i = 0; i < n; i++) {
      if (SDL_IsGameController(i)) {
        pad = SDL_GameControllerOpen(i);
        if (pad) {
          platform_log("gamepad: %s", SDL_GameControllerName(pad));
          break;
        }
      }
    }
    if (!pad)
      platform_log("gamepad: none at startup (hotplug ok)");
  }
  return 0;
}

int PlatformShouldQuit(void)
{
  return running ? 0 : 1;
}

/*
 * Persist AUTOSAVE.SAV before the process may be killed.
 * Swiping the task away often never returns to main()'s post-loop
 * MoasAutosaveOnExit — background / terminating / RequestQuit must save here.
 */
static void platform_try_autosave(const char *reason)
{
#ifndef DOS
  static int in_save;

  if (in_save)
    return;
  if (!Screen) {
    MOAS_ALOG("autosave skipped (%s): Screen not ready", reason ? reason : "?");
    return;
  }
  in_save = 1;
  MOAS_ALOG("autosave (%s)", reason ? reason : "exit");
#ifdef __ANDROID__
  SDL_Log("moas: autosave trigger: %s", reason ? reason : "exit");
#endif
  MoasAutosaveOnExit();
  in_save = 0;
#else
  (void)reason;
#endif
}

void PlatformRequestQuit(void)
{
  /* Save before finish() — Android may kill us before main() exits the loop. */
  platform_try_autosave("request-quit");
  running = 0;
  KeyAbortState = 2;
#ifdef __ANDROID__
  /*
   * finish() alone can leave the task and relaunch SDLActivity. Prefer
   * finishAndRemoveTask (API 21+) then finish(); native main still returns.
   */
  {
    JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
    jobject activity = (jobject)SDL_AndroidGetActivity();
    if (env && activity) {
      jclass cls = (*env)->GetObjectClass(env, activity);
      if (cls) {
        jmethodID mid = (*env)->GetMethodID(env, cls, "finishAndRemoveTask", "()V");
        if (!mid)
          mid = (*env)->GetMethodID(env, cls, "finish", "()V");
        if (mid)
          (*env)->CallVoidMethod(env, activity, mid);
        (*env)->DeleteLocalRef(env, cls);
      }
      (*env)->DeleteLocalRef(env, activity);
    }
  }
#endif
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE void PlatformSetPaused(int paused)
{
  wasm_paused = paused ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE int PlatformIsPaused(void)
{
  return wasm_paused ? 1 : 0;
}

static void wasm_pause_spin(void)
{
  while (wasm_paused && running) {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
      if (ev.type == SDL_QUIT) {
        PlatformRequestQuit();
        return;
      }
      if (ev.type == SDL_KEYDOWN) {
        SDL_Keycode k = ev.key.keysym.sym;
        if (k == SDLK_p || k == SDLK_ESCAPE || k == SDLK_SPACE)
          wasm_paused = 0;
      }
    }
    SDL_Delay(40);
  }
}
#endif

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
  if (pad) { SDL_GameControllerClose(pad); pad = NULL; }
#ifdef __ANDROID__
  /* Full quit so SDLActivity tears down cleanly after main returns. */
  SDL_Quit();
#else
  /*
   * Full SDL_Quit() walks SDL's allocator and aborts if the game already
   * corrupted the heap. Tear down video only; process exit reclaims the rest.
   */
  SDL_QuitSubSystem(SDL_INIT_VIDEO);
#endif
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
  int dw, dh, dx, dy;
  float sx, sy, scale;

  if (renderer)
    SDL_GetRendererOutputSize(renderer, &ww, &wh);
  else if (window)
    SDL_GetWindowSize(window, &ww, &wh);
  if (ww < 1) ww = 1;
  if (wh < 1) wh = 1;

  sx = (float)ww / (float)LOGICAL_W;
  sy = (float)wh / (float)LOGICAL_H;
  scale = (sx < sy) ? sx : sy;
  dw = (int)(LOGICAL_W * scale + 0.5f);
  dh = (int)(LOGICAL_H * scale + 0.5f);
  dx = (ww - dw) / 2;
  dy = (wh - dh) / 2;

#if defined(__ANDROID__) || defined(MOAS_OVERLAY_BUTTONS)
  /*
   * Keep side columns free for the touch overlay (Save/Load/USE/LOOK, Esc/Skip).
   * overlay_rects needs dx >= btn_w + margins (~80–136). If the natural
   * letterbox is narrower (near-16:10 panels), shrink the game viewport so
   * buttons sit beside the picture instead of on top of it.
   */
  {
    int min_side = ww / 9;
    if (min_side < 88)
      min_side = 88;
    if (min_side > 180)
      min_side = 180;
    if (dx < min_side && ww > 2 * min_side + 64) {
      int avail_w = ww - 2 * min_side;
      float sx2 = (float)avail_w / (float)LOGICAL_W;
      float sy2 = (float)wh / (float)LOGICAL_H;
      float sc2 = (sx2 < sy2) ? sx2 : sy2;
      dw = (int)(LOGICAL_W * sc2 + 0.5f);
      dh = (int)(LOGICAL_H * sc2 + 0.5f);
      dx = min_side + (avail_w - dw) / 2;
      dy = (wh - dh) / 2;
    }
  }
#endif

  if (out_x) *out_x = dx;
  if (out_y) *out_y = dy;
  if (out_w) *out_w = dw;
  if (out_h) *out_h = dh;
  if (out_ww) *out_ww = ww;
  if (out_wh) *out_wh = wh;
}

/* SDL mouse/touch events use window coordinates; present/hit-test use
 * renderer output pixels. On Android high-DPI these often differ (2x). */
static void window_to_output(int wx, int wy, int *ox, int *oy)
{
  int ww = 1, wh = 1, ow = 1, oh = 1;
  if (window)
    SDL_GetWindowSize(window, &ww, &wh);
  if (renderer)
    SDL_GetRendererOutputSize(renderer, &ow, &oh);
  else {
    ow = ww;
    oh = wh;
  }
  if (ww < 1) ww = 1;
  if (wh < 1) wh = 1;
  if (ox) *ox = (int)((long)wx * (long)ow / (long)ww);
  if (oy) *oy = (int)((long)wy * (long)oh / (long)wh);
}


/* Overlay button ids — always defined so PlatformPumpEvents can reference them;
 * on non-Android builds overlay_hit is a stub that always returns OV_NONE. */
enum {
  OV_NONE = 0,
  OV_SAVE = 1,
  OV_LOAD = 2,
  OV_ESC = 3,
  OV_SKIP = 4,
  OV_MODE_USE = 5,
  OV_MODE_LOOK = 6
};

#if defined(__ANDROID__) || defined(MOAS_OVERLAY_BUTTONS)
/* Touch controls in letterbox margins (window pixels), not the 320x200 buffer.
 * Layout: left column Save / Load; right column Esc / Skip. */

/* 5x7 glyphs, bit0 = leftmost pixel of each row. A–Z + space + digits unused. */
static const unsigned char ov_font_5x7[][7] = {
  /* A */ {0x0e,0x11,0x11,0x1f,0x11,0x11,0x11},
  /* B */ {0x1e,0x11,0x11,0x1e,0x11,0x11,0x1e},
  /* C */ {0x0e,0x11,0x10,0x10,0x10,0x11,0x0e},
  /* D */ {0x1e,0x11,0x11,0x11,0x11,0x11,0x1e},
  /* E */ {0x1f,0x10,0x10,0x1e,0x10,0x10,0x1f},
  /* F */ {0x1f,0x10,0x10,0x1e,0x10,0x10,0x10},
  /* G */ {0x0e,0x11,0x10,0x17,0x11,0x11,0x0e},
  /* H */ {0x11,0x11,0x11,0x1f,0x11,0x11,0x11},
  /* I */ {0x0e,0x04,0x04,0x04,0x04,0x04,0x0e},
  /* J */ {0x01,0x01,0x01,0x01,0x11,0x11,0x0e},
  /* K */ {0x11,0x12,0x14,0x18,0x14,0x12,0x11},
  /* L */ {0x10,0x10,0x10,0x10,0x10,0x10,0x1f},
  /* M */ {0x11,0x1b,0x15,0x15,0x11,0x11,0x11},
  /* N */ {0x11,0x19,0x15,0x13,0x11,0x11,0x11},
  /* O */ {0x0e,0x11,0x11,0x11,0x11,0x11,0x0e},
  /* P */ {0x1e,0x11,0x11,0x1e,0x10,0x10,0x10},
  /* Q */ {0x0e,0x11,0x11,0x11,0x15,0x12,0x0d},
  /* R */ {0x1e,0x11,0x11,0x1e,0x14,0x12,0x11},
  /* S */ {0x0e,0x11,0x10,0x0e,0x01,0x11,0x0e},
  /* T */ {0x1f,0x04,0x04,0x04,0x04,0x04,0x04},
  /* U */ {0x11,0x11,0x11,0x11,0x11,0x11,0x0e},
  /* V */ {0x11,0x11,0x11,0x11,0x11,0x0a,0x04},
  /* W */ {0x11,0x11,0x11,0x15,0x15,0x1b,0x11},
  /* X */ {0x11,0x11,0x0a,0x04,0x0a,0x11,0x11},
  /* Y */ {0x11,0x11,0x0a,0x04,0x04,0x04,0x04},
  /* Z */ {0x1f,0x01,0x02,0x04,0x08,0x10,0x1f},
};

static void ov_draw_char(int x, int y, char ch, int scale, Uint8 r, Uint8 g, Uint8 b)
{
  int row, col, gi;
  if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');
  if (ch < 'A' || ch > 'Z') return;
  gi = ch - 'A';
  SDL_SetRenderDrawColor(renderer, r, g, b, 255);
  for (row = 0; row < 7; row++) {
    unsigned char bits = ov_font_5x7[gi][row];
    for (col = 0; col < 5; col++) {
      if (bits & (1 << (4 - col))) {
        SDL_Rect p;
        p.x = x + col * scale;
        p.y = y + row * scale;
        p.w = scale;
        p.h = scale;
        SDL_RenderFillRect(renderer, &p);
      }
    }
  }
}

static void ov_draw_text(int x, int y, const char *s, int scale, Uint8 r, Uint8 g, Uint8 b)
{
  int cx = x;
  for (; *s; s++) {
    if (*s == ' ') {
      cx += 4 * scale;
      continue;
    }
    ov_draw_char(cx, y, *s, scale, r, g, b);
    cx += 6 * scale;
  }
}

/* Button rects [0]=Save [1]=Load [2]=Esc [3]=Skip [4]=USE/PULL [5]=LOOK
 * Save/Load + Esc/Skip stick to the top; USE/LOOK sit low for thumbs. */
static void overlay_rects(SDL_Rect out[6])
{
  int dx, dy, dw, dh, ww, wh;
  int btn_w, btn_h, gap, margin;
  int left_x, right_x;
  int top_y, mode_y0;
  int side;
  int top_stack_h, mode_stack_h;

  letterbox_geom(&dx, &dy, &dw, &dh, &ww, &wh);
  gap = 10;
  margin = 8;
  side = dx;
  if (side < ww - (dx + dw))
    side = ww - (dx + dw);
  btn_w = side - margin * 2;
  if (btn_w > 120)
    btn_w = 120;
  if (btn_w < 64)
    btn_w = 64;
  btn_h = (btn_w * 5) / 7;
  if (btn_h < 52)
    btn_h = 52;
  if (btn_h > 88)
    btn_h = 88;

  top_stack_h = btn_h * 2 + gap;
  mode_stack_h = btn_h * 2 + gap;

  if (dx >= btn_w + margin * 2) {
    left_x = (dx - btn_w) / 2;
    if (left_x < margin)
      left_x = margin;
    right_x = dx + dw + (ww - (dx + dw) - btn_w) / 2;
    if (right_x + btn_w > ww - margin)
      right_x = ww - margin - btn_w;
    /* Menu/abort at top; Pull/Use + LOOK near bottom for thumbs. */
    top_y = margin;
    mode_y0 = wh - margin - mode_stack_h;
    /* Keep modes from climbing into the top cluster. */
    if (mode_y0 < top_y + top_stack_h + gap * 2)
      mode_y0 = top_y + top_stack_h + gap * 2;
  } else {
    btn_w = 72;
    btn_h = 56;
    top_stack_h = btn_h * 2 + gap;
    mode_stack_h = btn_h * 2 + gap;
    left_x = margin;
    right_x = ww - margin - btn_w;
    top_y = margin;
    mode_y0 = wh - margin - mode_stack_h;
    if (mode_y0 < top_y + top_stack_h + gap * 2)
      mode_y0 = top_y + top_stack_h + gap * 2;
  }

  out[0].x = left_x;  out[0].y = top_y;                    out[0].w = btn_w; out[0].h = btn_h; /* Save */
  out[1].x = left_x;  out[1].y = top_y + btn_h + gap;       out[1].w = btn_w; out[1].h = btn_h; /* Load */
  out[2].x = right_x; out[2].y = top_y;                    out[2].w = btn_w; out[2].h = btn_h; /* Esc  */
  out[3].x = right_x; out[3].y = top_y + btn_h + gap;       out[3].w = btn_w; out[3].h = btn_h; /* Skip */
  out[4].x = left_x;  out[4].y = mode_y0;                  out[4].w = btn_w; out[4].h = btn_h; /* USE/PULL */
  out[5].x = left_x;  out[5].y = mode_y0 + btn_h + gap;     out[5].w = btn_w; out[5].h = btn_h; /* LOOK */
}

/* ox/oy are renderer-output pixels (after window_to_output). */
static int overlay_hit(int ox, int oy)
{
  SDL_Rect r[6];
  int i;
  int pad = 14;

  overlay_rects(r);
  for (i = 0; i < 6; i++) {
    if (ox >= r[i].x - pad && ox < r[i].x + r[i].w + pad &&
        oy >= r[i].y - pad && oy < r[i].y + r[i].h + pad)
      return i + 1; /* OV_SAVE..OV_MODE_LOOK */
  }
  return OV_NONE;
}

static void overlay_fire(int which)
{
  switch (which) {
    case OV_SAVE:
    case OV_LOAD:
      /*
       * Do not open the menu on press: the same touch would still be “down”
       * (or its synthetic mouse release would satisfy LoadSaveMenu’s wait) and
       * close the menu immediately / pick a bogus slot. Queue until release;
       * LookKeys runs SaveGame/LoadGame with the usual control checks.
       */
      overlay_pending_cmd = which;
      MOAS_ALOG("overlay: queued %s", which == OV_SAVE ? "SAVE" : "LOAD");
      break;
    case OV_ESC:
      if (KeyAbortState == 1)
        KeyAbortState = 2;
      KeyScanCode = 1;
      MOAS_ALOG("overlay: ESC abort=%d", (int)KeyAbortState);
      break;
    case OV_SKIP:
      /* Synthetic Shift: speed up / dismiss speech bubbles (not full sequence abort). */
      overlay_shift_hold = 20; /* ~frames while speech checks PlatformShiftDown */
      MOAS_ALOG("overlay: SKIP (speech)");
      break;
    case OV_MODE_USE:
      touch_mode = 0;
      MOAS_ALOG("overlay: mode USE");
      break;
    case OV_MODE_LOOK:
      touch_mode = 1;
      MOAS_ALOG("overlay: mode LOOK");
      break;
    default:
      break;
  }
}

int PlatformPollOverlayCmd(void)
{
  int c = overlay_pending_cmd;
  overlay_pending_cmd = 0;
  return c;
}

int PlatformGetTouchMode(void)
{
  return touch_mode;
}

static void ov_fill_roundish(SDL_Rect *r, Uint8 R, Uint8 G, Uint8 B)
{
  /* Fake rounded corners by stepping the fill edges in a few pixels. */
  SDL_Rect mid = *r;
  int c = r->w / 12;
  if (c < 3) c = 3;
  if (c > 10) c = 10;
  mid.x += c; mid.w -= 2 * c;
  SDL_SetRenderDrawColor(renderer, R, G, B, 255);
  SDL_RenderFillRect(renderer, &mid);
  mid = *r;
  mid.y += c; mid.h -= 2 * c;
  SDL_RenderFillRect(renderer, &mid);
  /* corner nubs */
  {
    SDL_Rect q;
    q.x = r->x + c / 2; q.y = r->y + c / 2; q.w = c; q.h = c;
    SDL_RenderFillRect(renderer, &q);
    q.x = r->x + r->w - c - c / 2;
    SDL_RenderFillRect(renderer, &q);
    q.y = r->y + r->h - c - c / 2;
    SDL_RenderFillRect(renderer, &q);
    q.x = r->x + c / 2;
    SDL_RenderFillRect(renderer, &q);
  }
}

static void ov_icon_save(SDL_Rect *r, int pressed)
{
  /* 3.5" floppy: body, metal shutter, label window, write-protect notch */
  SDL_Rect body, shutter, label, hub, notch;
  int m = r->w / 10;
  int oy = pressed ? 2 : 0;
  body.x = r->x + m; body.y = r->y + m + oy;
  body.w = r->w - 2 * m; body.h = r->h / 2 - m;
  SDL_SetRenderDrawColor(renderer, 245, 245, 245, 255);
  SDL_RenderFillRect(renderer, &body);
  SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
  SDL_RenderDrawRect(renderer, &body);
  /* metal shutter top */
  shutter.x = body.x + body.w / 5; shutter.y = body.y;
  shutter.w = body.w * 3 / 5; shutter.h = body.h / 4;
  SDL_SetRenderDrawColor(renderer, 90, 95, 100, 255);
  SDL_RenderFillRect(renderer, &shutter);
  /* label area */
  label.x = body.x + body.w / 6; label.y = body.y + body.h * 2 / 5;
  label.w = body.w * 2 / 3; label.h = body.h * 2 / 5;
  SDL_SetRenderDrawColor(renderer, 230, 235, 240, 255);
  SDL_RenderFillRect(renderer, &label);
  SDL_SetRenderDrawColor(renderer, 50, 100, 60, 255);
  SDL_RenderDrawRect(renderer, &label);
  /* hub circle (square approx) */
  hub.w = body.w / 5; hub.h = hub.w;
  hub.x = body.x + (body.w - hub.w) / 2;
  hub.y = body.y + body.h - hub.h - 2;
  SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
  SDL_RenderFillRect(renderer, &hub);
  /* write-protect notch */
  notch.x = body.x + body.w - m / 2; notch.y = body.y + body.h / 3;
  notch.w = m / 2 + 1; notch.h = body.h / 5;
  SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
  SDL_RenderFillRect(renderer, &notch);
}

static void ov_icon_load(SDL_Rect *r, int pressed)
{
  /* Folder with tab + downward arrow into it */
  SDL_Rect tab, body;
  int m = r->w / 10;
  int mid = r->x + r->w / 2;
  int oy = pressed ? 2 : 0;
  int y0 = r->y + m + oy;
  tab.x = r->x + m; tab.y = y0;
  tab.w = r->w / 3; tab.h = r->h / 12;
  body.x = r->x + m; body.y = tab.y + tab.h;
  body.w = r->w - 2 * m; body.h = r->h / 3;
  SDL_SetRenderDrawColor(renderer, 255, 220, 120, 255);
  SDL_RenderFillRect(renderer, &tab);
  SDL_SetRenderDrawColor(renderer, 255, 200, 70, 255);
  SDL_RenderFillRect(renderer, &body);
  SDL_SetRenderDrawColor(renderer, 120, 80, 20, 255);
  SDL_RenderDrawRect(renderer, &body);
  SDL_RenderDrawRect(renderer, &tab);
  /* thick arrow into folder */
  {
    int tip = body.y + body.h * 2 / 3;
    int i;
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    for (i = 0; i < 3; i++) {
      SDL_RenderDrawLine(renderer, mid + i - 1, y0 - 2, mid + i - 1, tip);
      SDL_RenderDrawLine(renderer, mid - m, tip - m + i, mid, tip + i);
      SDL_RenderDrawLine(renderer, mid + m, tip - m + i, mid, tip + i);
    }
  }
}

static void ov_icon_esc(SDL_Rect *r, int pressed)
{
  /* Red stop plate with bold X */
  SDL_Rect plate;
  int m = r->w / 8;
  int oy = pressed ? 2 : 0;
  int cx = r->x + r->w / 2, cy = r->y + r->h / 3 + oy;
  int rad = r->w / 5;
  int i;
  plate.x = cx - rad; plate.y = cy - rad; plate.w = rad * 2; plate.h = rad * 2;
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
  SDL_RenderFillRect(renderer, &plate);
  SDL_SetRenderDrawColor(renderer, 180, 30, 30, 255);
  {
    SDL_Rect in = plate;
    in.x += 3; in.y += 3; in.w -= 6; in.h -= 6;
    SDL_RenderFillRect(renderer, &in);
  }
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
  for (i = 0; i < 3; i++) {
    SDL_RenderDrawLine(renderer, plate.x + m + i, plate.y + m,
                       plate.x + plate.w - m + i, plate.y + plate.h - m);
    SDL_RenderDrawLine(renderer, plate.x + plate.w - m - i, plate.y + m,
                       plate.x + m - i, plate.y + plate.h - m);
  }
}

static void ov_icon_skip(SDL_Rect *r, int pressed)
{
  /* Fast-forward: two filled chevrons */
  int m = r->w / 8;
  int oy = pressed ? 2 : 0;
  int x0 = r->x + r->w / 5;
  int y0 = r->y + m + oy;
  int y1 = r->y + r->h / 3 + oy;
  int y2 = r->y + r->h / 2 - m + oy;
  int x1 = r->x + r->w * 2 / 5;
  int x2 = r->x + r->w / 2;
  int i;
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
  for (i = 0; i < 4; i++) {
    SDL_RenderDrawLine(renderer, x0 + i, y0, x1 + i, y1);
    SDL_RenderDrawLine(renderer, x1 + i, y1, x0 + i, y2);
    SDL_RenderDrawLine(renderer, x2 + i, y0, x2 + (x1 - x0) + i, y1);
    SDL_RenderDrawLine(renderer, x2 + (x1 - x0) + i, y1, x2 + i, y2);
  }
}

static void overlay_draw(void)
{
  SDL_Rect r[6];
  static const SDL_Color cols[6] = {
    { 32, 140, 58, 255 },   /* Save */
    { 36, 100, 190, 255 },  /* Load */
    { 170, 42, 42, 255 },   /* Esc  */
    { 200, 120, 28, 255 },  /* Skip */
    { 70, 70, 90, 255 },    /* USE / Pull */
    { 70, 70, 90, 255 },    /* LOOK */
  };
  const char *labels[6] = {
    Ui(UI_OV_SAVE), Ui(UI_OV_LOAD), Ui(UI_OV_ESC), Ui(UI_OV_SKIP),
    Ui(UI_OV_USE), Ui(UI_OV_LOOK)
  };
  int i, ts;

  if (!renderer)
    return;
  /* overlay_shift_hold decays in pad_apply / end of pump */

  overlay_rects(r);
  for (i = 0; i < 6; i++) {
    int pressed = (overlay_pressed == i + 1);
    int selected = ((i == 4 && touch_mode == 0) || (i == 5 && touch_mode == 1));
    SDL_Color c = cols[i];
    SDL_Rect face = r[i];

    if (selected) {
      c.r = (Uint8)(c.r + 40 > 255 ? 255 : c.r + 40);
      c.g = (Uint8)(c.g + 50 > 255 ? 255 : c.g + 50);
      c.b = (Uint8)(c.b + 80 > 255 ? 255 : c.b + 80);
    }
    if (pressed) {
      c.r = (Uint8)(c.r * 2 / 3);
      c.g = (Uint8)(c.g * 2 / 3);
      c.b = (Uint8)(c.b * 2 / 3);
    }

    if (!pressed) {
      SDL_Rect sh = r[i];
      sh.x += 3; sh.y += 3;
      SDL_SetRenderDrawColor(renderer, 0, 0, 0, 140);
      SDL_RenderFillRect(renderer, &sh);
    } else {
      face.x += 2; face.y += 2;
    }

    ov_fill_roundish(&face, c.r, c.g, c.b);

    if (selected) {
      SDL_SetRenderDrawColor(renderer, 255, 220, 80, 255);
      SDL_RenderDrawRect(renderer, &face);
      {
        SDL_Rect in = face;
        in.x += 2; in.y += 2; in.w -= 4; in.h -= 4;
        SDL_RenderDrawRect(renderer, &in);
      }
    } else {
      SDL_SetRenderDrawColor(renderer, 255, 255, 255, pressed ? 255 : 180);
      SDL_RenderDrawRect(renderer, &face);
    }

    if (i == 0) ov_icon_save(&face, pressed);
    else if (i == 1) ov_icon_load(&face, pressed);
    else if (i == 2) ov_icon_esc(&face, pressed);
    else if (i == 3) ov_icon_skip(&face, pressed);
    /* USE (Pull/Use) / LOOK: text only */

    ts = (face.w >= 80) ? 2 : 1;
    {
      int text_w = (int)strlen(labels[i]) * 6 * ts;
      int tx = face.x + (face.w - text_w) / 2;
      int ty = face.y + face.h - 7 * ts - 6;
      if (ty < face.y + face.h / 2)
        ty = face.y + (face.h - 7 * ts) / 2;
      ov_draw_text(tx + 1, ty + 1, labels[i], ts, 0, 0, 0);
      ov_draw_text(tx, ty, labels[i], ts, 255, 255, 255);
    }
  }
}
#else
static int overlay_hit(int wx, int wy) { (void)wx; (void)wy; return 0; }
static void overlay_fire(int which) { (void)which; }
static void overlay_draw(void) { }
int PlatformPollOverlayCmd(void) { return 0; }
int PlatformGetTouchMode(void) { return 0; }
#endif

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
  /* Android (and optional desktop) touch buttons sit in letterbox, above the game. */
  overlay_draw();
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
    int rx = dx + (int)((float)x * (float)dw / (float)LOGICAL_W);
    int ry = dy + (int)((float)y * (float)dh / (float)LOGICAL_H);
    int wwin = 1, whin = 1;
    SDL_GetWindowSize(window, &wwin, &whin);
    if (ww < 1) ww = 1;
    if (wh < 1) wh = 1;
    /* Warp expects window coords; letterbox uses output pixels. */
    SDL_WarpMouseInWindow(window,
                          (int)((long)rx * (long)wwin / (long)ww),
                          (int)((long)ry * (long)whin / (long)wh));
  }
}

static void mouse_to_logical(int wx, int wy, short *ox, short *oy)
{
  int dx, dy, dw, dh;
  int rx, ry;
  float lx, ly;

  /* Events are window-space; letterbox geometry is renderer-output space. */
  window_to_output(wx, wy, &rx, &ry);
  letterbox_geom(&dx, &dy, &dw, &dh, NULL, NULL);
  if (dw < 1) dw = 1;
  if (dh < 1) dh = 1;

  lx = ((float)rx - (float)dx) * (float)LOGICAL_W / (float)dw;
  ly = ((float)ry - (float)dy) * (float)LOGICAL_H / (float)dh;

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


/* ---- Gamepad (SDL GameController) ------------------------------------- */
/*
 * Left stick / D-pad  → walk (MoasPadWalk)
 * Right stick         → cursor
 * RT                  → use (left click)
 * LT                  → look (right click / SIEHE)
 * A                   → skip speech (Shift)
 * B                   → Esc / abort sequence
 * Start               → Esc / abort sequence
 * Back                → quit request
 * X / Y               → USE / LOOK touch mode (overlay parity)
 */
static int axis_dead(int v, int dead)
{
  if (v > -dead && v < dead) return 0;
  return v;
}

static void pad_apply(void)
{
  Sint16 lx, ly, rx, ry, lt, rt;
  int walk_x, walk_y, dx, dy;
  const int stick_dead = 8000;
  const int trig_dead = 12000;
  int use, look;

  if (overlay_shift_hold > 0)
    overlay_shift_hold--;

  if (!pad)
    return;

  lx = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
  ly = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);
  rx = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTX);
  ry = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTY);
  lt = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT);
  rt = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);

  /* D-pad overrides left stick when pressed. */
  walk_x = axis_dead(lx, stick_dead);
  walk_y = axis_dead(ly, stick_dead);
  if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT))
    walk_x = -32767;
  if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT))
    walk_x = 32767;
  if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP))
    walk_y = -32767;
  if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN))
    walk_y = 32767;

  /*
   * Right stick → relative cursor. Warp the OS pointer so release does not
   * snap back via sync_mouse_position() to the old physical position.
   * Speed scales with deflection (gentle near centre, faster at the edge).
   */
  dx = axis_dead(rx, stick_dead);
  dy = axis_dead(ry, stick_dead);
  if (dx || dy) {
    int mx, my;
    /* |d|/32767 in 0..1; use squared response for finer aim near centre. */
    float fx = (float)dx / 32767.0f;
    float fy = (float)dy / 32767.0f;
    float sx = fx * (0.35f + 0.65f * (fx < 0 ? -fx : fx));
    float sy = fy * (0.35f + 0.65f * (fy < 0 ? -fy : fy));
    mx = (int)Mouse.x + (int)(sx * 14.0f);
    my = (int)Mouse.y + (int)(sy * 14.0f);
    if (mx < 0) mx = 0;
    if (my < 0) my = 0;
    if (mx > LOGICAL_W - 1) mx = LOGICAL_W - 1;
    if (my > LOGICAL_H - 1) my = LOGICAL_H - 1;
    Mouse.x = (short)mx;
    Mouse.y = (short)my;
    pad_aiming = 1;
    PlatformSetMousePos(Mouse.x, Mouse.y);
  }

  use = (rt > trig_dead) || SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
  look = (lt > trig_dead) || SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
  pad_use = use ? 1 : 0;
  pad_look = look ? 1 : 0;
  Mouse.Left = phys_left || pad_use;
  Mouse.Right = phys_right || pad_look;
  Mouse.Middle = phys_middle;

  if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A))
    overlay_shift_hold = 20; /* skip speech */
  if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_B) ||
      SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_START)) {
    if (KeyAbortState == 1)
      KeyAbortState = 2;
    KeyScanCode = 1; /* Esc make */
  }
  if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_BACK))
    PlatformRequestQuit();
#if defined(__ANDROID__) || defined(MOAS_OVERLAY_BUTTONS)
  if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_X))
    touch_mode = 0;
  if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_Y))
    touch_mode = 1;
#endif

#ifndef DOS
  if (pad_walk_cooldown > 0)
    pad_walk_cooldown--;
  else if (walk_x || walk_y) {
    MoasPadWalk((walk_x * 1000) / 32767, (walk_y * 1000) / 32767);
    pad_walk_cooldown = 4; /* ~throttle WalkClick spam */
  }
#endif
}

void PlatformPumpEvents(void)
{
  SDL_Event ev;
  while (SDL_PollEvent(&ev)) {
    switch (ev.type) {
      case SDL_QUIT:
        PlatformRequestQuit();
        break;
      case SDL_CONTROLLERDEVICEADDED:
        if (!pad) {
          pad = SDL_GameControllerOpen(ev.cdevice.which);
          if (pad)
            platform_log("gamepad connected: %s", SDL_GameControllerName(pad));
        }
        break;
      case SDL_CONTROLLERDEVICEREMOVED:
        if (pad && ev.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad))) {
          SDL_GameControllerClose(pad);
          pad = NULL;
          pad_use = pad_look = 0;
          platform_log("gamepad disconnected");
        }
        break;
#ifdef __ANDROID__
      /*
       * Swipe-away / Home: process is often killed after onPause without
       * returning to main()'s exit path. Save immediately on background
       * and on terminating so AUTOSAVE.SAV exists for the next cold start.
       */
      case SDL_APP_WILLENTERBACKGROUND:
        platform_try_autosave("will-enter-background");
        break;
      case SDL_APP_DIDENTERBACKGROUND:
        platform_try_autosave("did-enter-background");
        break;
      case SDL_APP_TERMINATING:
        platform_try_autosave("terminating");
        PlatformRequestQuit();
        break;
#endif
      case SDL_MOUSEMOTION: {
        short mx, my;
        /* Physical mouse motion takes ownership back from the stick. */
        if (ev.motion.xrel != 0 || ev.motion.yrel != 0)
          pad_aiming = 0;
        mouse_to_logical(ev.motion.x, ev.motion.y, &mx, &my);
        Mouse.x = mx;
        Mouse.y = my;
        break;
      }
      case SDL_MOUSEBUTTONDOWN:
      case SDL_MOUSEBUTTONUP: {
        /* Only update current button state. MouseCheck latches Last* each frame. */
        int down = (ev.type == SDL_MOUSEBUTTONDOWN);
        int rx, ry, ov;
        window_to_output(ev.button.x, ev.button.y, &rx, &ry);
        ov = overlay_hit(rx, ry);
        if (ov && ev.button.button == SDL_BUTTON_LEFT) {
          if (down) {
            overlay_pressed = ov;
            /* ESC/SKIP act immediately; SAVE/LOAD wait for release. */
            if (ov == OV_ESC || ov == OV_SKIP || ov == OV_MODE_USE || ov == OV_MODE_LOOK)
              overlay_fire(ov);
          } else {
            if (overlay_pressed == ov && (ov == OV_SAVE || ov == OV_LOAD))
              overlay_fire(ov);
            else if (overlay_pressed == OV_SAVE || overlay_pressed == OV_LOAD)
              overlay_fire(overlay_pressed); /* release slightly off button still counts */
            overlay_pressed = 0;
          }
          break; /* do not inject a game click */
        }
        if (ov) {
          if (!down)
            overlay_pressed = 0;
          break; /* ignore release over overlay */
        }
        if (!down) {
          if (overlay_pressed == OV_SAVE || overlay_pressed == OV_LOAD)
            overlay_fire(overlay_pressed);
          overlay_pressed = 0;
        }
        {
          short mx, my;
          mouse_to_logical(ev.button.x, ev.button.y, &mx, &my);
          Mouse.x = mx;
          Mouse.y = my;
        }
        if (ev.button.button == SDL_BUTTON_LEFT) {
          phys_left = down ? 1 : 0;
          Mouse.Left = phys_left || pad_use;
          if (down) {
            Mouse.ClickX = Mouse.x;
            Mouse.ClickY = Mouse.y;
          } else {
            Mouse.ReleaseX = Mouse.x;
            Mouse.ReleaseY = Mouse.y;
          }
        } else if (ev.button.button == SDL_BUTTON_RIGHT) {
          phys_right = down ? 1 : 0;
          Mouse.Right = phys_right || pad_look;
        } else if (ev.button.button == SDL_BUTTON_MIDDLE) {
          phys_middle = down ? 1 : 0;
          Mouse.Middle = phys_middle;
        }
        break;
      }
      case SDL_FINGERDOWN: {
        int ww, wh, wx, wy, rx, ry, ov;
        SDL_GetWindowSize(window, &ww, &wh);
        wx = (int)(ev.tfinger.x * (float)ww + 0.5f);
        wy = (int)(ev.tfinger.y * (float)wh + 0.5f);
        window_to_output(wx, wy, &rx, &ry);
        ov = overlay_hit(rx, ry);
        if (ov) {
          overlay_pressed = ov;
          if (ov == OV_ESC || ov == OV_SKIP || ov == OV_MODE_USE || ov == OV_MODE_LOOK)
            overlay_fire(ov);
          break;
        }
        {
          short mx, my;
          mouse_to_logical(wx, wy, &mx, &my);
          Mouse.x = mx;
          Mouse.y = my;
          phys_left = 1;
          Mouse.Left = 1;
          Mouse.ClickX = Mouse.x;
          Mouse.ClickY = Mouse.y;
        }
        break;
      }
      case SDL_FINGERUP: {
        int ww, wh, wx, wy, rx, ry;
        SDL_GetWindowSize(window, &ww, &wh);
        wx = (int)(ev.tfinger.x * (float)ww + 0.5f);
        wy = (int)(ev.tfinger.y * (float)wh + 0.5f);
        window_to_output(wx, wy, &rx, &ry);
        if (overlay_hit(rx, ry) || overlay_pressed) {
          if (overlay_pressed == OV_SAVE || overlay_pressed == OV_LOAD)
            overlay_fire(overlay_pressed);
          overlay_pressed = 0;
          break;
        }
        {
          short mx, my;
          mouse_to_logical(wx, wy, &mx, &my);
          Mouse.x = mx;
          Mouse.y = my;
          phys_left = 0;
          Mouse.Left = pad_use ? 1 : 0;
          Mouse.ReleaseX = Mouse.x;
          Mouse.ReleaseY = Mouse.y;
        }
        break;
      }
      case SDL_FINGERMOTION: {
        /* Keep pressed highlight only while finger stays on a button. */
        int ww, wh, wx, wy, rx, ry, ov;
        if (!overlay_pressed)
          break;
        SDL_GetWindowSize(window, &ww, &wh);
        wx = (int)(ev.tfinger.x * (float)ww + 0.5f);
        wy = (int)(ev.tfinger.y * (float)wh + 0.5f);
        window_to_output(wx, wy, &rx, &ry);
        ov = overlay_hit(rx, ry);
        if (ov != overlay_pressed)
          overlay_pressed = ov; /* may become 0 when leaving */
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
          /*
           * Match DOS KeyboardIrq: ESC only aborts when a script has armed
           * skippable mode (KeyAbortState==1). Setting state 2 unconditionally
           * left PowerRedrawArea / waits dead after Esc during free walk.
           */
          if (k == SDLK_ESCAPE && KeyAbortState == 1)
            KeyAbortState = 2;
          if (k == SDLK_q ||
              (k == SDLK_c && (mod & KMOD_CTRL)) ||
              (k == SDLK_F4 && (mod & KMOD_ALT)) ||
              k == SDLK_AC_BACK)
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
  /*
   * Do not resync from the OS pointer while the right stick owns the cursor
   * (including after release — otherwise the soft cursor snaps back). Physical
   * mouse motion clears pad_aiming in SDL_MOUSEMOTION.
   */
  if (!pad_aiming)
    sync_mouse_position();
  pad_apply();
#ifdef __EMSCRIPTEN__
  wasm_pause_spin();
#endif
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
#ifdef __EMSCRIPTEN__
  wasm_pause_spin();
#endif
  SDL_Delay(ms);
}
