/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * SDL2 platform layer for MOAS.
 * Provides 320x200x256 software framebuffer presentation, mouse/keyboard
 * mapping into the original structures, and a millisecond ticker.
 */

#include "platform.h"
#include "lang.h"
#include "debug.h"
#include "settings.h"
#include "version.h"

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
extern void MoasReloadLanguage(void);
extern int MoasCanOpenSaveMenu(void);
extern int MoasAnySaveExists(void);
extern int MoasGetHotspots(short *rects, int max);
#endif

/* Minimal declarations of the globals we touch (defined elsewhere). */
typedef unsigned char uChar;
extern uChar *Screen;
extern uChar *Display;
extern unsigned char *Palette;   /* 768 bytes, VGA 0-63 */
extern char KeyAbortState;
extern unsigned char KeyScanCode;
extern short CurrentProc, TotalSpeed, SpeechSpeed;
extern void PowerRedrawArea(short x1, short y1, short x2, short y2);


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
/* Extra window width (both side columns) for the touch overlay. */
#define OVERLAY_EXTRA_W 280

static SDL_Window   *window   = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture  *texture  = NULL;
static Uint32       *pixels   = NULL;
static Uint32        start_ticks = 0;
static int           running  = 1;
static int           restart_requested;
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
/*
 * Synthetic Shift (overlay SKIP, gamepad A) until this SDL tick. Time-based:
 * the event pump runs in busy loops (SayWait), so a frame counter expired
 * before UpDateSayWindows (every 4 game ticks, ~27 ms) could see it. Speech
 * is dismissed on the Shift release edge, so the hold must outlast one
 * UpDateSayWindows period.
 */
#define SHIFT_HOLD_MS 120
static Uint32        overlay_shift_until = 0;
static int           overlay_down = 0;       /* OV_* where the press started */
static int           overlay_pressed = 0;    /* OV_* drawn pressed (pointer still on it) */
static int           overlay_pending_cmd = 0; /* SAVE/LOAD after release → LookKeys */
static int           touch_mode = 0;          /* 0=USE 1=LOOK */
static int           overlay_on = 0;          /* on-screen buttons visible */
static int           ov_save_ok = 1;          /* SAVE/LOAD usable right now */
static int           ov_load_ok = 1;
static int           pointer_is_touch = 0;    /* last pointer input was a finger */
static int           touch_seen = 0;          /* any finger input this session */
static SDL_FingerID  game_finger = 0;         /* finger acting as left button */
static int           game_finger_down = 0;
static SDL_FingerID  overlay_finger = 0;
static int           esc_break_pending = 0;   /* synthetic Esc: send break next pump */
static int           shake_ox = 0, shake_oy = 0;
static int           screen_blank = 0;
static Uint32        sdl_palette[256];

