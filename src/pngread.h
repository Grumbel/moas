/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef MOAS_PNGREAD_H
#define MOAS_PNGREAD_H

#include <stdint.h>

/* Decodes a non-interlaced indexed PNG (bit depth 1/2/4/8) from memory.
 * *pix is malloc'ed, one byte per pixel. pal gets 768 bytes (missing entries
 * zero), *npal the PLTE entry count. Returns NULL on success, else an error
 * message. */
const char *png_read_indexed(const uint8_t *data, long len, int *w, int *h, uint8_t **pix,
                             uint8_t *pal, int *npal);

#endif
