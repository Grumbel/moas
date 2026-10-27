/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef MOAS_DEBUG_H
#define MOAS_DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif

/* Bit flags — combine freely */
#define MOAS_DBG_SCRIPT   (1u << 0)  /* Execute opcodes */
#define MOAS_DBG_INPUT    (1u << 1)  /* mouse/keyboard edges */
#define MOAS_DBG_WAIT     (1u << 2)  /* GetClickOrEnter / Wait / selects */
#define MOAS_DBG_WINDOW   (1u << 3)  /* Open/Close window, cursor state */
#define MOAS_DBG_LOAD     (1u << 4)  /* data loads */
#define MOAS_DBG_ALL      (0xffffu)

#ifdef DOS
/* DOS build: no CLI debug plumbing — keep symbols so game code compiles. */
static unsigned moas_debug = 0;
static int moas_skip_intro = 0;
static int moas_start_proc = -1;
static int moas_start_addr = 0;
static int moas_load_slot = -1;
static char moas_load_path[512];
static int moas_fullscreen = 0;
static int moas_no_autosave = 0; /* DOS: no CLI; always allow AUTOSAVE.SAV */
static int moas_touch_controls = 0;
static int moas_aspect = 1;
static int moas_lang_explicit = 0;
static int moas_master = 0;
static void MoasParseArgs(int argc, char **argv) { (void)argc; (void)argv; }
static void MoasDbg(unsigned flag, const char *fmt, ...) { (void)flag; (void)fmt; }
#else
extern unsigned moas_debug;
extern int moas_skip_intro;   /* skip Execute(3,0) */
extern int moas_start_proc;   /* -1 = default 0 after intro */
extern int moas_start_addr;
extern int moas_load_slot;     /* -1 = none; 0..9 = GAME{n}.SAV */
extern char moas_load_path[512]; /* explicit path; empty if unset */
extern int moas_fullscreen;   /* -1 = settings.ini / windowed, 0 = window, 1 = desktop fullscreen */
extern int moas_no_autosave;  /* 1 = skip exit AUTOSAVE.SAV and startup restore */
extern int moas_touch_controls; /* -1 = platform default, 0 = off, 1 = on-screen buttons */
extern int moas_aspect;       /* -1 = settings.ini / 4:3, 0 = 8:5 (square pixels), 1 = 4:3 */
extern int moas_lang_explicit; /* 1 = --lang / MOAS_LANG given; settings.ini must not override */
extern int moas_master;       /* 1 = -d/--debug: master mode on at start, Alt+T works without THPRIV */

void MoasParseArgs(int argc, char **argv);
/* "4:3" → 1, "8:5" (or "16:10") → 0, else -1. */
int MoasParseAspect(const char *s);
void MoasDbg(unsigned flag, const char *fmt, ...);
#endif

#ifdef __cplusplus
}
#endif

#endif