int PlatformShiftDown(void)
{
  /* Overlay SKIP button and gamepad A synthesize Shift for speech skip. */
  if (overlay_shift_until && (Sint32)(overlay_shift_until - SDL_GetTicks()) > 0)
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


static void ov_free_icons(void);

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

/* Game data files staged under APK assets/ by mk/android/scripts/build-apk.sh.
 * Optional licence name variants cover case differences on case-sensitive FS. */
static const char *const android_data_files[] = {
  "GAME.1", "GAME.2", "GAME.3", "GAME.4",
  "GAME.PCK", "GAME.XXX",
  "LICENCE.DAT", "Licence.Dat", "licence.dat",
  /* optional language packs (assets/lang/…) */
  "lang/en.po",
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
    if (moas_fullscreen == 1)
      flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    {
      int win_w = LOGICAL_W * SCALE;
      int win_h = LOGICAL_H * SCALE;
      /*
       * Touch overlay default: on for Android / web, off for desktop.
       * --touch-controls / --no-touch-controls or the F12 menu override it.
       */
#if defined(__ANDROID__) || defined(__EMSCRIPTEN__) || defined(MOAS_OVERLAY_BUTTONS)
      overlay_on = (moas_touch_controls != 0);
#else
      overlay_on = (moas_touch_controls == 1);
#endif
#ifndef __ANDROID__
      /*
       * Side letterbox for the button columns. overlay_rects needs
       * dx >= ~136 (btn up to 120 + margins); +280 → 140/side when the
       * framebuffer is height-limited (SCALE 3 → 960×600 game area).
       */
      if (overlay_on)
        win_w += OVERLAY_EXTRA_W;
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
                 w, h, SCALE, moas_fullscreen == 1);
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

  if (in_save || !running)
    return; /* after a quit request scripts unwind: state is not faithful */
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
  if (!running) {
    restart_requested = 0; /* window closed while a restart unwinds: quit */
    return;
  }
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

void PlatformRequestRestart(void)
{
  if (!running)
    return;
  /* Same unwind as a quit, but no autosave: that game is being abandoned. */
  restart_requested = 1;
  running = 0;
  KeyAbortState = 2;
}

int PlatformTakeRestart(void)
{
  if (!restart_requested)
    return 0;
  restart_requested = 0;
  running = 1;
  return 1;
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE void PlatformEngineMenu(void);

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
  MoasSettingsSave(); /* needs the window for the fullscreen state */

  /* Destroy SDL objects before freeing our buffers. */
  ov_free_icons();
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

void PlatformSetPaletteEntry(int index, int r, int g, int b)
{
  if (index < 0 || index > 255) return;
  /* VGA DAC registers are 6 bits wide; the upper bits are dropped. */
  r = (r & 63) * 255 / 63;
  g = (g & 63) * 255 / 63;
  b = (b & 63) * 255 / 63;
  sdl_palette[index] = 0xFF000000u | ((Uint32)r << 16) | ((Uint32)g << 8) | (Uint32)b;
}

void PlatformSetPalette(const unsigned char *pal768)
{
  if (!pal768) return;
  for (int i = 0; i < 256; i++)
    PlatformSetPaletteEntry(i, pal768[i * 3 + 0], pal768[i * 3 + 1], pal768[i * 3 + 2]);
}

#define OV_GAP    10
#define OV_MARGIN 8

/*
 * Portrait (taller than wide, e.g. a phone held upright): the picture uses
 * the full width, with one row of buttons above it and two below, instead
 * of squeezing the picture between side columns and leaving the space above
 * and below empty. Returns 0 when there is no room for that. top_h / bot_h
 * are the heights reserved above and below the picture.
 */
static int ov_portrait(int ww, int wh, int *bw, int *bh, int *top_h, int *bot_h)
{
  int w, h, top, bot, dh;

  if (!overlay_on || wh <= ww)
    return 0;
  /* Four across the top row, with a wider gap between the two pairs. */
  w = (ww - 2 * OV_MARGIN - 5 * OV_GAP) / 4;
  if (w > 200)
    w = 200;
  if (w < 64)
    return 0;
  h = (w * 5) / 7;
  if (h < 52)
    h = 52;
  if (h > 120)
    h = 120;
  top = h + 2 * OV_MARGIN;
  bot = 2 * h + OV_GAP + 2 * OV_MARGIN;
  dh = (int)((long)LOGICAL_H * ww / LOGICAL_W);
  if (wh - dh < top + bot)
    return 0;
  if (bw) *bw = w;
  if (bh) *bh = h;
  if (top_h) *top_h = top;
  if (bot_h) *bot_h = bot;
  return 1;
}

/* Shared letterbox geometry for present + mouse (output-pixel space). */
static void letterbox_geom(int *out_x, int *out_y, int *out_w, int *out_h,
                           int *out_ww, int *out_wh)
{
  int ww = LOGICAL_W * SCALE, wh = LOGICAL_H * SCALE;
  int dw, dh, dx, dy, top_h, bot_h;
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

  if (ov_portrait(ww, wh, NULL, NULL, &top_h, &bot_h)) {
    /* Full width, centred between the button rows. */
    dw = ww;
    dh = (int)((long)LOGICAL_H * ww / LOGICAL_W);
    dx = 0;
    dy = top_h + (wh - top_h - bot_h - dh) / 2;
  } else if (overlay_on)
  /*
   * Keep side columns free for the touch overlay (Save/Load/USE/LOOK left,
   * Options/Hint/Skip right).
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


/* Overlay button ids: index into overlay_rects() + 1. */
enum {
  OV_NONE = 0,
  OV_SAVE,
  OV_LOAD,
  OV_SKIP_SCENE,
  OV_SKIP_TEXT,
  OV_MODE_USE,
  OV_MODE_LOOK,
  OV_OPTIONS,
  OV_HINT,
  OV_COUNT = OV_HINT
};

/* ---- 5x7 bitmap font (overlay labels, options menu) -------------------- */
/* bit4 = leftmost pixel of each row */
static const unsigned char ui_font_alpha[26][7] = {
  {0x0e,0x11,0x11,0x1f,0x11,0x11,0x11},{0x1e,0x11,0x11,0x1e,0x11,0x11,0x1e},
  {0x0e,0x11,0x10,0x10,0x10,0x11,0x0e},{0x1e,0x11,0x11,0x11,0x11,0x11,0x1e},
  {0x1f,0x10,0x10,0x1e,0x10,0x10,0x1f},{0x1f,0x10,0x10,0x1e,0x10,0x10,0x10},
  {0x0e,0x11,0x10,0x17,0x11,0x11,0x0e},{0x11,0x11,0x11,0x1f,0x11,0x11,0x11},
  {0x0e,0x04,0x04,0x04,0x04,0x04,0x0e},{0x01,0x01,0x01,0x01,0x11,0x11,0x0e},
  {0x11,0x12,0x14,0x18,0x14,0x12,0x11},{0x10,0x10,0x10,0x10,0x10,0x10,0x1f},
  {0x11,0x1b,0x15,0x15,0x11,0x11,0x11},{0x11,0x19,0x15,0x13,0x11,0x11,0x11},
  {0x0e,0x11,0x11,0x11,0x11,0x11,0x0e},{0x1e,0x11,0x11,0x1e,0x10,0x10,0x10},
  {0x0e,0x11,0x11,0x11,0x15,0x12,0x0d},{0x1e,0x11,0x11,0x1e,0x14,0x12,0x11},
  {0x0e,0x11,0x10,0x0e,0x01,0x11,0x0e},{0x1f,0x04,0x04,0x04,0x04,0x04,0x04},
  {0x11,0x11,0x11,0x11,0x11,0x11,0x0e},{0x11,0x11,0x11,0x11,0x11,0x0a,0x04},
  {0x11,0x11,0x11,0x15,0x15,0x1b,0x11},{0x11,0x11,0x0a,0x04,0x0a,0x11,0x11},
  {0x11,0x11,0x0a,0x04,0x04,0x04,0x04},{0x1f,0x01,0x02,0x04,0x08,0x10,0x1f},
};
static const unsigned char ui_font_digit[10][7] = {
  {0x0e,0x11,0x13,0x15,0x19,0x11,0x0e},{0x04,0x0c,0x04,0x04,0x04,0x04,0x0e},
  {0x0e,0x11,0x01,0x06,0x08,0x10,0x1f},{0x1f,0x01,0x02,0x06,0x01,0x11,0x0e},
  {0x02,0x06,0x0a,0x12,0x1f,0x02,0x02},{0x1f,0x10,0x1e,0x01,0x01,0x11,0x0e},
  {0x06,0x08,0x10,0x1e,0x11,0x11,0x0e},{0x1f,0x01,0x02,0x04,0x08,0x08,0x08},
  {0x0e,0x11,0x11,0x0e,0x11,0x11,0x0e},{0x0e,0x11,0x11,0x0f,0x01,0x02,0x0c},
};
static const struct { char ch; unsigned char rows[7]; } ui_font_punct[] = {
  { '<', {0x02,0x04,0x08,0x10,0x08,0x04,0x02} },
  { '>', {0x08,0x04,0x02,0x01,0x02,0x04,0x08} },
  { '+', {0x00,0x04,0x04,0x1f,0x04,0x04,0x00} },
  { '-', {0x00,0x00,0x00,0x1f,0x00,0x00,0x00} },
  { ':', {0x00,0x0c,0x0c,0x00,0x0c,0x0c,0x00} },
  { '/', {0x01,0x01,0x02,0x04,0x08,0x10,0x10} },
  { '.', {0x00,0x00,0x00,0x00,0x00,0x0c,0x0c} },
  { '?', {0x0e,0x11,0x01,0x02,0x04,0x00,0x04} },
};

static const unsigned char *ui_glyph(char ch)
{
  size_t i;
  if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');
  if (ch >= 'A' && ch <= 'Z') return ui_font_alpha[ch - 'A'];
  if (ch >= '0' && ch <= '9') return ui_font_digit[ch - '0'];
  for (i = 0; i < sizeof(ui_font_punct) / sizeof(ui_font_punct[0]); i++)
    if (ui_font_punct[i].ch == ch)
      return ui_font_punct[i].rows;
  return NULL;
}

static int ui_text_width(const char *s, int scale)
{
  int w = 0;
  for (; *s; s++)
    w += (*s == ' ') ? 4 * scale : 6 * scale;
  return w > 0 ? w - scale : 0;
}

static void ui_draw_text(int x, int y, const char *s, int scale, Uint8 r, Uint8 g, Uint8 b)
{
  SDL_SetRenderDrawColor(renderer, r, g, b, 255);
  for (; *s; s++) {
    const unsigned char *glyph;
    int row, col;
    if (*s == ' ') {
      x += 4 * scale;
      continue;
    }
    glyph = ui_glyph(*s);
    for (row = 0; glyph && row < 7; row++)
      for (col = 0; col < 5; col++)
        if (glyph[row] & (1 << (4 - col))) {
          SDL_Rect p;
          p.x = x + col * scale;
          p.y = y + row * scale;
          p.w = scale;
          p.h = scale;
          SDL_RenderFillRect(renderer, &p);
        }
    x += 6 * scale;
  }
}

/* Text with a 1-unit drop shadow so it reads on any face colour. */
static void ui_draw_label(int x, int y, const char *s, int scale, Uint8 r, Uint8 g, Uint8 b)
{
  ui_draw_text(x + scale, y + scale, s, scale, 0, 0, 0);
  ui_draw_text(x, y, s, scale, r, g, b);
}

static Uint8 ui_shade(int v, int pct)
{
  v = v * pct / 100;
  return (Uint8)(v < 0 ? 0 : (v > 255 ? 255 : v));
}

/* Rounded rectangle, vertical gradient top→bot. Needs BLEND for alpha. */
static void ui_fill_rounded(const SDL_Rect *r, int rad, SDL_Color top, SDL_Color bot)
{
  int y;
  if (r->w <= 0 || r->h <= 0) return;
  if (rad * 2 > r->h) rad = r->h / 2;
  if (rad * 2 > r->w) rad = r->w / 2;
  for (y = 0; y < r->h; y++) {
    int inset = 0, dy = -1;
    int t = (r->h > 1) ? y * 256 / (r->h - 1) : 0;
    if (y < rad)
      dy = rad - y;
    else if (y >= r->h - rad)
      dy = y - (r->h - rad - 1);
    if (dy > 0)
      while (inset < rad && (rad - inset) * (rad - inset) + dy * dy > rad * rad)
        inset++;
    SDL_SetRenderDrawColor(renderer,
                           (Uint8)(top.r + ((bot.r - top.r) * t >> 8)),
                           (Uint8)(top.g + ((bot.g - top.g) * t >> 8)),
                           (Uint8)(top.b + ((bot.b - top.b) * t >> 8)),
                           (Uint8)(top.a + ((bot.a - top.a) * t >> 8)));
    SDL_RenderDrawLine(renderer, r->x + inset, r->y + y,
                       r->x + r->w - 1 - inset, r->y + y);
  }
}

static void ui_fill_rounded_solid(const SDL_Rect *r, int rad, Uint8 R, Uint8 G, Uint8 B, Uint8 A)
{
  SDL_Color c;
  c.r = R; c.g = G; c.b = B; c.a = A;
  ui_fill_rounded(r, rad, c, c);
}

/* ---- Touch overlay ------------------------------------------------------- */
/*
 * Buttons live in the side letterbox (renderer-output pixels), never on top
 * of the 320x200 picture. Left column: Save / Load (top), Use / Look
 * (bottom, thumb reach). Right column: Options / Hint (top), Skip Scene /
 * Skip Text (bottom). Portrait: see ov_portrait().
 * Enabled by default on Android and the web build; elsewhere via
 * --touch-controls or the options menu (F12).
 */

/* 16x16 pixel-art icons, scaled by whole multiples. '.' = transparent. */
static const char *const ov_icon_art[OV_COUNT][16] = {
  /* OV_SAVE */ {
    "................",
    ".kkkkkkkkkkkkk..",
    ".kbbkllllldkbbk.",
    ".kbbklldddlkbbk.",
    ".kbbklldddlkbbk.",
    ".kbbkllllllkbbk.",
    ".kbbbkkkkkkbbbk.",
    ".kbbbbbbbbbbbbk.",
    ".kbwwwwwwwwwwbk.",
    ".kbwddddddddwbk.",
    ".kbwwwwwwwwwwbk.",
    ".kbwddddddddwbk.",
    ".kbwwwwwwwwwwbk.",
    ".kbwwwwwwwwwwbk.",
    ".kkkkkkkkkkkkkk.",
    "................",
  },
  /* OV_LOAD */ {
    "........kk......",
    ".......kGGk.....",
    "......kGGGGk....",
    ".....kGGGGGGk...",
    ".....kkkGGkkk...",
    ".kkkk..kGGk.....",
    ".kyyyk.kGGk.....",
    ".kyyyykkGGkkkkk.",
    ".kyyyyykGGkyyyk.",
    ".kyyyyykkkkyyyk.",
    ".kooooooooooook.",
    ".koyyyyyyyyyyyk.",
    ".koyyyyyyyyyyyk.",
    ".koyyyyyyyyyyyk.",
    ".kkkkkkkkkkkkkk.",
    "................",
  },
  /* OV_SKIP_SCENE: film frame with skip-to-end */ {
    "................",
    ".kkkkkkkkkkkkkk.",
    ".kdwwddwwddwwdk.",
    ".kddddddddddddk.",
    ".kdbbbbbbbbbbdk.",
    ".kdbwbbwbbwwbdk.",
    ".kdbwwbwwbwwbdk.",
    ".kdbwwwwwwwwbdk.",
    ".kdbwwwwwwwwbdk.",
    ".kdbwwbwwbwwbdk.",
    ".kdbwbbwbbwwbdk.",
    ".kdbbbbbbbbbbdk.",
    ".kddddddddddddk.",
    ".kdwwddwwddwwdk.",
    ".kkkkkkkkkkkkkk.",
    "................",
  },
  /* OV_SKIP_TEXT: speech bubble with fast-forward */ {
    "................",
    "..kkkkkkkkkkkk..",
    ".kwwwwwwwwwwwwk.",
    ".kwwwwwwwwwwwwk.",
    ".kwwdwwwdwwwwwk.",
    ".kwwddwwddwwwwk.",
    ".kwwdddwdddwwwk.",
    ".kwwddwwddwwwwk.",
    ".kwwdwwwdwwwwwk.",
    ".kwwwwwwwwwwwwk.",
    "..kwwwkkkkkkkk..",
    "..kwwk..........",
    "..kwk...........",
    "..kk............",
    "................",
    "................",
  },
  /* OV_MODE_USE: hand */ {
    "......kk........",
    ".....kssk.......",
    ".....kssk.......",
    ".....kssk.......",
    ".....ksskkkkkk..",
    ".....ksskssSssk.",
    ".kkk.ksskssSssk.",
    ".kssksssSssSssk.",
    ".kssssssssssssk.",
    "..ksssssssssssk.",
    "...kssssssssssk.",
    "....kssssssssk..",
    ".....kssssssk...",
    ".....kkkkkkkk...",
    "................",
    "................",
  },
  /* OV_MODE_LOOK: eye */ {
    "................",
    "................",
    "................",
    ".....kkkkkk.....",
    "...kkwwwwwwkk...",
    "..kwwwbbbbwwwk..",
    ".kwwwbbbbbbwwwk.",
    "kwwwbbkkwkbbwwwk",
    "kwwwbbkkkkbbwwwk",
    ".kwwwbbbbbbwwwk.",
    "..kwwwbbbbwwwk..",
    "...kkwwwwwwkk...",
    ".....kkkkkk.....",
    "................",
    "................",
    "................",
  },
  /* OV_OPTIONS */ {
    "................",
    "................",
    "................",
    "..kkkkkkkkkkkk..",
    "..kwwwwwwwwwwk..",
    "..kkkkkkkkkkkk..",
    "................",
    "..kkkkkkkkkkkk..",
    "..kwwwwwwwwwwk..",
    "..kkkkkkkkkkkk..",
    "................",
    "..kkkkkkkkkkkk..",
    "..kwwwwwwwwwwk..",
    "..kkkkkkkkkkkk..",
    "................",
    "................",
  },
  /* OV_HINT */ {
    "......kkkk......",
    "....kkyyyykk....",
    "...kyywwyyyyk...",
    "..kyywwyyyyyyk..",
    "..kywwyyyyyyok..",
    "..kyyyyyyyyyok..",
    "..kyyyyyyyyyok..",
    "...kyyyyyyyok...",
    "....kyyyyyok....",
    ".....kyyyok.....",
    ".....kkkkkk.....",
    ".....kllllk.....",
    ".....kddddk.....",
    ".....kllllk.....",
    "......kddk......",
    ".......kk.......",
  },
};
static SDL_Texture *ov_icon_tex[OV_COUNT];

static Uint32 ov_icon_rgba(char c)
{
  switch (c) {
    case 'k': return 0xFF141418u; /* outline */
    case 'w': return 0xFFF4F4F0u; /* white */
    case 'l': return 0xFFB8BEC6u; /* light grey */
    case 'd': return 0xFF5A6068u; /* dark grey */
    case 'b': return 0xFF2E5FB8u; /* floppy blue */
    case 'y': return 0xFFF6C850u; /* folder */
    case 'o': return 0xFFC8902Cu; /* folder shade */
    case 'r': return 0xFFD23C32u; /* stop red */
    case 'G': return 0xFF52D06Au; /* arrow green */
    case 's': return 0xFFF2C69Au; /* skin */
    case 'S': return 0xFFC8946Au; /* skin shade */
    default:  return 0x00000000u;
  }
}

static SDL_Texture *ov_icon(int which)
{
  Uint32 px[16 * 16];
  int x, y;
  SDL_Texture *t;

  if (which < 1 || which > OV_COUNT || !renderer)
    return NULL;
  if (ov_icon_tex[which - 1])
    return ov_icon_tex[which - 1];
  for (y = 0; y < 16; y++)
    for (x = 0; x < 16; x++)
      px[y * 16 + x] = ov_icon_rgba(ov_icon_art[which - 1][y][x]);
  t = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                        SDL_TEXTUREACCESS_STATIC, 16, 16);
  if (!t)
    return NULL;
  SDL_UpdateTexture(t, NULL, px, 16 * (int)sizeof(Uint32));
  SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
  ov_icon_tex[which - 1] = t;
  return t;
}

static void ov_free_icons(void)
{
  int i;
  for (i = 0; i < OV_COUNT; i++)
    if (ov_icon_tex[i]) {
      SDL_DestroyTexture(ov_icon_tex[i]);
      ov_icon_tex[i] = NULL;
    }
}

/* Rects for OV_SAVE..OV_HINT in out[0..OV_COUNT-1]. */
static void overlay_rects(SDL_Rect out[OV_COUNT])
{
  int dx, dy, dw, dh, ww, wh;
  int btn_w, btn_h, gap, margin;
  int left_x, right_x;
  int top_y, low_y;
  int side;

  letterbox_geom(&dx, &dy, &dw, &dh, &ww, &wh);
  gap = OV_GAP;
  margin = OV_MARGIN;

#define OV_SET(i, X, Y) (out[(i) - 1].x = (X), out[(i) - 1].y = (Y), \
                         out[(i) - 1].w = btn_w, out[(i) - 1].h = btn_h)
  if (ov_portrait(ww, wh, &btn_w, &btn_h, NULL, NULL)) {
    /* Save Load -- Hint Options in a row above the picture; below it, the
     * thumb columns of landscape: Use / Look left, Skip Scene / Skip Text
     * right. */
    int step = btn_w + gap;
    left_x = margin;
    right_x = ww - margin - btn_w;
    top_y = margin;
    low_y = wh - margin - 2 * btn_h - gap;
    OV_SET(OV_SAVE,       left_x,          top_y);
    OV_SET(OV_LOAD,       left_x + step,   top_y);
    OV_SET(OV_HINT,       right_x - step,  top_y);
    OV_SET(OV_OPTIONS,       right_x,         top_y);
    OV_SET(OV_MODE_USE,   left_x,          low_y);
    OV_SET(OV_MODE_LOOK,  left_x,          low_y + btn_h + gap);
    OV_SET(OV_SKIP_SCENE, right_x,         low_y);
    OV_SET(OV_SKIP_TEXT,  right_x,         low_y + btn_h + gap);
    return;
  }

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

  if (dx >= btn_w + margin * 2) {
    left_x = (dx - btn_w) / 2;
    if (left_x < margin)
      left_x = margin;
    right_x = dx + dw + (ww - (dx + dw) - btn_w) / 2;
    if (right_x + btn_w > ww - margin)
      right_x = ww - margin - btn_w;
  } else {
    btn_w = 72;
    btn_h = 56;
    left_x = margin;
    right_x = ww - margin - btn_w;
  }
  /* Save/Load and Options/Hint at the top; Use/Look and the skips low for thumbs. */
  top_y = margin;
  low_y = wh - margin - (btn_h * 2 + gap);
  if (low_y < top_y + btn_h * 2 + gap * 3)
    low_y = top_y + btn_h * 2 + gap * 3;

  OV_SET(OV_SAVE,      left_x,  top_y);
  OV_SET(OV_LOAD,      left_x,  top_y + btn_h + gap);
  OV_SET(OV_OPTIONS,      right_x, top_y);
  OV_SET(OV_HINT,      right_x, top_y + btn_h + gap);
  OV_SET(OV_MODE_USE,  left_x,  low_y);
  OV_SET(OV_MODE_LOOK, left_x,  low_y + btn_h + gap);
  OV_SET(OV_SKIP_SCENE, right_x, low_y);
  OV_SET(OV_SKIP_TEXT,  right_x, low_y + btn_h + gap);
#undef OV_SET
}

/* ox/oy are renderer-output pixels (after window_to_output). */
static int overlay_hit(int ox, int oy)
{
  SDL_Rect r[OV_COUNT];
  int i, dx, dy, dw, dh;
  const int pad = 14; /* fingers are fat */

  if (!overlay_on)
    return OV_NONE;
  letterbox_geom(&dx, &dy, &dw, &dh, NULL, NULL);
  /* The game picture always wins, even inside a button's touch padding. */
  if (ox >= dx && ox < dx + dw && oy >= dy && oy < dy + dh)
    return OV_NONE;
  overlay_rects(r);
  for (i = 0; i < OV_COUNT; i++) {
    if (ox >= r[i].x - pad && ox < r[i].x + r[i].w + pad &&
        oy >= r[i].y - pad && oy < r[i].y + r[i].h + pad)
      return i + 1;
  }
  return OV_NONE;
}

/* Esc make code now, break code on the next pump (like a key tap). */
static void synth_esc(void)
{
  if (KeyAbortState == 1)
    KeyAbortState = 2;
  KeyScanCode = 1;
  esc_break_pending = 1;
}

/* ---- Hotspot highlight (H key, HINT button) ---------------------------- */
#define HINT_LINGER_MS 1500 /* still shown after letting go, so a tap works */
#define HINT_FADE_MS    300
#define HINT_MAX         80 /* 50 markers + MAX_PRS persons */
static int    hint_key_down = 0;
static Uint32 hint_until = 0;

static void hint_release(void)
{
  hint_until = SDL_GetTicks() + HINT_LINGER_MS;
}

/* 0..255: full while held, fading out at the end of the linger time. */
static int hint_alpha(void)
{
  Sint32 left;

  if (hint_key_down || overlay_down == OV_HINT)
    return 255;
  if (!hint_until)
    return 0;
  left = (Sint32)(hint_until - SDL_GetTicks());
  if (left <= 0) {
    hint_until = 0;
    return 0;
  }
  return left >= HINT_FADE_MS ? 255 : (int)(left * 255 / HINT_FADE_MS);
}

/* Border of thickness t just inside r. */
static void hint_frame(const SDL_Rect *r, int t)
{
  SDL_Rect e[4] = {
    { r->x, r->y, r->w, t },
    { r->x, r->y + r->h - t, r->w, t },
    { r->x, r->y + t, t, r->h - 2 * t },
    { r->x + r->w - t, r->y + t, t, r->h - 2 * t },
  };
  SDL_RenderFillRects(renderer, e, 4);
}

/*
 * Outline what can be clicked in the room (gold) and, in master mode, the
 * walk-in areas (cyan), over the game picture `pic`.
 */
static void hint_draw(const SDL_Rect *pic)
{
  short r[5 * HINT_MAX];
  int a = hint_alpha(), n, i, u;

  if (!a || !renderer)
    return;
  n = MoasGetHotspots(r, HINT_MAX);
  if (!n)
    return;
  u = pic->h / LOGICAL_H;
  if (u < 1)
    u = 1;
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  for (i = 0; i < n; i++) {
    const short *h = r + 5 * i;
    const SDL_Color c = h[4] ? (SDL_Color){ 80, 210, 255, 255 }
                             : (SDL_Color){ 255, 206, 72, 255 };
    SDL_Rect b, o;
    b.x = pic->x + h[0] * pic->w / LOGICAL_W;
    b.y = pic->y + h[1] * pic->h / LOGICAL_H;
    b.w = pic->x + (h[2] + 1) * pic->w / LOGICAL_W - b.x;
    b.h = pic->y + (h[3] + 1) * pic->h / LOGICAL_H - b.y;
    o.x = b.x - u; o.y = b.y - u; o.w = b.w + 2 * u; o.h = b.h + 2 * u;
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, (Uint8)(a * 50 / 255));
    SDL_RenderFillRect(renderer, &b);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, (Uint8)(a * 160 / 255));
    hint_frame(&o, u);
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, (Uint8)a);
    hint_frame(&b, u);
  }
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

