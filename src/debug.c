/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "debug.h"
#include "lang.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#ifdef __ANDROID__
#include <SDL.h>
#endif

unsigned moas_debug = 0;
int moas_skip_intro = 0;
int moas_start_proc = -1;
int moas_start_addr = 0;
int moas_load_slot = -1;
char moas_load_path[512];
int moas_fullscreen = 0;
int moas_no_autosave = 0;

static void usage(const char *argv0)
{
  fprintf(stderr,
    "Usage: %s [options]\n"
    "\n"
    "Options:\n"
    "  -h, --help              Show this help\n"
    "  -d, --debug=LIST        Debug flags (script,input,wait,window,load,all)\n"
    "  --skip-intro            Skip intro/title script\n"
    "  --proc=N                Start program N after intro\n"
    "  --addr=N                Start address in that program\n"
    "  --datadir=PATH          Game data directory (MOAS_DATADIR)\n"
    "  --statedir=PATH         Savegame directory (MOAS_STATEDIR)\n"
    "  --load=SLOT|FILE        Load save at startup (slot 0-9 or path to .SAV)\n"
    "  --lang=CODE             UI/script language (default de; loads lang/CODE.*)\n"
    "  --load SLOT|FILE        Same as --load=\n"
    "  --no-autosave           Do not restore/write AUTOSAVE.SAV on start/exit\n"
    "  -f, --fullscreen        Desktop fullscreen\n"
    "\n"
    "Examples:\n"
    "  %s --load=0\n"
    "  %s --load=~/.local/state/moas/GAME3.SAV\n"
    "  %s --skip-intro --debug=script\n"
    "  MOAS_DEBUG=all %s\n",
    argv0, argv0, argv0, argv0, argv0);
}

static unsigned parse_debug_list(const char *list)
{
  unsigned flags = 0;
  char buf[256];
  char *save = NULL;
  char *tok;
  size_t n;

  if (!list || !*list) return MOAS_DBG_ALL;
  n = strlen(list);
  if (n >= sizeof(buf)) n = sizeof(buf) - 1;
  memcpy(buf, list, n);
  buf[n] = 0;

  for (tok = strtok_r(buf, ",+:|", &save); tok; tok = strtok_r(NULL, ",+:|", &save)) {
    while (*tok == ' ' || *tok == '\t') tok++;
    if (!*tok) continue;
    if (!strcmp(tok, "all"))           flags |= MOAS_DBG_ALL;
    else if (!strcmp(tok, "script"))   flags |= MOAS_DBG_SCRIPT;
    else if (!strcmp(tok, "input"))    flags |= MOAS_DBG_INPUT;
    else if (!strcmp(tok, "wait"))     flags |= MOAS_DBG_WAIT;
    else if (!strcmp(tok, "window"))   flags |= MOAS_DBG_WINDOW;
    else if (!strcmp(tok, "load"))     flags |= MOAS_DBG_LOAD;
    else fprintf(stderr, "moas: unknown debug flag '%s'\n", tok);
  }
  return flags;
}

