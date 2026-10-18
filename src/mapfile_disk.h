/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef MOAS_MAPFILE_DISK_H
#define MOAS_MAPFILE_DISK_H

#include <stdint.h>

/* On-disk layout of MAPFILE as written by the original 16/32-bit tools.
 * Packed to 30 bytes; matches GAME.2 records. */
#pragma pack(push, 1)
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
#pragma pack(pop)

_Static_assert(sizeof(MapFileDisk) == 30, "MapFileDisk must be 30 bytes");

#endif