/* SAVE/LOAD/MENU act on release; the rest on press. */
static int overlay_fires_on_release(int which)
{
  return which == OV_SAVE || which == OV_LOAD || which == OV_OPTIONS;
}

static void overlay_fire(int which)
{
  switch (which) {
    case OV_SAVE:
    case OV_LOAD:
      if (!(which == OV_SAVE ? ov_save_ok : ov_load_ok))
        break; /* greyed out */
      /*
       * Queued for LookKeys, which runs SaveGame/LoadGame with the usual
       * control checks (dropped during cutscenes/dialogs). Opening on
       * press would let the same touch close LoadSaveMenu at once.
       */
      overlay_pending_cmd = (which == OV_SAVE) ? 1 : 2;
      MOAS_ALOG("overlay: queued %s", which == OV_SAVE ? "SAVE" : "LOAD");
      break;
    case OV_SKIP_SCENE:
      synth_esc();
      MOAS_ALOG("overlay: ESC abort=%d", (int)KeyAbortState);
      break;
    case OV_SKIP_TEXT:
      /* Synthetic Shift: speed up / dismiss speech bubbles (not full sequence abort). */
      overlay_shift_until = SDL_GetTicks() + SHIFT_HOLD_MS;
      break;
    case OV_MODE_USE:
      touch_mode = MOAS_TOUCH_USE;
      break;
    case OV_MODE_LOOK:
      touch_mode = MOAS_TOUCH_LOOK;
      break;
    case OV_OPTIONS:
      PlatformEngineMenu();
      break;
    default:
      break;
  }
}

