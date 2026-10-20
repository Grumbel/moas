/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "datapath.h"

#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <errno.h>
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#if defined(_WIN32) || defined(__WATCOMC__)
#include <direct.h>
#endif
#ifdef __ANDROID__
#include <SDL.h>
#endif

static void data_log(const char *fmt, ...)
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

#ifndef MOAS_DATADIR
#define MOAS_DATADIR ""
#endif

static char data_dir[PATH_MAX] = ".";
static int data_dir_resolved = 0;

static int is_dir(const char *path)
{
  struct stat st;
  return path && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int file_readable(const char *path)
{
  return path && access(path, R_OK) == 0;
}

static int looks_like_data_dir(const char *dir)
{
  char buf[PATH_MAX];
  static const char *markers[] = {
    "GAME.PCK", "game.pck", "GAME.1", "game.1",
    "GAME.2", "game.2", "GAME.XXX", "game.xxx",
    "LICENCE.DAT", "Licence.Dat", "licence.dat",
    NULL
  };
  int i;
  if (!is_dir(dir)) return 0;
  for (i = 0; markers[i]; i++) {
    int n = snprintf(buf, sizeof(buf), "%s/%s", dir, markers[i]);
    if (n > 0 && (size_t)n < sizeof(buf) && file_readable(buf))
      return 1;
  }
  return 0;
}

static void set_data_dir(const char *dir)
{
  if (!dir || !*dir) {
    strncpy(data_dir, ".", sizeof(data_dir) - 1);
    data_dir[sizeof(data_dir) - 1] = 0;
    return;
  }
  strncpy(data_dir, dir, sizeof(data_dir) - 1);
  data_dir[sizeof(data_dir) - 1] = 0;
  {
    size_t n = strlen(data_dir);
    while (n > 1 && data_dir[n - 1] == '/')
      data_dir[--n] = 0;
  }
  data_dir_resolved = 1;
  data_log("moas: using data directory: %s", data_dir);
}

static void dirname_of(const char *path, char *out, size_t outsz)
{
  const char *slash;
  size_t len;
  if (!path || !*path) {
    strncpy(out, ".", outsz - 1);
    out[outsz - 1] = 0;
    return;
  }
  slash = strrchr(path, '/');
  if (!slash) {
    strncpy(out, ".", outsz - 1);
    out[outsz - 1] = 0;
    return;
  }
  if (slash == path) {
    strncpy(out, "/", outsz - 1);
    out[outsz - 1] = 0;
    return;
  }
  len = (size_t)(slash - path);
  if (len >= outsz) len = outsz - 1;
  memcpy(out, path, len);
  out[len] = 0;
}

void MoasInitDataPath(const char *argv0)
{

  const char *env;
  char buf[PATH_MAX];
  char exe_dir[PATH_MAX];
  char tried[8][PATH_MAX];
  int ntried = 0;

#define REMEMBER(p) do { \
    if (ntried < 8) { \
      const char *_rp = (p); \
      if (_rp != NULL && _rp[0] != '\0') { \
        snprintf(tried[ntried], PATH_MAX, "%s", _rp); \
        ntried++; \
      } \
    } \
  } while (0)

  env = getenv("MOAS_DATADIR");
  if (env && env[0]) {
    REMEMBER(env);
    if (looks_like_data_dir(env)) {
      set_data_dir(env);
      data_log("moas: data directory (env MOAS_DATADIR): %s", data_dir);
      return;
    }
    /* Trust an explicit path even if markers are not found yet (e.g. first
     * extract race); open() will still fail clearly per file. */
    set_data_dir(env);
    data_log("moas: MOAS_DATADIR set but markers missing, trying anyway: %s", data_dir);
    return;
  }

  if (MOAS_DATADIR[0]) {
    REMEMBER(MOAS_DATADIR);
    if (looks_like_data_dir(MOAS_DATADIR)) {
      set_data_dir(MOAS_DATADIR);
      fprintf(stderr, "moas: data directory (built-in): %s\n", data_dir);
      return;
    }
  }

  REMEMBER("./manick");
  if (looks_like_data_dir("./manick")) {
    set_data_dir("./manick");
    fprintf(stderr, "moas: data directory (./manick): %s\n", data_dir);
    return;
  }
  REMEMBER("manick");
  if (looks_like_data_dir("manick")) {
    set_data_dir("manick");
    fprintf(stderr, "moas: data directory (manick): %s\n", data_dir);
    return;
  }
  REMEMBER(".");
  if (looks_like_data_dir(".")) {
    set_data_dir(".");
    fprintf(stderr, "moas: data directory (cwd): %s\n", data_dir);
    return;
  }

  dirname_of(argv0, exe_dir, sizeof(exe_dir));

  snprintf(buf, sizeof(buf), "%s/../share/moas", exe_dir);
  REMEMBER(buf);
  if (looks_like_data_dir(buf)) {
    set_data_dir(buf);
    fprintf(stderr, "moas: data directory ($exe/../share/moas): %s\n", data_dir);
    return;
  }
  snprintf(buf, sizeof(buf), "%s/manick", exe_dir);
  REMEMBER(buf);
  if (looks_like_data_dir(buf)) {
    set_data_dir(buf);
    fprintf(stderr, "moas: data directory ($exe/manick): %s\n", data_dir);
    return;
  }
  snprintf(buf, sizeof(buf), "%s/../manick", exe_dir);
  REMEMBER(buf);
  if (looks_like_data_dir(buf)) {
    set_data_dir(buf);
    fprintf(stderr, "moas: data directory ($exe/../manick): %s\n", data_dir);
    return;
  }

  set_data_dir(".");
  fprintf(stderr,
          "moas: WARNING: could not find game data directory.\n"
          "  Expected files like GAME.1, GAME.PCK, LICENCE.DAT.\n"
          "  Searched:\n");
  {
    int i;
    for (i = 0; i < ntried; i++)
      fprintf(stderr, "    - %s%s\n", tried[i],
              is_dir(tried[i]) ? " (exists, but no game files)" : " (missing)");
  }
  fprintf(stderr,
          "  Fix: set MOAS_DATADIR to the directory that contains GAME.*\n"
          "       e.g. export MOAS_DATADIR=/path/to/manick\n");
#undef REMEMBER
}

