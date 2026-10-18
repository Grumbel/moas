/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef MOAS_DATAPATH_H
#define MOAS_DATAPATH_H

#ifdef __cplusplus
extern "C" {
#endif

/* Resolve the game data directory.
 * Order:
 *   1. MOAS_DATADIR environment variable
 *   2. Compile-time MOAS_DATADIR (install prefix)
 *   3. ./manick relative to cwd
 *   4. dirname(argv0)/../share/moas
 *   5. dirname(argv0)/manick
 *   6. cwd (.)
 * A directory is accepted if it contains GAME.PCK or GAME.1 (any case).
 */
void MoasInitDataPath(const char *argv0);

/* Currently selected data directory (never NULL after init; may be "."). */
const char *MoasDataDir(void);

/* Open a data file for reading: try name in cwd, then under data dir,
 * with simple case folding. Returns fd or -1. */
int MoasOpenData(const char *name);

#ifdef __cplusplus
}
#endif

#endif