/* Pointer went down on button `which` (finger or left mouse). */
static void overlay_press(int which)
{
  overlay_down = which;
  overlay_pressed = which;
  if (!overlay_fires_on_release(which))
    overlay_fire(which);
}

/* Pointer released; `which` is the button under it now (may be OV_NONE). */
static void overlay_release(int which)
{
  int down = overlay_down;
  overlay_down = 0;
  overlay_pressed = 0;
  /* Sliding off the button before lifting cancels, like a desktop button. */
  if (down && which == down && overlay_fires_on_release(down))
    overlay_fire(down);
  if (down == OV_HINT)
    hint_release();
}

int PlatformPollOverlayCmd(void)
{
  int c = overlay_pending_cmd;
  overlay_pending_cmd = 0;
  return c;
}

int PlatformGetTouchMode(void)
{
  return overlay_on ? touch_mode : MOAS_TOUCH_USE;
}

static void overlay_set_enabled(int on)
{
  on = on ? 1 : 0;
  if (on == overlay_on)
    return;
  overlay_on = on;
  overlay_down = overlay_pressed = 0;
  if (!on)
    touch_mode = MOAS_TOUCH_USE;
#if !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
  /* Grow/shrink a plain window so the picture keeps its size. */
  if (window && !(SDL_GetWindowFlags(window) & (SDL_WINDOW_FULLSCREEN |
                  SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_MAXIMIZED))) {
    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    w += on ? OVERLAY_EXTRA_W : -OVERLAY_EXTRA_W;
    if (w >= LOGICAL_W)
      SDL_SetWindowSize(window, w, h);
  }
#endif
  platform_log("touch controls %s", on ? "on" : "off");
}

static int is_touch_device(void);

/* Keyboard equivalent shown in the button's top-right corner while the
 * options menu is open (not on touch). */
static const char *overlay_key_name(int which)
{
  switch (which) {
    case OV_SAVE: return "F5";
    case OV_LOAD: return "F7";
    case OV_SKIP_SCENE: return "ESC";
    case OV_SKIP_TEXT:  return "SHIFT";
    case OV_OPTIONS: return "F12";
    case OV_HINT: return "H";
    case OV_MODE_USE:  return "LMB";
    case OV_MODE_LOOK: return "RMB";
    default:      return NULL;
  }
}

