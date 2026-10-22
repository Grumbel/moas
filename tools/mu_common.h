/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Shared helpers for moas-util. */
#ifndef MU_COMMON_H
#define MU_COMMON_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#if defined(__GNUC__)
#  define MU_PRINTF(a, b) __attribute__((format(printf, a, b)))
#  define MU_NORETURN __attribute__((noreturn))
#else
#  define MU_PRINTF(a, b)
#  define MU_NORETURN
#endif

#pragma pack(push, 1)
typedef struct {
  int8_t   FileNum;
  int32_t  FilePos;
  int32_t  Length;
  uint32_t Data;
  int32_t  x;
  int32_t  y;
  int32_t  Anz;
  int8_t   InUse;
  uint32_t Check;
} MapFileDisk;
#pragma pack(pop)

_Static_assert(sizeof(MapFileDisk) == 30, "MapFileDisk");

enum { K_PROG, K_BILD, K_INV, K_PHASE, K_COUNT };
extern const char *const kind_names[K_COUNT];

typedef struct {
  int16_t n[K_COUNT];
  MapFileDisk *e[K_COUNT];
} Catalog;

extern const char *g_datadir;

void die(const char *fmt, ...) MU_PRINTF(1, 2) MU_NORETURN;
void die_errno(const char *what) MU_NORETURN;
void *xmalloc(size_t n);
void *xrealloc(void *p, size_t n);
char *xstrdup(const char *s);
int path_join(char *out, size_t outsz, const char *dir, const char *name);
int mkdir_p(const char *path);

/* Open NAME in DIR, retrying upper/lower case. -1 if missing. */
int open_in(const char *dir, const char *name);
int open_data(const char *name);
/* Whole file, or NULL if it can't be opened. */
uint8_t *read_file(const char *path, long *len);
uint8_t *read_in(const char *dir, const char *name, long *len);
void write_file(const char *path, const void *data, long len);

/* Engine checksum (chk.c: MkChk). */
uint32_t moas_chk(const uint8_t *p, long n);

Catalog catalog_parse(const uint8_t *g2, long len);
Catalog load_catalog_in(const char *dir);
Catalog load_catalog(void);
void free_catalog(Catalog *c);

/* Pixel size of a graphic resource. 0 if the kind has no pixels. */
int gfx_width(int kind, const MapFileDisk *e);
int gfx_height(int kind, const MapFileDisk *e);
/* Payload bytes of a graphic, without a room's trailing palette. */
long gfx_payload(int kind, const MapFileDisk *e);
int bild_has_palette(const MapFileDisk *e);

/* CP850 text, as used by all game strings. Bytes without a safe
 * printable form come out as \xNN. */
void put_cp850(FILE *fp, const uint8_t *s, size_t n);
/* Parse a "..." literal at *pp (UTF-8 + \" \\ \xNN), CP850 bytes into
 * out (NUL-terminated). Returns length, or -1 with *err set. */
int parse_cp850_string(const char **pp, uint8_t *out, size_t outsz, const char **err);

#endif
