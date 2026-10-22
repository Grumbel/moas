/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Graphics stream codec (engine: UniversalUnpack in src/grafix.c). */
#ifndef MU_GFX_H
#define MU_GFX_H

#include <stdint.h>

/* First byte of every packed graphic resource; the engine skips it. */
#define GFX_HEADER 4

/* Decode a pixel stream (after the header byte) into dst. Unlike the
 * engine, the glass colour is written like any other, so the result is
 * the exact pixel data. xs: row stride for the 248..250 ops. Returns the
 * number of pixels written, or -1 if the stream is corrupt or overflows. */
long gfx_decode(const uint8_t *src, long size, uint8_t *dst, long cap, long xs);

/* Encode pixels into a stream that gfx_decode turns back into them,
 * including the leading GFX_HEADER byte. *out is malloc'ed. Returns NULL on
 * success, else an error message. */
const char *gfx_encode(const uint8_t *pix, long n, uint8_t **out, long *outlen);

#endif