static void overlay_draw_button(int which, const SDL_Rect *r)
{
  static const SDL_Color face_cols[OV_COUNT] = {
    {  40, 132,  70, 255 },  /* Save */
    {  44,  98, 176, 255 },  /* Load */
    { 156,  44,  44, 255 },  /* Skip Scene */
    { 196, 120,  36, 255 },  /* Skip Text  */
    {  70,  74,  96, 255 },  /* Use  */
    {  70,  74,  96, 255 },  /* Look */
    {  70,  74,  96, 255 },  /* Options */
    { 150, 112,  30, 255 },  /* Hint */
  };
  static const int labels[OV_COUNT] = {
    UI_OV_SAVE, UI_OV_LOAD, UI_OV_SKIP_SCENE, UI_OV_SKIP_TEXT, UI_OV_USE, UI_OV_LOOK, UI_OV_OPTIONS,
    UI_OV_HINT
  };
  int disabled = (which == OV_SAVE && !ov_save_ok) ||
                 (which == OV_LOAD && !ov_load_ok);
  int pressed = (overlay_pressed == which) && !disabled;
  int selected = (which == OV_MODE_USE && touch_mode == MOAS_TOUCH_USE) ||
                 (which == OV_MODE_LOOK && touch_mode == MOAS_TOUCH_LOOK);
  SDL_Color c = face_cols[which - 1], top, bot;
  SDL_Rect face = *r, inner, sh;
  const char *label = Ui(labels[which - 1]);
  int rad = r->w / 7;
  int ts, label_h, pad, icon, tw;
  SDL_Texture *tex;

  if (disabled) {
    /* Greyed out: flat grey, dimmed icon and label (below). */
    int g = (c.r * 30 + c.g * 59 + c.b * 11) / 100;
    c.r = c.g = c.b = (Uint8)(g * 3 / 4);
  }
  if (selected) {
    c.r = ui_shade(c.r, 150);
    c.g = ui_shade(c.g, 150);
    c.b = ui_shade(c.b, 170);
  }
  if (pressed) {
    c.r = ui_shade(c.r, 70);
    c.g = ui_shade(c.g, 70);
    c.b = ui_shade(c.b, 70);
    face.y += 2;
  } else {
    sh = face;
    sh.y += 4;
    ui_fill_rounded_solid(&sh, rad, 0, 0, 0, 110);
  }

  /* Rim (gold when the mode is active), then the gradient face. */
  if (selected)
    ui_fill_rounded_solid(&face, rad, 255, 206, 72, 255);
  else
    ui_fill_rounded_solid(&face, rad, ui_shade(c.r, 45), ui_shade(c.g, 45), ui_shade(c.b, 45), 255);
  inner = face;
  inner.x += selected ? 3 : 2;
  inner.y += selected ? 3 : 2;
  inner.w -= selected ? 6 : 4;
  inner.h -= selected ? 6 : 4;
  top.r = ui_shade(c.r, 125); top.g = ui_shade(c.g, 125); top.b = ui_shade(c.b, 125); top.a = 255;
  bot.r = ui_shade(c.r, 80);  bot.g = ui_shade(c.g, 80);  bot.b = ui_shade(c.b, 80);  bot.a = 255;
  ui_fill_rounded(&inner, rad - 2, top, bot);

  /* Label along the bottom; 2x when it fits. */
  ts = (face.h >= 70 && ui_text_width(label, 2) <= inner.w - 8) ? 2 : 1;
  tw = ui_text_width(label, ts);
  label_h = 7 * ts;
  pad = face.h / 16 + 2;

  /* Icon: largest whole multiple of 16 that fits above the label. */
  icon = (face.h - label_h - 3 * pad) / 16 * 16;
  if (icon > (inner.w - 2 * pad) / 16 * 16)
    icon = (inner.w - 2 * pad) / 16 * 16;
  tex = ov_icon(which);
  if (tex && icon >= 16) {
    SDL_Rect dst;
    int u = icon / 16;
    dst.w = dst.h = icon;
    dst.x = face.x + (face.w - icon) / 2;
    dst.y = face.y + pad;
    /* soft drop shadow */
    dst.x += u; dst.y += u;
    SDL_SetTextureColorMod(tex, 0, 0, 0);
    SDL_SetTextureAlphaMod(tex, 110);
    SDL_RenderCopy(renderer, tex, NULL, &dst);
    dst.x -= u; dst.y -= u;
    if (disabled) {
      SDL_SetTextureColorMod(tex, 150, 150, 150);
      SDL_SetTextureAlphaMod(tex, 110);
    } else {
      SDL_SetTextureColorMod(tex, 255, 255, 255);
      SDL_SetTextureAlphaMod(tex, 255);
    }
    SDL_RenderCopy(renderer, tex, NULL, &dst);
    SDL_SetTextureColorMod(tex, 255, 255, 255);
    SDL_SetTextureAlphaMod(tex, 255);
  }
  if (disabled)
    ui_draw_label(face.x + (face.w - tw) / 2, face.y + face.h - label_h - pad,
                  label, ts, 140, 140, 140);
  else
    ui_draw_label(face.x + (face.w - tw) / 2, face.y + face.h - label_h - pad,
                  label, ts, 255, 255, 255);
}

/*
 * Key names on the buttons, drawn over the options menu's dimming so they
 * stay bright. One size for all buttons; covering part of the icon is fine.
 */
static void overlay_draw_keys(void)
{
  SDL_Rect r[OV_COUNT];
  int i;

  if (!renderer || !overlay_on || is_touch_device())
    return;
  overlay_rects(r);
  for (i = 0; i < OV_COUNT; i++) {
    const char *key = overlay_key_name(i + 1);
    int ks = r[i].h >= 70 ? 2 : 1, pad = r[i].h / 16 + 2;
    if (key)
      ui_draw_label(r[i].x + r[i].w - 2 - pad - ui_text_width(key, ks),
                    r[i].y + 2 + pad / 2 + 1, key, ks, 255, 230, 140);
  }
}

/* SAVE/LOAD only do something when LookKeys would honour them; LOAD
 * additionally needs a savegame. The file check is throttled. */
static void overlay_update_availability(void)
{
  static Uint32 last_check;
  static int any_save = -1;
  Uint32 now = SDL_GetTicks();
  int can = MoasCanOpenSaveMenu();

  if (any_save < 0 || now - last_check >= 1000) {
    any_save = MoasAnySaveExists();
    last_check = now;
  }
  ov_save_ok = can;
  ov_load_ok = can && any_save;
}

static void overlay_draw(void)
{
  SDL_Rect r[OV_COUNT];
  int i;

  if (!renderer || !overlay_on)
    return;
  overlay_update_availability();
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  overlay_rects(r);
  for (i = 0; i < OV_COUNT; i++)
    overlay_draw_button(i + 1, &r[i]);
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}


/* ---- Options menu (F12 / overlay OPTIONS / gamepad Back) ------------------ */
/*
 * Port-side settings, not the game's own F10 FileMenu. Works with keyboard,
 * gamepad, mouse (hover + click, wheel, right-click closes) and touch
 * (tap a row; tap the left half of a value to step backwards; tap outside
 * the panel to close).
 */
static int engine_menu_visible;
static int restart_armed; /* RESTART clicked once; next click confirms */

enum { ENG_LANG = 0, ENG_FULLSCREEN, ENG_TOUCH, ENG_SPEED, ENG_SPEECH,
       ENG_RESUME, ENG_RESTART, ENG_QUIT, ENG_COUNT };

typedef struct {
  SDL_Rect panel;
  SDL_Rect item[ENG_COUNT];
  SDL_Rect value[ENG_COUNT]; /* w == 0: no value column */
  int u;                     /* pixel scale of the 5x7 font */
} EngLayout;

static int eng_is_fullscreen(void)
{
  if (!window) return 0;
  return (SDL_GetWindowFlags(window) & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP)) != 0;
}

const char *PlatformPreferredLanguage(void)
{
  const char *lang = "en";
#if SDL_VERSION_ATLEAST(2, 0, 14)
  SDL_Locale *loc = SDL_GetPreferredLocales();
  /* Only the first choice: German as a fallback language is still English. */
  if (loc && loc[0].language && !SDL_strcasecmp(loc[0].language, "de"))
    lang = "de";
  platform_log("os locale: %s -> %s",
               (loc && loc[0].language) ? loc[0].language : "?", lang);
  SDL_free(loc);
#endif
  return lang;
}

int PlatformIsFullscreen(void)
{
  return eng_is_fullscreen();
}

int PlatformTouchControlsOn(void)
{
  return overlay_on;
}

static void eng_toggle_fullscreen(void)
{
  if (!window) return;
  if (eng_is_fullscreen())
    SDL_SetWindowFullscreen(window, 0);
  else
    SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
}

/* F11 or Alt+Enter, ignoring key repeat. */
static int is_fullscreen_key(const SDL_KeyboardEvent *key)
{
  SDL_Keycode k = key->keysym.sym;

  if (key->repeat) return 0;
  return k == SDLK_F11 ||
         ((k == SDLK_RETURN || k == SDLK_KP_ENTER) && (key->keysym.mod & KMOD_ALT));
}

/*
 * Android, or a finger has been used: the overlay OPTIONS button is then the
 * only way back into this menu, so Touch buttons must not turn it off.
 */
static int is_touch_device(void)
{
#ifdef __ANDROID__
  return 1;
#else
  return touch_seen;
#endif
}

static int eng_row_visible(int i)
{
  if (i == ENG_TOUCH)
    return !(overlay_on && is_touch_device());
  return 1;
}

/* Next visible row in direction dir (+1/-1); dir 0 keeps sel if visible. */
static int eng_step(int sel, int dir)
{
  int n;
  if (dir == 0) {
    if (eng_row_visible(sel))
      return sel;
    dir = 1;
  }
  for (n = 0; n < ENG_COUNT; n++) {
    sel = (sel + dir + ENG_COUNT) % ENG_COUNT;
    if (eng_row_visible(sel))
      break;
  }
  return sel;
}

