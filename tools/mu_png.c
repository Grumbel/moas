/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "mu_png.h"
#include "mu_common.h"
#include "pngread.h"
#include "pngwrite.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void vga_to_rgb(const uint8_t *vga, uint8_t *rgb)
{
  int i;
  for (i = 0; i < 768; i++) {
    uint8_t v = vga[i] & 63;
    rgb[i] = (uint8_t)((v << 2) | (v >> 4));
  }
}

void rgb_to_vga(const uint8_t *rgb, uint8_t *vga)
{
  int i;
  for (i = 0; i < 768; i++) vga[i] = rgb[i] >> 2;
}

int png_write(const char *path, const uint8_t *pix, int w, int h,
              const uint8_t *pal, const uint8_t *alpha)
{
  if (png_write_indexed(path, pix, w, h, pal, alpha) != 0) {
    fprintf(stderr, "moas-util: cannot write %s: %s\n", path, strerror(errno));
    return -1;
  }
  return 0;
}

const char *png_read(const char *path, int *w, int *h, uint8_t **pix,
                     uint8_t *pal, int *npal)
{
  long len;
  uint8_t *f = read_file(path, &len);
  const char *err;
  if (!f) return "cannot open file";
  err = png_read_indexed(f, len, w, h, pix, pal, npal);
  free(f);
  return err;
}
