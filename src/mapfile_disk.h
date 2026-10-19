/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef MOAS_MAPFILE_DISK_H
#define MOAS_MAPFILE_DISK_H

#include <stdint.h>

/*
 * On-disk MAPFILE record (GAME.2). Must be exactly 30 bytes — original
 * Watcom builds used -zp1 / pack(1). Wrong size skews every Phase/Bilder
 * catalog entry and corrupts sprites (including the mouse cursor).
 */
#if defined(__WATCOMC__)
#  pragma pack(1)
#else
#  pragma pack(push, 1)
#endif
typedef struct {
  int8_t   FileNum;
  int32_t  FilePos;
  int32_t  Length;
  uint32_t Data;     /* original pointer, discarded on load */
  int32_t  x;
  int32_t  y;
  int32_t  Anz;
  int8_t   InUse;
  uint32_t Check;
} MapFileDisk;
#if defined(__WATCOMC__)
#  pragma pack()
#else
#  pragma pack(pop)
#endif

/* Compile-time size check (works on C89 compilers without _Static_assert). */
typedef char moas_mapfile_disk_size_ok[(sizeof(MapFileDisk) == 30) ? 1 : -1];

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
_Static_assert(sizeof(MapFileDisk) == 30, "MapFileDisk must be 30 bytes");
#endif

#endif /* MOAS_MAPFILE_DISK_H */