static void eng_layout(EngLayout *L)
{
  int out_w = 640, out_h = 400, u, i, y, row_h, rows = 0;
  const int cols = 24; /* widest row: "SPEECH SPEED    < 24 >" */

  for (i = 0; i < ENG_COUNT; i++)
    rows += eng_row_visible(i);
  SDL_GetRendererOutputSize(renderer, &out_w, &out_h);
  u = out_h / 160;
  if (u > 4) u = 4;
  if (u < 1) u = 1;
  /* panel: 6u margin, 7u title + 6u gap, rows of 13u, 6u hint gap + 7 + 6u */
  while (u > 1 && ((cols * 6 + 12) * u > out_w - 16 ||
                   (6 + 13 + rows * 13 + 6) * u + 7 + 6 * u > out_h - 16))
    u--;
  L->u = u;
  L->panel.w = (cols * 6 + 12) * u;
  L->panel.h = (6 + 13 + rows * 13 + 6) * u + 7 + 6 * u;
  L->panel.x = (out_w - L->panel.w) / 2;
  L->panel.y = (out_h - L->panel.h) / 2;
  row_h = 13 * u;
  y = L->panel.y + (6 + 13) * u;
  for (i = 0; i < ENG_COUNT; i++) {
    if (!eng_row_visible(i)) {
      memset(&L->item[i], 0, sizeof(L->item[i]));
      L->value[i] = L->item[i];
      continue;
    }
    L->item[i].x = L->panel.x + 4 * u;
    L->item[i].y = y;
    L->item[i].w = L->panel.w - 8 * u;
    L->item[i].h = row_h - u;
    L->value[i] = L->item[i];
    if (i <= ENG_SPEECH) {
      L->value[i].w = 9 * 6 * u;
      L->value[i].x = L->item[i].x + L->item[i].w - L->value[i].w - 2 * u;
    } else
      L->value[i].w = 0;
    y += row_h;
  }
}

static int eng_hit(const EngLayout *L, int x, int y)
{
  int i;
  for (i = 0; i < ENG_COUNT; i++)
    if (x >= L->item[i].x && x < L->item[i].x + L->item[i].w &&
        y >= L->item[i].y && y < L->item[i].y + L->item[i].h)
      return i;
  return -1;
}

static int eng_in_panel(const EngLayout *L, int x, int y)
{
  return x >= L->panel.x && x < L->panel.x + L->panel.w &&
         y >= L->panel.y && y < L->panel.y + L->panel.h;
}

/* Tap/click on a row: left half of the value column steps back. */
static int eng_dir_at(const EngLayout *L, int item, int x)
{
  if (L->value[item].w > 0 && x >= L->value[item].x &&
      x < L->value[item].x + L->value[item].w / 2)
    return -1;
  return 1;
}

static void eng_menu_draw(const EngLayout *L, int sel, int pressed)
{
  int out_w = 640, out_h = 400, i, u = L->u;
  SDL_Rect dim, rim, bar;
  char val[16];
  const char *title = Ui(UI_OPT_TITLE);
  const char *hint = Ui(overlay_on ? UI_OPT_HINT_TOUCH : UI_OPT_HINT_KEYS);

  SDL_GetRendererOutputSize(renderer, &out_w, &out_h);
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  dim.x = 0; dim.y = 0; dim.w = out_w; dim.h = out_h;
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 170);
  SDL_RenderFillRect(renderer, &dim);

  rim = L->panel;
  ui_fill_rounded_solid(&rim, 4 * u, 200, 200, 220, 255);
  rim.x += u; rim.y += u; rim.w -= 2 * u; rim.h -= 2 * u;
  {
    SDL_Color top = { 34, 40, 60, 250 }, bot = { 18, 20, 32, 250 };
    ui_fill_rounded(&rim, 3 * u, top, bot);
  }
  ui_draw_label(L->panel.x + (L->panel.w - ui_text_width(title, u)) / 2,
                L->panel.y + 6 * u, title, u, 255, 206, 72);

  for (i = 0; i < ENG_COUNT; i++) {
    const SDL_Rect *it = &L->item[i];
    const char *label = "";
    int ty = it->y + (it->h - 7 * u) / 2;

    if (!eng_row_visible(i))
      continue;
    if (i == sel || i == pressed) {
      bar = *it;
      if (i == pressed)
        ui_fill_rounded_solid(&bar, 2 * u, 40, 62, 120, 255);
      else
        ui_fill_rounded_solid(&bar, 2 * u, 62, 94, 168, 255);
    }
    val[0] = 0;
    switch (i) {
      case ENG_LANG:
        label = Ui(UI_OPT_LANGUAGE);
        snprintf(val, sizeof(val), "%s", (LangGetCode() && LangGetCode()[0]) ? LangGetCode() : "de");
        break;
      case ENG_FULLSCREEN:
        label = Ui(UI_OPT_FULLSCREEN);
        snprintf(val, sizeof(val), "%s", Ui(eng_is_fullscreen() ? UI_OPT_ON : UI_OPT_OFF));
        break;
      case ENG_TOUCH:
        label = Ui(UI_OPT_TOUCH);
        snprintf(val, sizeof(val), "%s", Ui(overlay_on ? UI_OPT_ON : UI_OPT_OFF));
        break;
      case ENG_SPEED:
        label = Ui(UI_OPT_SPEED);
        snprintf(val, sizeof(val), "%d", (int)TotalSpeed);
        break;
      case ENG_SPEECH:
        label = Ui(UI_OPT_SPEECH);
        snprintf(val, sizeof(val), "%d", (int)SpeechSpeed);
        break;
      case ENG_RESUME:
        label = Ui(UI_OPT_RESUME);
        break;
      case ENG_RESTART:
        label = Ui(restart_armed ? UI_OPT_RESTART_SURE : UI_OPT_RESTART);
        break;
      case ENG_QUIT:
        label = Ui(UI_OPT_QUIT);
        break;
    }
    if (L->value[i].w > 0) {
      const SDL_Rect *v = &L->value[i];
      ui_draw_label(it->x + 3 * u, ty, label, u, 255, 255, 255);
      ui_draw_label(v->x, ty, "<", u, 150, 170, 210);
      ui_draw_label(v->x + v->w - 5 * u, ty, ">", u, 150, 170, 210);
      ui_draw_label(v->x + (v->w - ui_text_width(val, u)) / 2, ty, val, u, 255, 230, 140);
    } else {
      ui_draw_label(it->x + (it->w - ui_text_width(label, u)) / 2, ty, label, u, 255, 255, 255);
    }
  }
  {
    int hs = u > 1 ? u - 1 : 1;
    if (ui_text_width(hint, hs) > L->panel.w - 8)
      hs = 1;
    ui_draw_text(L->panel.x + (L->panel.w - ui_text_width(hint, hs)) / 2,
                 L->panel.y + L->panel.h - 6 * u - 7 * hs / 2,
                 hint, hs, 150, 150, 175);
  }
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

/* dir: +1 next / activate, -1 previous. */
static void eng_menu_change(int sel, int dir)
{
  const char *code;
  switch (sel) {
    case ENG_LANG:
      code = LangGetCode();
      LangSetCode((code && strcmp(code, "en") == 0) ? "de" : "en");
      MoasReloadLanguage();
      break;
    case ENG_FULLSCREEN:
      eng_toggle_fullscreen();
      break;
    case ENG_TOUCH:
      if (eng_row_visible(ENG_TOUCH))
        overlay_set_enabled(!overlay_on);
      break;
    case ENG_SPEED:
      /* Same 1..8 range as the game's own F10 menu. */
      TotalSpeed = (short)(TotalSpeed + dir);
      if (TotalSpeed < 1) TotalSpeed = 8;
      if (TotalSpeed > 8) TotalSpeed = 1;
      break;
    case ENG_SPEECH:
      SpeechSpeed = (short)(SpeechSpeed + dir);
      if (SpeechSpeed < 1) SpeechSpeed = 24;
      if (SpeechSpeed > 24) SpeechSpeed = 1;
      break;
    case ENG_RESUME:
      if (dir > 0)
        engine_menu_visible = 0;
      break;
    case ENG_RESTART:
      /* Autosave makes this the only way back to the intro; ask twice. */
      if (dir > 0) {
        if (!restart_armed) {
          restart_armed = 1;
          return;
        }
        engine_menu_visible = 0;
        PlatformRequestRestart();
      }
      break;
    case ENG_QUIT:
      if (dir > 0) {
        engine_menu_visible = 0;
        PlatformRequestQuit();
      }
      break;
  }
}

/* ---- Key hints (start screen and options menu) -------------------------- */
static const struct { const char *key; int text; } keyhint_rows[] = {
  { "F5",    UI_KEY_SAVE },
  { "F7",    UI_KEY_LOAD },
  { "F12",   UI_KEY_OPTIONS },
  { "SHIFT", UI_KEY_SKIP_TEXT },
  { "ESC",   UI_KEY_SKIP_SCENE },
  { "H",     UI_KEY_HOTSPOTS },
};
#define KEYHINT_ROWS ((int)(sizeof(keyhint_rows) / sizeof(keyhint_rows[0])))

