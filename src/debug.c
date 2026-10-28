/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "debug.h"
#include "lang.h"
#include "compat.h"
#include "version.h"

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
int moas_fullscreen = -1;
int moas_no_autosave = 0;
int moas_touch_controls = -1;
int moas_aspect = -1;
int moas_lang_explicit = 0;
int moas_master = 0;

int MoasParseAspect(const char *s)
{
  if (!strcmp(s, "4:3"))
    return 1;
  if (!strcmp(s, "8:5") || !strcmp(s, "16:10"))
    return 0;
  return -1;
}

static void usage(const char *argv0)
{
  fprintf(stderr,
    "Usage: %s [options]\n"
    "\n"
    "Options:\n"
    "  -h, --help              Show this help\n"
    "  --version               Show the version and exit\n"
    "  -d, --debug LIST        Debug flags (script,input,wait,window,load,all);\n"
    "                          also starts in master mode (like THPRIV + Alt+T)\n"
    "  --skip-intro            Skip intro/title script\n"
    "  --proc N                Start program N after intro\n"
    "  --addr N                Start address in that program\n"
    "  --datadir PATH          Game data directory (MOAS_DATADIR)\n"
    "  --statedir PATH         Savegame directory (MOAS_STATEDIR)\n"
    "  --load SLOT|FILE        Load save at startup (slot 0-9 or path to .SAV)\n"
    "  --lang CODE             Language: de, en, es, fr, it, nl (loads lang/CODE.po;\n"
    "                          default: settings.ini, else the OS language, else en)\n"
    "  --no-autosave           Do not restore/write AUTOSAVE.SAV on start/exit\n"
    "  -f, --fullscreen        Desktop fullscreen\n"
    "  --windowed              Start in a window (overrides settings.ini)\n"
    "  --aspect RATIO          4:3 (default, like the original CRT) or 8:5\n"
    "                          (square pixels)\n"
    "  --touch-controls        Show on-screen Save/Load/Use/Look/Options/Hint/Skip buttons\n"
    "  --no-touch-controls     Hide them (toggle with F12)\n"
    "\n"
    "Options taking a value accept both --opt VALUE and --opt=VALUE.\n"
    "\n"
    "Examples:\n"
    "  %s --load=0\n"
    "  %s --load=~/.local/state/manick/GAME3.SAV\n"
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

/*
 * Match option NAME as "NAME=VALUE" or "NAME VALUE" (consumes argv[*i+1]).
 * Returns the value, or NULL if argv[*i] is a different option.
 */
static const char *opt_value(int argc, char **argv, int *i, const char *name)
{
  const char *a = argv[*i];
  size_t n = strlen(name);

  if (strncmp(a, name, n) != 0)
    return NULL;
  if (a[n] == '=')
    return a + n + 1;
  if (a[n] != 0)
    return NULL;
  if (*i + 1 >= argc) {
    fprintf(stderr, "moas: %s needs a value (try --help)\n", name);
    exit(1);
  }
  return argv[++*i];
}

void MoasParseArgs(int argc, char **argv)
{
  int i;
  const char *env;

  env = getenv("MOAS_DEBUG");
  if (env && *env)
    moas_debug |= parse_debug_list(env);
  env = getenv("MOAS_LANG");
  if (env && *env) {
    LangSetCode(env);
    moas_lang_explicit = 1;
  }
#ifdef __ANDROID__
  /* Always surface load diagnostics on logcat until the port is solid. */
  if (!env || !*env)
    moas_debug |= MOAS_DBG_LOAD;
#endif

  for (i = 1; i < argc; i++) {
    const char *a = argv[i];
    const char *v;
    if (!strcmp(a, "-h") || !strcmp(a, "--help")) {
      usage(argv[0] ? argv[0] : "manick");
      exit(0);
    }
    if (!strcmp(a, "--version")) {
      printf("moas %s\n", MOAS_VERSION);
      exit(0);
    }
    if (!strcmp(a, "--skip-intro")) {
      moas_skip_intro = 1;
      continue;
    }
    if ((v = opt_value(argc, argv, &i, "--lang"))) {
      LangSetCode(v);
      moas_lang_explicit = 1;
      continue;
    }
    /* Bare -d / --debug at the end or before another option: all flags. */
    if (!strcmp(a, "-d") || !strcmp(a, "--debug")) {
      if (i + 1 < argc && argv[i + 1][0] != '-')
        moas_debug |= parse_debug_list(argv[++i]);
      else
        moas_debug |= MOAS_DBG_ALL;
      moas_master = 1;
      continue;
    }
    if ((v = opt_value(argc, argv, &i, "--debug"))) {
      moas_debug |= parse_debug_list(v);
      moas_master = 1;
      continue;
    }
    if ((v = opt_value(argc, argv, &i, "--proc"))) {
      moas_start_proc = atoi(v);
      continue;
    }
    if ((v = opt_value(argc, argv, &i, "--addr"))) {
      moas_start_addr = atoi(v);
      continue;
    }
    if ((v = opt_value(argc, argv, &i, "--datadir"))) {
#ifndef DOS
      setenv("MOAS_DATADIR", v, 1);
#endif
      continue;
    }
    if (!strcmp(a, "--fullscreen") || !strcmp(a, "-f")) {
      moas_fullscreen = 1;
      continue;
    }
    if (!strcmp(a, "--windowed")) {
      moas_fullscreen = 0;
      continue;
    }
    if ((v = opt_value(argc, argv, &i, "--aspect"))) {
      moas_aspect = MoasParseAspect(v);
      if (moas_aspect < 0) {
        fprintf(stderr, "moas: --aspect must be 4:3 or 8:5 (got %s)\n", v);
        exit(1);
      }
      continue;
    }
    if ((v = opt_value(argc, argv, &i, "--statedir"))) {
#ifndef DOS
      setenv("MOAS_STATEDIR", v, 1);
#endif
      continue;
    }
    if ((v = opt_value(argc, argv, &i, "--load"))) {
      const char *arg = v;
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
    if (!strcmp(a, "--touch-controls")) {
      moas_touch_controls = 1;
      continue;
    }
    if (!strcmp(a, "--no-touch-controls")) {
      moas_touch_controls = 0;
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
