/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-2.0-or-later */
/* unpack / pack: game data <-> editable source tree. */
#ifndef MU_DATA_H
#define MU_DATA_H

/* Data files in datadir -> source tree in outdir. */
void cmd_unpack(const char *datadir, const char *outdir);
/* Source tree in srcdir -> data files in outdir. reference (or NULL): a
 * data directory whose packed graphics are reused where the pixels are
 * unchanged, which makes an unedited tree pack to identical files. */
void cmd_pack(const char *srcdir, const char *outdir, const char *reference);
/* How many graphics in datadir the encoder reproduces byte for byte. */
void cmd_gfx_test(const char *datadir);

#endif