const char *MoasDataDir(void)
{
  return data_dir;
}

static char state_dir[PATH_MAX];
static char config_dir[PATH_MAX];
static int state_dir_ready;
static int config_dir_ready;

static int moas_mkdir_p(const char *path)
{
  char tmp[PATH_MAX];
  size_t len;
  size_t i;

  if (!path || !path[0])
    return -1;
  len = strlen(path);
  if (len >= sizeof(tmp))
    return -1;
  memcpy(tmp, path, len + 1);

  for (i = 1; i < len; i++) {
#if defined(_WIN32) || defined(DOS)
    if (tmp[i] == '\\' || tmp[i] == '/') {
#else
    if (tmp[i] == '/') {
#endif
      char save = tmp[i];
      tmp[i] = 0;
      if (tmp[0] && tmp[strlen(tmp)-1] != ':') {
#if defined(_WIN32) || defined(__WATCOMC__)
        /* Win32 / Open Watcom: path only (no mode). */
        (void)_mkdir(tmp);
#else
        mkdir(tmp, 0755);
#endif
      }
      tmp[i] = save;
    }
  }
#if defined(_WIN32) || defined(__WATCOMC__)
  if (_mkdir(tmp) != 0) {
#else
  if (mkdir(tmp, 0755) != 0) {
#endif
    struct stat st;
    if (stat(tmp, &st) != 0 || (st.st_mode & S_IFDIR) == 0)
      return -1;
  }
  return 0;
}

static void resolve_state_dir(void)
{
  const char *env;

  if (state_dir_ready)
    return;

#if defined(DOS)
  strncpy(state_dir, ".", sizeof(state_dir) - 1);
  state_dir[sizeof(state_dir) - 1] = 0;
#elif defined(__EMSCRIPTEN__)
  strncpy(state_dir, "/moas-state", sizeof(state_dir) - 1);
  state_dir[sizeof(state_dir) - 1] = 0;
  moas_mkdir_p(state_dir);
#elif defined(__ANDROID__)
  {
    const char *base = getenv("MOAS_STATEDIR");
    if (base && base[0]) {
      /* Explicit state dir (set by PlatformAndroidPrepareData). */
      strncpy(state_dir, base, sizeof(state_dir) - 1);
      state_dir[sizeof(state_dir) - 1] = 0;
    } else {
      /* Ensure data_dir is resolved before using it as the parent. */
      (void)MoasDataDir();
      base = data_dir[0] ? data_dir : ".";
      snprintf(state_dir, sizeof(state_dir), "%s/moas-state", base);
    }
    if (moas_mkdir_p(state_dir) != 0)
      data_log("moas: WARNING: could not create Android state dir '%s'", state_dir);
    data_log("moas: Android state dir: %s", state_dir);
  }
#elif defined(_WIN32)
  {
    const char *la = getenv("LOCALAPPDATA");
    if (la && la[0])
      snprintf(state_dir, sizeof(state_dir), "%s\\moas\\state", la);
    else
      snprintf(state_dir, sizeof(state_dir), "moas-state");
    moas_mkdir_p(state_dir);
  }
#else
  /* XDG Base Directory: state home for savegames */
  env = getenv("MOAS_STATEDIR");
  if (env && env[0]) {
    strncpy(state_dir, env, sizeof(state_dir) - 1);
    state_dir[sizeof(state_dir) - 1] = 0;
  } else {
    env = getenv("XDG_STATE_HOME");
    if (env && env[0])
      snprintf(state_dir, sizeof(state_dir), "%s/moas", env);
    else {
      const char *home = getenv("HOME");
      if (home && home[0])
        snprintf(state_dir, sizeof(state_dir), "%s/.local/state/moas", home);
      else
        snprintf(state_dir, sizeof(state_dir), "moas-state");
    }
  }
  if (moas_mkdir_p(state_dir) != 0)
    data_log("moas: WARNING: could not create state dir '%s'", state_dir);
#endif
  state_dir_ready = 1;
}

static void resolve_config_dir(void)
{
  const char *env;

  if (config_dir_ready)
    return;

#if defined(DOS)
  strncpy(config_dir, ".", sizeof(config_dir) - 1);
  config_dir[sizeof(config_dir) - 1] = 0;
#elif defined(__EMSCRIPTEN__)
  strncpy(config_dir, "/moas-config", sizeof(config_dir) - 1);
  config_dir[sizeof(config_dir) - 1] = 0;
  moas_mkdir_p(config_dir);
#elif defined(__ANDROID__)
  {
    const char *base = getenv("MOAS_CONFIGDIR");
    if (!base || !base[0])
      base = data_dir[0] ? data_dir : ".";
    snprintf(config_dir, sizeof(config_dir), "%s/moas-config", base);
    moas_mkdir_p(config_dir);
  }
#elif defined(_WIN32)
  {
    const char *ad = getenv("APPDATA");
    if (ad && ad[0])
      snprintf(config_dir, sizeof(config_dir), "%s\\moas", ad);
    else
      snprintf(config_dir, sizeof(config_dir), "moas-config");
    moas_mkdir_p(config_dir);
  }
#else
  env = getenv("MOAS_CONFIGDIR");
  if (env && env[0]) {
    strncpy(config_dir, env, sizeof(config_dir) - 1);
    config_dir[sizeof(config_dir) - 1] = 0;
  } else {
    env = getenv("XDG_CONFIG_HOME");
    if (env && env[0])
      snprintf(config_dir, sizeof(config_dir), "%s/moas", env);
    else {
      const char *home = getenv("HOME");
      if (home && home[0])
        snprintf(config_dir, sizeof(config_dir), "%s/.config/moas", home);
      else
        snprintf(config_dir, sizeof(config_dir), "moas-config");
    }
  }
  if (moas_mkdir_p(config_dir) != 0)
    data_log("moas: WARNING: could not create config dir '%s'", config_dir);
#endif
  config_dir_ready = 1;
}

const char *MoasStateDir(void)
{
  resolve_state_dir();
  return state_dir;
}

const char *MoasConfigDir(void)
{
  resolve_config_dir();
  return config_dir;
}

void MoasLogDirectories(void)
{
  const char *data = MoasDataDir();
  const char *state = MoasStateDir();
  const char *config = MoasConfigDir();

  data_log("moas: directories:");
  data_log("  data   (GAME.*):  %s", data ? data : "(null)");
  data_log("  state  (saves):   %s", state ? state : "(null)");
  data_log("  config (settings): %s", config ? config : "(null)");
}


int MoasStatePath(char *buf, size_t buflen, const char *name)
{
  const char *dir;
  int n;

  if (!buf || buflen < 2 || !name || !name[0])
    return -1;
  dir = MoasStateDir();
#if defined(_WIN32)
  n = snprintf(buf, buflen, "%s\\%s", dir, name);
#else
  n = snprintf(buf, buflen, "%s/%s", dir, name);
#endif
  if (n < 0 || (size_t)n >= buflen)
    return -1;
  return 0;
}

int MoasOpenState(const char *name, int flags, int mode)
{
  char path[PATH_MAX];
  int fd;

  if (MoasStatePath(path, sizeof(path), name) != 0)
    return -1;
#if defined(DOS) || defined(_WIN32)
  fd = open(path, flags | O_BINARY, mode);
#else
  (void)mode;
  if (flags & O_CREAT)
    fd = open(path, flags, mode ? mode : 0666);
  else
    fd = open(path, flags);
#endif
  if (fd < 0)
    data_log("moas: MoasOpenState('%s') failed path=%s", name, path);
  return fd;
}


static int try_open(const char *path)
{
  return open(path, O_RDONLY);
}

static void fold_case_copy(char *dst, const char *src, size_t dstsz, int upper)
{
  size_t i;
  for (i = 0; src[i] && i + 1 < dstsz; i++) {
    unsigned char c = (unsigned char)src[i];
    dst[i] = (char)(upper ? toupper(c) : tolower(c));
  }
  dst[i] = 0;
}

int MoasOpenData(const char *name)
{
  char path[PATH_MAX];
  char folded[PATH_MAX];
  int fd;

  if (!name || !*name) return -1;

  fd = try_open(name);
  if (fd >= 0) return fd;

  fold_case_copy(folded, name, sizeof(folded), 1);
  fd = try_open(folded);
  if (fd >= 0) return fd;
  fold_case_copy(folded, name, sizeof(folded), 0);
  fd = try_open(folded);
  if (fd >= 0) return fd;

  if (data_dir[0] && strcmp(data_dir, ".") != 0) {
    snprintf(path, sizeof(path), "%s/%s", data_dir, name);
    fd = try_open(path);
    if (fd >= 0) return fd;

    fold_case_copy(folded, name, sizeof(folded), 1);
    snprintf(path, sizeof(path), "%s/%s", data_dir, folded);
    fd = try_open(path);
    if (fd >= 0) return fd;

    fold_case_copy(folded, name, sizeof(folded), 0);
    snprintf(path, sizeof(path), "%s/%s", data_dir, folded);
    fd = try_open(path);
    if (fd >= 0) return fd;
  }

  data_log("moas: MoasOpenData failed for '%s' (dir=%s)", name, data_dir);
  return -1;
}

void MoasReportMissingData(const char *name)
{
  char cwd[PATH_MAX];
  data_log("moas: cannot open data file '%s'", name ? name : "(null)");
  data_log("  data directory: %s", data_dir);
  if (getcwd(cwd, sizeof(cwd)))
    data_log("  cwd: %s", cwd);
  else
    data_log("  cwd: (unknown)");
  data_log("  Hint: set MOAS_DATADIR or install game data (Android: APK assets)");
}
