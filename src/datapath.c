/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "datapath.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>

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
    if (ntried < 8 && (p) && (p)[0]) { \
      strncpy(tried[ntried], (p), PATH_MAX - 1); \
      tried[ntried][PATH_MAX - 1] = 0; \
      ntried++; \
    } \
  } while (0)

  env = getenv("MOAS_DATADIR");
  if (env) {
    REMEMBER(env);
    if (looks_like_data_dir(env)) {
      set_data_dir(env);
      fprintf(stderr, "moas: data directory (env MOAS_DATADIR): %s\n", data_dir);
      return;
    }
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

  return -1;
}

void MoasReportMissingData(const char *name)
{
  fprintf(stderr,
          "moas: cannot open data file '%s'\n"
          "  data directory: %s\n"
          "  cwd:            ",
          name ? name : "(null)", data_dir);
  {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)))
      fprintf(stderr, "%s\n", cwd);
    else
      fprintf(stderr, "(unknown)\n");
  }
  fprintf(stderr,
          "  Hint: install places data in share/moas next to the binary,\n"
          "        or set MOAS_DATADIR to the manick/ folder.\n");
}
