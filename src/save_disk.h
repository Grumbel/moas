/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef MOAS_SAVE_DISK_H
#define MOAS_SAVE_DISK_H

#include <stdint.h>

/*
 * Portable on-disk pieces for GAME*.SAV on the Linux port.
 * Not byte-identical to DOS saves (enum/pointer sizes differ); consistent
 * for Linux↔Linux. Person[] uses PersonDisk (same as GAME.1).
 */

#pragma pack(push, 1)

typedef struct {
  int16_t x1, y1, x2, y2;
  uint16_t Ref;
  int16_t MarkTyp; /* Label=0, Jumper=1, Unused=2 */
} MarkerDisk;

typedef struct {
  int16_t Num;
  int16_t Typ; /* Normal=0, InfoTyp1=1, InfoTyp2=2 */
} PicOnScreenDisk;

typedef struct {
  uint16_t TimeLeft;
  uint16_t Proc, Adresse;
} OntimeDisk;

typedef struct {
  uint16_t TextAdd;
  int16_t RefNum;
  int8_t Typ, StdTyp;
  int16_t Querverweis;
} DialogInhaltDisk;

typedef struct {
  int16_t ReferenzNummer;
  uint16_t TextProc;
  DialogInhaltDisk Inhalt[30];
} DialogDisk;

typedef struct {
  int16_t PrsNum;
  uint8_t TxtColor;
  int8_t StdFont;
} SayPropDisk;

/* SayWindow without pointers; Data/Text follow when flags set. */
typedef struct {
  int16_t RefPrs;
  int16_t x, y, xs, ys, x1, x2;
  int8_t Font;
  int16_t Time;
  int16_t wx, wy;
  uint8_t hasData;
  uint8_t hasText;
} SayWindowDisk;

#pragma pack(pop)

#endif
