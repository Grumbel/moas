/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef MOAS_PERSON_DISK_H
#define MOAS_PERSON_DISK_H

#include <stdint.h>

/*
 * On-disk PERSON as written by the original 32-bit tools (GAME.1).
 * Record size 236 bytes; file stride is 240 (236 + 4-byte checksum).
 */
#pragma pack(push, 1)
typedef struct {
  int16_t  Num;
  int16_t  PhaseNum;
  int32_t  xs, ys, Anz;
  uint32_t Phase;           /* original pointer, discarded */
  int16_t  Sequence[8][12];
  int8_t   Counter, Dir, Boden, GhostLevel;
  int16_t  x, y, z;
  int16_t  TargetX, TargetY;
  int16_t  gtx, gty;
  int8_t   iWalk;
  int8_t   BaseSpeed, StepSpeed, PhaseSpeed;
  uint16_t Bits;
} PersonDisk;
#pragma pack(pop)

_Static_assert(sizeof(PersonDisk) == 236, "PersonDisk must be 236 bytes");

#define PERSON_DISK_STRIDE 240  /* 236 + 4-byte CHKSUM */

#endif