/* The title room (prog 4: "Intro", "Anleitung", copyright notes). */
static int keyhints_on_start_screen(void)
{
  return CurrentProc == 4;
}

/*
 * Bottom right of the game picture, for keyboard players. Skipped on touch
 * devices and when it would cover `avoid` (the options panel).
 */
static void keyhints_draw(const SDL_Rect *avoid)
{
  int dx, dy, dw, dh, u, i, kw = 0, tw = 0, pad, line_h;
  SDL_Rect box;

  if (!renderer || is_touch_device())
    return;
  letterbox_geom(&dx, &dy, &dw, &dh, NULL, NULL);
  u = dh / 300;
  if (u < 1)
    u = 1;
  for (i = 0; i < KEYHINT_ROWS; i++) {
    int w = ui_text_width(keyhint_rows[i].key, u);
    if (w > kw) kw = w;
    w = ui_text_width(Ui(keyhint_rows[i].text), u);
    if (w > tw) tw = w;
  }
  pad = 3 * u;
  line_h = 10 * u;
  box.w = pad + kw + 5 * u + tw + pad;
  box.h = pad + KEYHINT_ROWS * line_h - 3 * u + pad;
  box.x = dx + dw - box.w - 4 * u;
  box.y = dy + dh - box.h - 4 * u;
  if (avoid && SDL_HasIntersection(&box, avoid))
    return;

  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  ui_fill_rounded_solid(&box, 2 * u, 0, 0, 0, 150);
  for (i = 0; i < KEYHINT_ROWS; i++) {
    int y = box.y + pad + i * line_h;
    ui_draw_label(box.x + pad, y, keyhint_rows[i].key, u, 255, 206, 72);
    ui_draw_label(box.x + pad + kw + 5 * u, y, Ui(keyhint_rows[i].text), u, 255, 255, 255);
  }
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

/*
 * Centred on the screen just below the options panel: the bottom edge has
 * the portrait touch buttons, the game picture can sit under the panel.
 */
static void version_draw(const EngLayout *L)
{
  int out_w = 640, out_h = 400, u = L->u > 1 ? L->u - 1 : 1, y;
  char buf[96];

  SDL_GetRendererOutputSize(renderer, &out_w, &out_h);
  snprintf(buf, sizeof(buf), "MOAS %s", MOAS_VERSION);
  if (ui_text_width(buf, u) > L->panel.w)
    u = 1;
  y = L->panel.y + L->panel.h + 4 * L->u;
  if (y > out_h - 8 * u)
    y = out_h - 8 * u;
  ui_draw_label((out_w - ui_text_width(buf, u)) / 2, y, buf, u, 150, 150, 175);
}

/* Forward: PlatformPresent draws the game + overlay under the menu. */
static void platform_present_frame(void);
static void pad_device_event(const SDL_Event *ev);
static void platform_try_autosave(const char *reason);

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
void PlatformEngineMenu(void)
{
  static int in_menu = 0;
  int sel = 0, pressed = -1, outside = 0;
  SDL_Event ev;
  EngLayout L;

  if (!renderer || !window || in_menu)
    return;
  in_menu = 1;
  engine_menu_visible = 1;
  restart_armed = 0;
  while (engine_menu_visible && running) {
    eng_layout(&L);
    sel = eng_step(sel, 0);
    platform_present_frame();
    eng_menu_draw(&L, sel, pressed);
    overlay_draw_keys();
    keyhints_draw(&L.panel);
    version_draw(&L);
    SDL_RenderPresent(renderer);

    while (engine_menu_visible && SDL_PollEvent(&ev)) {
      int px = 0, py = 0, down = 0, up = 0;

      switch (ev.type) {
        case SDL_QUIT:
          PlatformRequestQuit();
          engine_menu_visible = 0;
          break;
#ifdef __ANDROID__
        case SDL_APP_WILLENTERBACKGROUND:
        case SDL_APP_DIDENTERBACKGROUND:
          platform_try_autosave("background (menu)");
          break;
        case SDL_APP_TERMINATING:
          PlatformRequestQuit();
          break;
#endif
        case SDL_CONTROLLERDEVICEADDED:
        case SDL_CONTROLLERDEVICEREMOVED:
          pad_device_event(&ev);
          break;
        case SDL_KEYDOWN: {
          SDL_Keycode k = ev.key.keysym.sym;
          if (is_fullscreen_key(&ev.key))
            eng_toggle_fullscreen();
          else if (k == SDLK_ESCAPE || k == SDLK_F12 || k == SDLK_AC_BACK)
            engine_menu_visible = 0;
          else if (k == SDLK_UP || k == SDLK_w)
            sel = eng_step(sel, -1);
          else if (k == SDLK_DOWN || k == SDLK_s)
            sel = eng_step(sel, 1);
          else if (k == SDLK_LEFT)
            eng_menu_change(sel, -1);
          else if (k == SDLK_RIGHT || k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE)
            eng_menu_change(sel, 1);
          break;
        }
        case SDL_CONTROLLERBUTTONDOWN:
          switch (ev.cbutton.button) {
            case SDL_CONTROLLER_BUTTON_DPAD_UP:
              sel = eng_step(sel, -1);
              break;
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
              sel = eng_step(sel, 1);
              break;
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
              eng_menu_change(sel, -1);
              break;
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
            case SDL_CONTROLLER_BUTTON_A:
            case SDL_CONTROLLER_BUTTON_X:
              eng_menu_change(sel, 1);
              break;
            case SDL_CONTROLLER_BUTTON_B:
            case SDL_CONTROLLER_BUTTON_START:
            case SDL_CONTROLLER_BUTTON_BACK:
              engine_menu_visible = 0;
              break;
          }
          break;
        case SDL_MOUSEMOTION:
          if (ev.motion.which == SDL_TOUCH_MOUSEID)
            break;
          window_to_output(ev.motion.x, ev.motion.y, &px, &py);
          {
            int i = eng_hit(&L, px, py);
            if (i >= 0)
              sel = i;
          }
          break;
        case SDL_MOUSEWHEEL:
          if (ev.wheel.y > 0)
            sel = eng_step(sel, -1);
          else if (ev.wheel.y < 0)
            sel = eng_step(sel, 1);
          break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
          if (ev.button.which == SDL_TOUCH_MOUSEID)
            break;
          if (ev.button.button == SDL_BUTTON_RIGHT) {
            if (ev.type == SDL_MOUSEBUTTONUP)
              engine_menu_visible = 0;
            break;
          }
          if (ev.button.button != SDL_BUTTON_LEFT)
            break;
          window_to_output(ev.button.x, ev.button.y, &px, &py);
          down = (ev.type == SDL_MOUSEBUTTONDOWN);
          up = !down;
          break;
        case SDL_FINGERDOWN:
        case SDL_FINGERUP: {
          int ww = 1, wh = 1;
#ifdef SDL_MOUSE_TOUCHID
          if (ev.tfinger.touchId == SDL_MOUSE_TOUCHID)
            break;
#endif
          touch_seen = 1;
          SDL_GetWindowSize(window, &ww, &wh);
          window_to_output((int)(ev.tfinger.x * (float)ww + 0.5f),
                           (int)(ev.tfinger.y * (float)wh + 0.5f), &px, &py);
          down = (ev.type == SDL_FINGERDOWN);
          up = !down;
          break;
        }
        default:
          break;
      }

      if (down) {
        pressed = eng_hit(&L, px, py);
        outside = !eng_in_panel(&L, px, py);
        if (pressed >= 0)
          sel = pressed;
      } else if (up) {
        int i = eng_hit(&L, px, py);
        if (i >= 0 && i == pressed)
          eng_menu_change(i, eng_dir_at(&L, i, px));
        else if (outside && !eng_in_panel(&L, px, py))
          engine_menu_visible = 0;
        pressed = -1;
        outside = 0;
      }
      if (sel != ENG_RESTART)
        restart_armed = 0; /* moved away: RESTART needs two clicks again */
    }
    SDL_Delay(16);
  }
  engine_menu_visible = 0;
  in_menu = 0;
  hint_key_down = 0; /* its key-up went to the menu loop */
  MoasSettingsSave();
  if (Screen)
    PowerRedrawArea(0, 0, 319, 199);
}

static void platform_present_frame(void)
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
    if (!engine_menu_visible)
      hint_draw(&dst);
  }
  /* Touch buttons sit in the letterbox, beside the game picture. */
  overlay_draw();
  if (!engine_menu_visible && keyhints_on_start_screen())
    keyhints_draw(NULL);
}

