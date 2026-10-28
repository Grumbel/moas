/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef MOAS_PNGWRITE_H
#define MOAS_PNGWRITE_H

#include <stdint.h>

/* Writes an 8-bit indexed PNG (no zlib dependency), shared by the game
 * (screenshots) and moas-util. pal: 768 bytes 8-bit RGB. alpha: 256
 * entries or NULL (opaque). Returns 0, or -1 with errno set. */
int png_write_indexed(const char *path, const uint8_t *pix, int w, int h,
                      const uint8_t *pal, const uint8_t *alpha);

#endif
