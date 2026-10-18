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

static int is_dir(const char *path)
{
  struct stat st;
  return path && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int file_readable(const char *path)
{
  return path && access(path, R_OK) == 0;
}

/* True if dir contains a recognizable game data file. */
static int looks_like_data_dir(const char *dir)
{
  char buf[PATH_MAX];
  static const char *markers[] = {
    "GAME.PCK", "game.pck", "GAME.1", "game.1",
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
  /* strip trailing slashes */
  {
    size_t n = strlen(data_dir);
    while (n > 1 && data_dir[n - 1] == '/') {
      data_dir[--n] = 0;
    }
  }
}

/* dirname of path into out (may be "."). */
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

  env = getenv("MOAS_DATADIR");
  if (env && looks_like_data_dir(env)) {
    set_data_dir(env);
    return;
  }

  if (MOAS_DATADIR[0] && looks_like_data_dir(MOAS_DATADIR)) {
    set_data_dir(MOAS_DATADIR);
    return;
  }

  if (looks_like_data_dir("./manick")) {
    set_data_dir("./manick");
    return;
  }
  if (looks_like_data_dir("manick")) {
    set_data_dir("manick");
    return;
  }
  if (looks_like_data_dir(".")) {
    set_data_dir(".");
    return;
  }

  dirname_of(argv0, exe_dir, sizeof(exe_dir));

  snprintf(buf, sizeof(buf), "%s/../share/moas", exe_dir);
  if (looks_like_data_dir(buf)) {
    set_data_dir(buf);
    return;
  }
  snprintf(buf, sizeof(buf), "%s/manick", exe_dir);
  if (looks_like_data_dir(buf)) {
    set_data_dir(buf);
    return;
  }
  snprintf(buf, sizeof(buf), "%s/../manick", exe_dir);
  if (looks_like_data_dir(buf)) {
    set_data_dir(buf);
    return;
  }

  /* Fall back to cwd; opens will still try relative names. */
  set_data_dir(".");
}

const char *MoasDataDir(void)
{
  return data_dir;
}

static int try_open(const char *path)
{
  int fd = open(path, O_RDONLY);
  return fd;
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

  /* 1. as given in cwd */
  fd = try_open(name);
  if (fd >= 0) return fd;

  /* 2. case variants in cwd */
  fold_case_copy(folded, name, sizeof(folded), 1);
  fd = try_open(folded);
  if (fd >= 0) return fd;
  fold_case_copy(folded, name, sizeof(folded), 0);
  fd = try_open(folded);
  if (fd >= 0) return fd;

  /* 3. under data dir */
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
