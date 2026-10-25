/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "langgfx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "lang.h"
#include "pngread.h"
#include "log.h"

static const char *const kind_names[LANGGFX_KINDS] = { "bild", "inv", "phase" };

/* applied[kind][num]: the last load of num used an override */
static unsigned char *applied[LANGGFX_KINDS];
static int applied_n[LANGGFX_KINDS];

static void set_applied(int kind, int num, int on)
{
  if (num >= applied_n[kind]) {
    int n = num + 64;
    unsigned char *p;
    if (!on)
      return;
    p = realloc(applied[kind], (size_t)n);
    if (!p)
      return;
    memset(p + applied_n[kind], 0, (size_t)(n - applied_n[kind]));
    applied[kind] = p;
    applied_n[kind] = n;
  }
  applied[kind][num] = (unsigned char)on;
}

static int find_override(int kind, int num, char *path, size_t pathsz)
{
  char rel[128];
  snprintf(rel, sizeof(rel), "%s/%s/%s_%03d.png", LangGetCode(), kind_names[kind],
           kind_names[kind], num);
  return LangFindFile(path, pathsz, rel);
}

static unsigned char *read_whole(const char *path, long *len)
{
  FILE *f = fopen(path, "rb");
  unsigned char *buf;
  long n;
  if (!f)
    return NULL;
  if (fseek(f, 0, SEEK_END) != 0 || (n = ftell(f)) <= 0 || fseek(f, 0, SEEK_SET) != 0) {
    fclose(f);
    return NULL;
  }
  buf = malloc((size_t)n);
  if (buf && fread(buf, 1, (size_t)n, f) != (size_t)n) {
    free(buf);
    buf = NULL;
  }
  fclose(f);
  *len = n;
  return buf;
}

int LangGfxApply(int kind, int num, unsigned char *dst, int w, int h, int stride)
{
  char path[1024];
  unsigned char *file, *pix;
  unsigned char pal[768];
  const char *err;
  long len;
  int pw, ph, npal, y;

  if (kind < 0 || kind >= LANGGFX_KINDS || num < 0)
    return 0;
  set_applied(kind, num, 0);
  if (find_override(kind, num, path, sizeof(path)) != 0)
    return 0;
  file = read_whole(path, &len);
  if (!file) {
    MoasLog("moas: cannot read %s\n", path);
    return 0;
  }
  err = png_read_indexed(file, len, &pw, &ph, &pix, pal, &npal);
  free(file);
  if (err) {
    MoasLog("moas: %s: %s\n", path, err);
    return 0;
  }
  if (pw != w || ph != h) {
    MoasLog("moas: %s: is %dx%d, the game's picture is %dx%d; ignored\n",
            path, pw, ph, w, h);
    free(pix);
    return 0;
  }
  for (y = 0; y < h; y++)
    memcpy(dst + (size_t)y * stride, pix + (size_t)y * w, (size_t)w);
  free(pix);
  set_applied(kind, num, 1);
  MoasDbg(MOAS_DBG_LOAD, "language picture %s", path);
  return 1;
}

int LangGfxChanged(int kind, int num)
{
  char path[1024];
  if (kind < 0 || kind >= LANGGFX_KINDS || num < 0)
    return 0;
  if (num < applied_n[kind] && applied[kind][num])
    return 1;
  return find_override(kind, num, path, sizeof(path)) == 0;
}