void MoasParseArgs(int argc, char **argv)
{
  int i;
  const char *env;

  env = getenv("MOAS_DEBUG");
  if (env && *env)
    moas_debug |= parse_debug_list(env);
  env = getenv("MOAS_LANG");
  if (env && *env)
    LangSetCode(env);
#ifdef __ANDROID__
  /* Always surface load diagnostics on logcat until the port is solid. */
  if (!env || !*env)
    moas_debug |= MOAS_DBG_LOAD;
#endif

  for (i = 1; i < argc; i++) {
    const char *a = argv[i];
    if (!strcmp(a, "-h") || !strcmp(a, "--help")) {
      usage(argv[0] ? argv[0] : "moas");
      exit(0);
    }
    if (!strcmp(a, "--skip-intro")) {
      moas_skip_intro = 1;
      continue;
    }
    if (!strncmp(a, "--lang=", 7)) {
      LangSetCode(a + 7);
      continue;
    }
    if (!strcmp(a, "--lang") && i + 1 < argc) {
      LangSetCode(argv[++i]);
      continue;
    }
    if (!strncmp(a, "--debug=", 8)) {
      moas_debug |= parse_debug_list(a + 8);
      continue;
    }
    if (!strcmp(a, "-d") || !strcmp(a, "--debug")) {
      if (i + 1 < argc)
        moas_debug |= parse_debug_list(argv[++i]);
      else
        moas_debug |= MOAS_DBG_ALL;
      continue;
    }
    if (!strncmp(a, "--proc=", 7)) {
      moas_start_proc = atoi(a + 7);
      continue;
    }
    if (!strncmp(a, "--addr=", 7)) {
      moas_start_addr = atoi(a + 7);
      continue;
    }
    if (!strncmp(a, "--datadir=", 10)) {
#ifndef DOS
      setenv("MOAS_DATADIR", a + 10, 1);
#endif
      continue;
    }
    if (!strcmp(a, "--fullscreen") || !strcmp(a, "-f")) {
      moas_fullscreen = 1;
      continue;
    }
    if (!strncmp(a, "--statedir=", 11)) {
#ifndef DOS
      setenv("MOAS_STATEDIR", a + 11, 1);
#endif
      continue;
    }
    if (!strcmp(a, "--statedir")) {
      if (i + 1 >= argc) {
        fprintf(stderr, "moas: --statedir needs a path\n");
        exit(1);
      }
#ifndef DOS
      setenv("MOAS_STATEDIR", argv[++i], 1);
#endif
      continue;
    }
    if (!strncmp(a, "--load=", 7) || !strcmp(a, "--load")) {
      const char *arg;
      if (!strncmp(a, "--load=", 7))
        arg = a + 7;
      else {
        if (i + 1 >= argc) {
          fprintf(stderr, "moas: --load needs a slot (0-9) or file path\n");
          exit(1);
        }
        arg = argv[++i];
      }
      {
        /* Pure digits 0-9 → slot; otherwise path (or GAME n relative to state). */
        const char *p = arg;
        int digits = 1;
        if (!*p) digits = 0;
        while (*p) {
          if (*p < '0' || *p > '9') { digits = 0; break; }
          p++;
        }
        if (digits && arg[0] && !arg[1] && arg[0] >= '0' && arg[0] <= '9') {
          moas_load_slot = arg[0] - '0';
          moas_load_path[0] = 0;
        } else if (digits && strlen(arg) <= 2) {
          int slot = atoi(arg);
          if (slot < 0 || slot > 9) {
            fprintf(stderr, "moas: --load slot must be 0..9 (got %s)\n", arg);
            exit(1);
          }
          moas_load_slot = slot;
          moas_load_path[0] = 0;
        } else {
          moas_load_slot = -1;
          strncpy(moas_load_path, arg, sizeof(moas_load_path) - 1);
          moas_load_path[sizeof(moas_load_path) - 1] = 0;
        }
      }
      moas_skip_intro = 1; /* save already has the game state */
      continue;
    }
    if (!strcmp(a, "--no-autosave")) {
      moas_no_autosave = 1;
      continue;
    }
    fprintf(stderr, "moas: unknown option '%s' (try --help)\n", a);
    exit(1);
  }

  if (moas_debug) {
#ifdef __ANDROID__
    SDL_Log("moas: debug flags=0x%x skip_intro=%d proc=%d addr=%d",
            moas_debug, moas_skip_intro, moas_start_proc, moas_start_addr);
#else
    fprintf(stderr, "moas: debug flags=0x%x skip_intro=%d proc=%d addr=%d\n",
            moas_debug, moas_skip_intro, moas_start_proc, moas_start_addr);
#endif
  }
}

void MoasDbg(unsigned flag, const char *fmt, ...)
{
  va_list ap;
  if (!(moas_debug & flag)) return;
  va_start(ap, fmt);
#ifdef __ANDROID__
  {
    char buf[512];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    SDL_Log("[dbg] %s", buf);
  }
#else
  fprintf(stderr, "moas: ");
  vfprintf(stderr, fmt, ap);
  fputc('\n', stderr);
#endif
  va_end(ap);
}
