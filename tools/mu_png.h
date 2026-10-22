/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Minimal 8-bit indexed PNG writer and reader (no zlib dependency). */
#ifndef MU_PNG_H
#define MU_PNG_H

#include <stdint.h>

/* pal: 768 bytes 8-bit RGB. alpha: 256 entries or NULL (opaque). */
int png_write(const char *path, const uint8_t *pix, int w, int h,
              const uint8_t *pal, const uint8_t *alpha);

/* Reads a non-interlaced indexed PNG (bit depth 1/2/4/8). *pix is malloc'ed,
 * one byte per pixel. pal gets 768 bytes (missing entries zero), *npal the
 * PLTE entry count. Returns NULL on success, else an error message. */
const char *png_read(const char *path, int *w, int *h, uint8_t **pix,
                     uint8_t *pal, int *npal);

/* VGA 6-bit palette <-> 8-bit RGB; lossless for values 0..63. */
void vga_to_rgb(const uint8_t *vga, uint8_t *rgb);
void rgb_to_vga(const uint8_t *rgb, uint8_t *vga);

#endif