void PlatformPresent(void)
{
  platform_present_frame();
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
 * Back                → options menu (has Quit)
 * X / Y               → USE / LOOK touch mode (when touch buttons are on)
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
    overlay_shift_until = SDL_GetTicks() + SHIFT_HOLD_MS; /* skip speech while held */
  /* B / Start / Back / X / Y are edge-triggered in pad_button_down. */

#ifndef DOS
  if (pad_walk_cooldown > 0)
    pad_walk_cooldown--;
  else if (walk_x || walk_y) {
    MoasPadWalk((walk_x * 1000) / 32767, (walk_y * 1000) / 32767);
    pad_walk_cooldown = 4; /* ~throttle WalkClick spam */
  }
#endif
}

static void pad_device_event(const SDL_Event *ev)
{
  if (ev->type == SDL_CONTROLLERDEVICEADDED) {
    if (!pad) {
      pad = SDL_GameControllerOpen(ev->cdevice.which);
      if (pad)
        platform_log("gamepad connected: %s", SDL_GameControllerName(pad));
    }
  } else if (ev->type == SDL_CONTROLLERDEVICEREMOVED) {
    if (pad && ev->cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad))) {
      SDL_GameControllerClose(pad);
      pad = NULL;
      pad_use = pad_look = 0;
      platform_log("gamepad disconnected");
    }
  }
}

static void pad_button_down(const SDL_ControllerButtonEvent *cb)
{
  if (!pad || cb->which != SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad)))
    return;
  switch (cb->button) {
    case SDL_CONTROLLER_BUTTON_B:
    case SDL_CONTROLLER_BUTTON_START:
      synth_esc();
      break;
    case SDL_CONTROLLER_BUTTON_BACK:
      PlatformEngineMenu();
      break;
    case SDL_CONTROLLER_BUTTON_X:
      if (overlay_on)
        touch_mode = MOAS_TOUCH_USE;
      break;
    case SDL_CONTROLLER_BUTTON_Y:
      if (overlay_on)
        touch_mode = MOAS_TOUCH_LOOK;
      break;
    default:
      break;
  }
}

/* Finger position: window coords (for mouse_to_logical) + output pixels. */
static void finger_pos(const SDL_TouchFingerEvent *tf, int *wx, int *wy, int *ox, int *oy)
{
  int ww = 1, wh = 1;
  SDL_GetWindowSize(window, &ww, &wh);
  *wx = (int)(tf->x * (float)ww + 0.5f);
  *wy = (int)(tf->y * (float)wh + 0.5f);
  window_to_output(*wx, *wy, ox, oy);
}

/*
 * Touch input is handled from SDL_FINGER* only. SDL also synthesizes mouse
 * events for the first finger (which == SDL_TOUCH_MOUSEID); those are
 * ignored so a tap is not seen twice and a finger on an overlay button does
 * not drag the game cursor into the letterbox.
 */
static void handle_finger(const SDL_Event *ev)
{
  const SDL_TouchFingerEvent *tf = &ev->tfinger;
  int wx, wy, ox, oy, ov;
  short mx, my;

#ifdef SDL_MOUSE_TOUCHID
  if (tf->touchId == SDL_MOUSE_TOUCHID)
    return; /* mouse-generated touch (SDL_HINT_MOUSE_TOUCH_EVENTS) */
#endif
  pointer_is_touch = 1;
  touch_seen = 1;
  finger_pos(tf, &wx, &wy, &ox, &oy);
  ov = overlay_hit(ox, oy);

  switch (ev->type) {
    case SDL_FINGERDOWN:
      if (ov) {
        if (!overlay_down) {
          overlay_finger = tf->fingerId;
          overlay_press(ov);
        }
        return;
      }
      if (game_finger_down)
        return; /* one finger drives the game cursor */
      game_finger = tf->fingerId;
      game_finger_down = 1;
      mouse_to_logical(wx, wy, &mx, &my);
      Mouse.x = mx;
      Mouse.y = my;
      phys_left = 1;
      Mouse.Left = 1;
      Mouse.ClickX = Mouse.x;
      Mouse.ClickY = Mouse.y;
      return;
    case SDL_FINGERMOTION:
      if (overlay_down && tf->fingerId == overlay_finger) {
        overlay_pressed = (ov == overlay_down) ? overlay_down : 0;
        return;
      }
      if (game_finger_down && tf->fingerId == game_finger) {
        mouse_to_logical(wx, wy, &mx, &my);
        Mouse.x = mx;
        Mouse.y = my;
      }
      return;
    case SDL_FINGERUP:
      if (overlay_down && tf->fingerId == overlay_finger) {
        overlay_release(ov);
        return;
      }
      if (game_finger_down && tf->fingerId == game_finger) {
        game_finger_down = 0;
        mouse_to_logical(wx, wy, &mx, &my);
        Mouse.x = mx;
        Mouse.y = my;
        phys_left = 0;
        Mouse.Left = pad_use ? 1 : 0;
        Mouse.ReleaseX = Mouse.x;
        Mouse.ReleaseY = Mouse.y;
      }
      return;
  }
}

void PlatformPumpEvents(void)
{
  SDL_Event ev;

  if (esc_break_pending) {
    /* Synthetic Esc tap: LookKeys saw the make code last frame. */
    if (KeyScanCode == 1)
      KeyScanCode = 1 + 128;
    esc_break_pending = 0;
  }
  while (SDL_PollEvent(&ev)) {
    switch (ev.type) {
      case SDL_QUIT:
        PlatformRequestQuit();
        break;
      case SDL_CONTROLLERDEVICEADDED:
      case SDL_CONTROLLERDEVICEREMOVED:
        pad_device_event(&ev);
        break;
      case SDL_CONTROLLERBUTTONDOWN:
        pad_button_down(&ev.cbutton);
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
        if (ev.motion.which == SDL_TOUCH_MOUSEID)
          break;
        pointer_is_touch = 0;
        /* Physical mouse motion takes ownership back from the stick. */
        if (ev.motion.xrel != 0 || ev.motion.yrel != 0)
          pad_aiming = 0;
        if (overlay_down) {
          int rx, ry;
          window_to_output(ev.motion.x, ev.motion.y, &rx, &ry);
          overlay_pressed = (overlay_hit(rx, ry) == overlay_down) ? overlay_down : 0;
        }
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
        short mx, my;
        if (ev.button.which == SDL_TOUCH_MOUSEID)
          break; /* handled as SDL_FINGER* */
        pointer_is_touch = 0;
        window_to_output(ev.button.x, ev.button.y, &rx, &ry);
        ov = overlay_hit(rx, ry);
        if (ev.button.button == SDL_BUTTON_LEFT) {
          if (down && ov) {
            overlay_press(ov);
            break; /* not a game click */
          }
          if (!down && overlay_down) {
            overlay_release(ov);
            break;
          }
        } else if (down && ov) {
          break; /* right/middle on a button: nothing */
        }
        /* Releases always reach the game, even over the letterbox. */
        mouse_to_logical(ev.button.x, ev.button.y, &mx, &my);
        Mouse.x = mx;
        Mouse.y = my;
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
      case SDL_FINGERDOWN:
      case SDL_FINGERUP:
      case SDL_FINGERMOTION:
        handle_finger(&ev);
        break;
      case SDL_KEYDOWN:
      case SDL_KEYUP: {
        SDL_Keycode k = ev.key.keysym.sym;
        SDL_Keymod mod = (SDL_Keymod)ev.key.keysym.mod;
        SDL_Scancode sc = ev.key.keysym.scancode;
        int dos_sc = sdl_to_dos_scancode(sc);
        int down = (ev.type == SDL_KEYDOWN);
        moas_shift_down = (mod & KMOD_SHIFT) ? 1 : 0;

        /* Hotspot highlight while H is held. The key still reaches the game
         * (text entry); LookKeys ignores it. */
        if (k == SDLK_h) {
          if (!down) {
            if (hint_key_down)
              hint_release();
            hint_key_down = 0;
          } else if (!(mod & (KMOD_ALT | KMOD_CTRL | KMOD_GUI))) {
            hint_key_down = 1;
          }
        }

        /* Handled before KeyScanCode so Alt+Enter doesn't also act as Enter. */
        if (down && is_fullscreen_key(&ev.key)) {
          eng_toggle_fullscreen();
          continue;
        }

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
          if (k == SDLK_F12) {
            PlatformEngineMenu();
            continue;
          }
          /* Alt+Q (DOS quit key) arrives via the conio queue below. */
          if ((k == SDLK_c && (mod & KMOD_CTRL)) ||
              (k == SDLK_F4 && (mod & KMOD_ALT)) ||
              k == SDLK_AC_BACK)
            PlatformRequestQuit();

          /* Feed DOS conio queue for LookKeys kbhit/getch path */
          {
            int ext = dos_ext_getch(sc);
            if ((mod & KMOD_ALT) && k >= SDLK_a && k <= SDLK_z && dos_sc > 0) {
              /* BIOS: Alt+letter = 0, scancode (Alt+Q quit, master keys). */
              key_enqueue(0);
              key_enqueue((unsigned char)dos_sc);
            } else if (ext >= 0) {
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
  if (!pad_aiming && !pointer_is_touch)
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
