/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef MOAS_LANGGFX_H
#define MOAS_LANGGFX_H

/*
 * Per-language picture overrides: lang/<code>/<kind>/<kind>_NNN.png, 8-bit
 * indexed, in the layout moas-util unpack writes. The pixel values are the
 * game's palette indices and replace the pixels decoded from GAME.N; the
 * PNG's own palette is ignored (a room keeps its palette from the data).
 * Index 0 is transparent in phases and inventory pictures, as in the data.
 */

enum { LANGGFX_BILD, LANGGFX_INV, LANGGFX_PHASE, LANGGFX_KINDS };

/* Copies the current language's override for kind/num into dst (w x h
 * pixels, `stride` bytes per row) if there is one of that size. 1 if done. */
int LangGfxApply(int kind, int num, unsigned char *dst, int w, int h, int stride);

/* After a language change: 1 if kind/num must be decoded again, because its
 * last load used an override or the current language has one. */
int LangGfxChanged(int kind, int num);

#endif
