/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Source tree written by unpack and read by pack:
 *
 *   moas.txt          GAME.2 catalog: one line per resource, file layout
 *   pck.txt           GAME.PCK text dictionary
 *   persons.txt       GAME.1 person templates
 *   scripts/prog_NNN.mas   script programs (GAME.3)
 *   bild/ inv/ phase/      graphics (GAME.4) as 8-bit indexed PNG
 *   GAME.XXX LICENCE.DAT   fonts and licence text, copied as they are
 */
#include "mu_data.h"
#include "mu_common.h"
#include "mu_gfx.h"
#include "mu_png.h"
#include "mu_script.h"

#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FILES 10

typedef struct {
  const char *dir;
  uint8_t *data[MAX_FILES];
  long len[MAX_FILES];
  int tried[MAX_FILES];
} DataFiles;

static const uint8_t *data_file(DataFiles *f, int num, long *len)
{
  if (num < 1 || num >= MAX_FILES) return NULL;
  if (!f->tried[num]) {
    char name[16];
    snprintf(name, sizeof(name), "GAME.%d", num);
    f->data[num] = read_in(f->dir, name, &f->len[num]);
    f->tried[num] = 1;
  }
  *len = f->len[num];
  return f->data[num];
}

static const uint8_t *blob(DataFiles *f, const MapFileDisk *e)
{
  long len;
  const uint8_t *d = data_file(f, e->FileNum, &len);
  if (!d || e->Length <= 0 || e->FilePos < 0 || (long)e->FilePos + e->Length > len) return NULL;
  return d + e->FilePos;
}

static void free_files(DataFiles *f)
{
  int i;
  for (i = 0; i < MAX_FILES; i++) free(f->data[i]);
  memset(f, 0, sizeof(*f));
}

static void out_path(char *buf, size_t sz, const char *dir, const char *fmt, ...) MU_PRINTF(4, 5);

static void out_path(char *buf, size_t sz, const char *dir, const char *fmt, ...)
{
  char name[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(name, sizeof(name), fmt, ap);
  va_end(ap);
  if (!path_join(buf, sz, dir, name)) die("path too long");
}

static FILE *open_out(const char *path)
{
  FILE *fp = fopen(path, "w");
  if (!fp) die("cannot write %s: %s", path, strerror(errno));
  return fp;
}

static void close_out(FILE *fp, const char *path)
{
  if (fclose(fp) != 0) die("short write %s", path);
}

static char *read_text(const char *path)
{
  long len;
  char *t = (char *)read_file(path, &len);   /* read_file NUL-terminates */
  if (!t) die("cannot read %s", path);
  return t;
}

/* ---- persons (GAME.1) ------------------------------------------------ */

/* PersonDisk (src/person_disk.h): 236 bytes + 4-byte checksum each. */
#define PERSON_SIZE 236
#define PERSON_STRIDE 240

typedef struct {
  const char *key;
  int count, size, is_signed, off;
} PersonField;

static const PersonField person_fields[] = {
  { "num",       1, 2, 1, 0 },
  { "phase",     1, 2, 1, 2 },
  { "size",      3, 4, 1, 4 },      /* xs ys anz */
  { "phase_ptr", 1, 4, 0, 16 },     /* original pointer, unused */
  { "seq0",     12, 2, 1, 20 },
  { "seq1",     12, 2, 1, 44 },
  { "seq2",     12, 2, 1, 68 },
  { "seq3",     12, 2, 1, 92 },
  { "seq4",     12, 2, 1, 116 },
  { "seq5",     12, 2, 1, 140 },
  { "seq6",     12, 2, 1, 164 },
  { "seq7",     12, 2, 1, 188 },
  { "counter",   1, 1, 1, 212 },
  { "dir",       1, 1, 1, 213 },
  { "boden",     1, 1, 1, 214 },
  { "ghost",     1, 1, 1, 215 },
  { "pos",       3, 2, 1, 216 },    /* x y z */
  { "target",    2, 2, 1, 222 },
  { "gt",        2, 2, 1, 226 },
  { "iwalk",     1, 1, 1, 230 },
  { "speed",     3, 1, 1, 231 },    /* base step phase */
  { "bits",      1, 2, 0, 234 },
};
#define N_PERSON_FIELDS (int)(sizeof(person_fields) / sizeof(person_fields[0]))

static long long get_le(const uint8_t *p, int size, int is_signed)
{
  uint32_t v = 0;
  int i;
  for (i = 0; i < size; i++) v |= (uint32_t)p[i] << (8 * i);
  if (!is_signed) return (long long)v;
  if (size == 1) return (int8_t)v;
  if (size == 2) return (int16_t)v;
  return (int32_t)v;
}

static void put_le(uint8_t *p, int size, long long v)
{
  int i;
  for (i = 0; i < size; i++) p[i] = (uint8_t)((unsigned long long)v >> (8 * i));
}

static void write_persons(const char *path, const uint8_t *g1, long len)
{
  FILE *fp;
  int n, i, f, j;
  if (len < 2) die("GAME.1 too short");
  n = (int16_t)(g1[0] | (g1[1] << 8));
  if (n < 0 || 2 + (long)n * PERSON_STRIDE != len) die("GAME.1: unexpected size");
  fp = open_out(path);
  fputs("; GAME.1: person templates, used by add_person/set_person. Fields as in\n"
        "; src/person_disk.h; size is xs ys anz, pos is x y z, speed is base step\n"
        "; phase. Checksums are recomputed by moas-util pack.\n", fp);
  for (i = 0; i < n; i++) {
    const uint8_t *r = g1 + 2 + (long)i * PERSON_STRIDE;
    if (moas_chk(r, PERSON_SIZE) != (uint32_t)get_le(r + PERSON_SIZE, 4, 0))
      die("GAME.1: person %d checksum mismatch", i);
    fprintf(fp, "\nperson %d\n", i);
    for (f = 0; f < N_PERSON_FIELDS; f++) {
      const PersonField *pf = &person_fields[f];
      fprintf(fp, "  %-9s", pf->key);
      for (j = 0; j < pf->count; j++) {
        long long v = get_le(r + pf->off + j * pf->size, pf->size, pf->is_signed);
        if (pf->is_signed) fprintf(fp, " %lld", v);
        else fprintf(fp, " 0x%0*llx", pf->size * 2, (unsigned long long)v);
      }
      fputc('\n', fp);
    }
  }
  close_out(fp, path);
}

static uint8_t *parse_persons(const char *path, long *outlen)
{
  char *text = read_text(path), *p = text;
  uint8_t *buf = NULL;
  int n = -1, line = 1;
  int seen[N_PERSON_FIELDS];
  long cap = 0;

  for (; *p; line++) {
    char *eol = strchr(p, '\n'), *c, *q = p;
    if (eol) *eol = 0;
    if ((c = strchr(q, ';'))) *c = 0;
    while (*q == ' ' || *q == '\t') q++;
    if (*q && *q != '\r') {
      char key[32];
      int klen = 0, f;
      while (q[klen] && q[klen] != ' ' && q[klen] != '\t' && q[klen] != '\r' && klen < 31) klen++;
      memcpy(key, q, (size_t)klen);
      key[klen] = 0;
      q += klen;
      if (!strcmp(key, "person")) {
        long idx = strtol(q, &c, 10);
        if (n >= 0)
          for (f = 0; f < N_PERSON_FIELDS; f++)
            if (!seen[f]) die("%s:%d: person %d has no %s", path, line, n, person_fields[f].key);
        if (idx != n + 1) die("%s:%d: expected person %d", path, line, n + 1);
        n++;
        if (2 + (long)(n + 1) * PERSON_STRIDE > cap) {
          cap = 2 + (long)(n + 64) * PERSON_STRIDE;
          buf = xrealloc(buf, (size_t)cap);
        }
        memset(buf + 2 + (long)n * PERSON_STRIDE, 0, PERSON_STRIDE);
        memset(seen, 0, sizeof(seen));
      } else {
        const PersonField *pf = NULL;
        int j;
        for (f = 0; f < N_PERSON_FIELDS; f++)
          if (!strcmp(person_fields[f].key, key)) pf = &person_fields[f];
        if (!pf) die("%s:%d: unknown field '%s'", path, line, key);
        if (n < 0) die("%s:%d: field before the first 'person' line", path, line);
        f = (int)(pf - person_fields);
        if (seen[f]) die("%s:%d: %s given twice", path, line, key);
        seen[f] = 1;
        for (j = 0; j < pf->count; j++) {
          long long v = strtoll(q, &c, 0);
          long long lo = pf->is_signed ? -(1LL << (pf->size * 8 - 1)) : 0;
          long long hi = pf->is_signed ? (1LL << (pf->size * 8 - 1)) - 1 : (1LL << (pf->size * 8)) - 1;
          if (c == q) die("%s:%d: %s needs %d values", path, line, key, pf->count);
          if (v < lo || v > hi) die("%s:%d: %s value %lld out of range", path, line, key, v);
          put_le(buf + 2 + (long)n * PERSON_STRIDE + pf->off + j * pf->size, pf->size, v);
          q = c;
        }
        while (*q == ' ' || *q == '\t' || *q == '\r') q++;
        if (*q) die("%s:%d: junk after %s", path, line, key);
      }
    }
    if (!eol) break;
    p = eol + 1;
  }
  if (n < 0) die("%s: no persons", path);
  {
    int f, i;
    for (f = 0; f < N_PERSON_FIELDS; f++)
      if (!seen[f]) die("%s: person %d has no %s", path, n, person_fields[f].key);
    n++;
    for (i = 0; i < n; i++) {
      uint8_t *r = buf + 2 + (long)i * PERSON_STRIDE;
      put_le(r + PERSON_SIZE, 4, moas_chk(r, PERSON_SIZE));
    }
  }
  put_le(buf, 2, n);
  free(text);
  *outlen = 2 + (long)n * PERSON_STRIDE;
  return buf;
}

/* ---- palettes for the PNGs ------------------------------------------- */

enum { ROLE_NONE, ROLE_ROOM, ROLE_INFO1, ROLE_INFO2 };

/* Walk map (Info2): bit 7 = walkable, bits 0-6 = floor value. Walkable is
 * bright, blocked dark; each floor value gets its own hue. */
static void walk_palette(uint8_t *pal)
{
  int i;
  for (i = 0; i < 256; i++) {
    int floor = i & 127, walk = i >> 7;
    double h = floor == 0 ? 120.0 : (double)((floor * 67) % 360), s = floor == 0 ? 0.45 : 0.75;
    double v = walk ? 0.95 : 0.30, c = v * s, x, r = 0, g = 0, b = 0;
    x = c * (1 - (((int)(h / 60) % 2 == 0) ? 1 - (h / 60 - (int)(h / 60)) : (h / 60 - (int)(h / 60))));
    switch ((int)(h / 60) % 6) {
      case 0: r = c; g = x; break;
      case 1: r = x; g = c; break;
      case 2: g = c; b = x; break;
      case 3: g = x; b = c; break;
      case 4: r = x; b = c; break;
      default: r = c; b = x; break;
    }
    if (floor == 0 && !walk) r = g = b = c = 0.12, v = 0.12;
    pal[i * 3 + 0] = (uint8_t)((r + v - c) * 255);
    pal[i * 3 + 1] = (uint8_t)((g + v - c) * 255);
    pal[i * 3 + 2] = (uint8_t)((b + v - c) * 255);
  }
}

/* Depth map (Info1): grey ramp; a person whose foot row is at most the
 * value is drawn behind the pixel. 200 and up cover everyone (red). */
static void depth_palette(uint8_t *pal)
{
  int i;
  for (i = 0; i < 256; i++) {
    int v = i < 200 ? i * 255 / 199 : 255;
    pal[i * 3] = pal[i * 3 + 1] = pal[i * 3 + 2] = (uint8_t)v;
    if (i >= 200) pal[i * 3 + 1] = pal[i * 3 + 2] = 0;
  }
}

/* ---- unpack ---------------------------------------------------------- */

static int load_progs(DataFiles *f, const Catalog *c, Prog *progs)
{
  int i;
  for (i = 0; i < c->n[K_PROG]; i++) {
    const MapFileDisk *e = &c->e[K_PROG][i];
    const uint8_t *b = blob(f, e);
    if (!b) die("cannot read prog %d", i);
    progs[i].code = (uint8_t *)b;
    progs[i].len = e->Length;
  }
  return c->n[K_PROG];
}

/* load_bild operands give the role of each bild. */
static void find_roles(const Prog *progs, int nprog, int nbild, int *role, int *room_of)
{
  int i, pc;
  for (i = 0; i < nbild; i++) { role[i] = ROLE_NONE; room_of[i] = -1; }
  for (i = 0; i < nprog; i++) {
    int sz;
    for (pc = 0; (sz = script_insn_size(progs[i].code, progs[i].len, pc)) > 0; pc += sz) {
      const uint8_t *q = progs[i].code + pc;
      int room, k;
      if (q[0] != 42) continue;
      room = (int16_t)(q[1] | (q[2] << 8));
      for (k = 0; k < 3; k++) {
        int b = (int16_t)(q[1 + 2 * k] | (q[2 + 2 * k] << 8));
        if (b > 0 && b < nbild) {
          role[b] = k == 0 ? ROLE_ROOM : k == 1 ? ROLE_INFO1 : ROLE_INFO2;
          if (k) room_of[b] = room;
        }
      }
    }
  }
}

/* Each GAME.N file must hold its resources back to back, a kind at a
 * time in index order: that is what the "layout" lines describe. */
static void write_layouts(FILE *fp, DataFiles *f, const Catalog *c)
{
  int num;
  for (num = 1; num < MAX_FILES; num++) {
    long pos = 0, flen = 0;
    int k, i, nkinds = 0, kinds[K_COUNT], any = 0;
    for (k = 0; k < K_COUNT; k++)
      for (i = 0; i < c->n[k]; i++) any |= c->e[k][i].FileNum == num;
    if (!any) continue;
    if (!data_file(f, num, &flen)) die("GAME.%d is missing", num);
    /* walk the file, following resources by position */
    while (pos < flen) {
      int found = 0;
      for (k = 0; k < K_COUNT && !found; k++)
        for (i = 0; i < c->n[k] && !found; i++)
          if (c->e[k][i].FileNum == num && c->e[k][i].FilePos == pos && c->e[k][i].Length > 0) {
            int j;
            if (nkinds && kinds[nkinds - 1] == k) die("GAME.%d: unsupported layout (%s out of order)", num, kind_names[k]);
            for (j = 0; j < nkinds; j++)
              if (kinds[j] == k) die("GAME.%d: unsupported layout (%s split up)", num, kind_names[k]);
            kinds[nkinds++] = k;
            for (; i < c->n[k] && c->e[k][i].FileNum == num; i++) {
              if (c->e[k][i].FilePos != pos) die("GAME.%d: unsupported layout at %s %d", num, kind_names[k], i);
              pos += c->e[k][i].Length;
            }
            found = 1;
          }
      if (!found) die("GAME.%d: bytes at %ld belong to no resource", num, pos);
    }
    fprintf(fp, "layout %d", num);
    for (k = 0; k < nkinds; k++) fprintf(fp, " %s", kind_names[kinds[k]]);
    fputc('\n', fp);
  }
}

void cmd_unpack(const char *datadir, const char *outdir)
{
  DataFiles files;
  Catalog c;
  Dict dict;
  Prog *progs;
  const char *err;
  char path[1024];
  uint8_t *buf, room_pal[768], first_pal[768], pal[768], alpha[256];
  long len;
  int i, k, nprog, *role, *room_of, have_room = 0, count[K_COUNT] = { 0 };
  FILE *fp;

  memset(&files, 0, sizeof(files));
  files.dir = datadir;
  c = load_catalog_in(datadir);
  if (mkdir_p(outdir) != 0) die_errno(outdir);
  for (k = 0; k < K_COUNT; k++) {
    out_path(path, sizeof(path), outdir, "%s", k == K_PROG ? "scripts" : kind_names[k]);
    if (mkdir_p(path) != 0) die_errno(path);
  }

  /* dictionary */
  buf = read_in(datadir, "GAME.PCK", &len);
  if (!buf) die("cannot read GAME.PCK");
  if ((err = dict_from_pck(&dict, buf, len))) die("%s", err);
  free(buf);
  out_path(path, sizeof(path), outdir, "pck.txt");
  fp = open_out(path);
  dict_write_text(fp, &dict);
  close_out(fp, path);

  /* scripts */
  progs = xmalloc(sizeof(Prog) * (size_t)(c.n[K_PROG] + 1));
  nprog = load_progs(&files, &c, progs);
  for (i = 0; i < nprog; i++) {
    out_path(path, sizeof(path), outdir, "scripts/prog_%03d.mas", i);
    fp = open_out(path);
    script_disasm(fp, i, progs, nprog, &dict);
    close_out(fp, path);
  }
  count[K_PROG] = nprog;

  /* graphics */
  role = xmalloc(sizeof(int) * (size_t)(c.n[K_BILD] + 1));
  room_of = xmalloc(sizeof(int) * (size_t)(c.n[K_BILD] + 1));
  find_roles(progs, nprog, c.n[K_BILD], role, room_of);
  memset(first_pal, 0, sizeof(first_pal));
  for (i = 0; i < c.n[K_BILD]; i++)
    if (bild_has_palette(&c.e[K_BILD][i]) && blob(&files, &c.e[K_BILD][i])) {
      vga_to_rgb(blob(&files, &c.e[K_BILD][i]) + c.e[K_BILD][i].Length - 768, first_pal);
      break;
    }
  memcpy(room_pal, first_pal, sizeof(room_pal));
  for (k = K_BILD; k < K_COUNT; k++)
    for (i = 0; i < c.n[k]; i++) {
      const MapFileDisk *e = &c.e[k][i];
      const uint8_t *b = blob(&files, e);
      int w = gfx_width(k, e), h = gfx_height(k, e);
      long n = (long)w * h;
      uint8_t *pix;
      const uint8_t *alpha_p = NULL;
      if (!b || w <= 0 || h <= 0) die("%s %d: unreadable", kind_names[k], i);
      if (b[0] != GFX_HEADER) die("%s %d: unexpected header byte %d", kind_names[k], i, b[0]);
      pix = xmalloc((size_t)n);
      if (gfx_decode(b + 1, gfx_payload(k, e) - 1, pix, n, w) != n)
        die("%s %d: corrupt pixel stream", kind_names[k], i);
      memset(alpha, 255, sizeof(alpha));
      if (k == K_BILD && bild_has_palette(e)) {
        vga_to_rgb(b + e->Length - 768, pal);
        memcpy(room_pal, pal, sizeof(pal));
        have_room = 1;
      } else if (k == K_BILD && role[i] == ROLE_INFO2) {
        walk_palette(pal);
      } else if (k == K_BILD && role[i] == ROLE_INFO1) {
        depth_palette(pal);
      } else if (k == K_BILD) {
        /* bild 0 (box, 255 transparent) and anything else */
        memcpy(pal, have_room ? room_pal : first_pal, sizeof(pal));
        if (i == 0) { alpha[255] = 0; alpha_p = alpha; }
      } else {
        /* sprites: 254 is the glass colour, 0 shows through too */
        memcpy(pal, first_pal, sizeof(pal));
        alpha[0] = alpha[254] = 0;
        alpha_p = alpha;
      }
      out_path(path, sizeof(path), outdir, "%s/%s_%03d.png", kind_names[k], kind_names[k], i);
      if (png_write(path, pix, w, h, pal, alpha_p) != 0) die("cannot write %s", path);
      free(pix);
      count[k]++;
    }

  /* catalog */
  out_path(path, sizeof(path), outdir, "moas.txt");
  fp = open_out(path);
  fputs("; GAME.2 resource catalog, written by moas-util unpack.\n"
        ";\n"
        "; layout N KINDS: GAME.N holds these kinds, in this order, each in index\n"
        ";   order. Positions and lengths are computed by moas-util pack.\n"
        "; KIND INDEX: catalog entry. file is the GAME.N file, x/y/anz the size\n"
        ";   (bild: x is the width, +16384 if a palette follows the pixels;\n"
        ";   phase: anz frames of x*y stacked vertically; inv: x is the place,\n"
        ";   pictures are 32x24). check is the stored checksum; pack recomputes it\n"
        ";   for anything it encodes. data and inuse are unused by the engine.\n"
        ";   Graphics checksums in the original data don't match their bytes;\n"
        ";   the engine ignores them.\n\n", fp);
  write_layouts(fp, &files, &c);
  fputc('\n', fp);
  for (k = 0; k < K_COUNT; k++)
    for (i = 0; i < c.n[k]; i++) {
      const MapFileDisk *e = &c.e[k][i];
      fprintf(fp, "%s %d file=%d x=%d y=%d anz=%d inuse=%d data=0x%08x check=0x%08x",
              kind_names[k], i, e->FileNum, (int)e->x, (int)e->y, (int)e->Anz,
              e->InUse, (unsigned)e->Data, (unsigned)e->Check);
      if (k == K_BILD && role[i] == ROLE_INFO1) fprintf(fp, "  ; depth map of bild %d", room_of[i]);
      if (k == K_BILD && role[i] == ROLE_INFO2) fprintf(fp, "  ; walk map of bild %d", room_of[i]);
      fputc('\n', fp);
    }
  close_out(fp, path);

  /* GAME.2 trailer must be its checksum; nothing else to keep */
  buf = read_in(datadir, "GAME.2", &len);
  if (!buf || len < 4 || moas_chk(buf, len - 4) != (uint32_t)get_le(buf + len - 4, 4, 0))
    die("GAME.2: trailing checksum mismatch");
  if (len != 8 + 30L * (c.n[0] + c.n[1] + c.n[2] + c.n[3]) + 4) die("GAME.2: unexpected size");
  free(buf);

  /* persons */
  buf = read_in(datadir, "GAME.1", &len);
  if (!buf) die("cannot read GAME.1");
  out_path(path, sizeof(path), outdir, "persons.txt");
  write_persons(path, buf, len);
  free(buf);

  /* copied as they are */
  {
    static const char *const raw[] = { "GAME.XXX", "LICENCE.DAT" };
    for (i = 0; i < 2; i++) {
      buf = read_in(datadir, raw[i], &len);
      if (!buf) die("cannot read %s", raw[i]);
      out_path(path, sizeof(path), outdir, "%s", raw[i]);
      write_file(path, buf, len);
      free(buf);
    }
  }

  printf("unpacked %s -> %s: %d scripts, %d bild, %d inv, %d phase\n", datadir, outdir,
         count[K_PROG], count[K_BILD], count[K_INV], count[K_PHASE]);
  free(role);
  free(room_of);
  free(progs);
  dict_free(&dict);
  free_files(&files);
  free_catalog(&c);
}

/* ---- pack ------------------------------------------------------------ */

typedef struct {
  int present;
  MapFileDisk e;
  uint8_t *data;   /* packed resource */
} Entry;

typedef struct {
  int num, nkinds, kinds[K_COUNT];
} Layout;

static int kind_of(const char *s, size_t n)
{
  int k;
  for (k = 0; k < K_COUNT; k++)
    if (strlen(kind_names[k]) == n && !strncmp(kind_names[k], s, n)) return k;
  return -1;
}

static long field(const char *path, int line, const char *s, const char *key, int hex)
{
  char pat[32];
  const char *p;
  char *end;
  long v;
  snprintf(pat, sizeof(pat), " %s=", key);
  p = strstr(s, pat);
  if (!p) die("%s:%d: missing %s=", path, line, key);
  p += strlen(pat);
  v = hex ? (long)strtoul(p, &end, 0) : strtol(p, &end, 0);
  if (end == p) die("%s:%d: bad %s=", path, line, key);
  return v;
}

static void parse_manifest(const char *path, Entry **entries, int *count, Layout *layouts, int *nlayouts)
{
  char *text = read_text(path), *p = text;
  int line = 1, k, cap[K_COUNT] = { 0 };
  for (k = 0; k < K_COUNT; k++) { entries[k] = NULL; count[k] = 0; }
  *nlayouts = 0;
  for (; *p; line++) {
    char *eol = strchr(p, '\n'), *c, *q = p;
    if (eol) *eol = 0;
    if ((c = strchr(q, ';'))) *c = 0;
    while (*q == ' ' || *q == '\t') q++;
    if (!strncmp(q, "layout", 6) && (q[6] == ' ' || q[6] == '\t')) {
      Layout *l = &layouts[(*nlayouts)++];
      if (*nlayouts > MAX_FILES) die("%s:%d: too many layout lines", path, line);
      l->num = (int)strtol(q + 6, &c, 10);
      l->nkinds = 0;
      if (l->num < 1 || l->num >= MAX_FILES) die("%s:%d: bad file number", path, line);
      for (q = c;;) {
        size_t n;
        while (*q == ' ' || *q == '\t' || *q == '\r') q++;
        if (!*q) break;
        n = strcspn(q, " \t\r");
        if ((k = kind_of(q, n)) < 0 || l->nkinds >= K_COUNT) die("%s:%d: bad kind", path, line);
        l->kinds[l->nkinds++] = k;
        q += n;
      }
    } else if (*q && *q != '\r') {
      size_t n = strcspn(q, " \t");
      long idx;
      Entry *e;
      if ((k = kind_of(q, n)) < 0) die("%s:%d: unknown line", path, line);
      idx = strtol(q + n, &c, 10);
      if (c == q + n || idx < 0 || idx > 32767) die("%s:%d: bad index", path, line);
      if (idx >= cap[k]) {
        int old = cap[k];
        cap[k] = (int)idx + 64;
        entries[k] = xrealloc(entries[k], sizeof(Entry) * (size_t)cap[k]);
        memset(entries[k] + old, 0, sizeof(Entry) * (size_t)(cap[k] - old));
      }
      e = &entries[k][idx];
      if (e->present) die("%s:%d: %s %ld listed twice", path, line, kind_names[k], idx);
      e->present = 1;
      e->e.FileNum = (int8_t)field(path, line, q, "file", 0);
      e->e.x = (int32_t)field(path, line, q, "x", 0);
      e->e.y = (int32_t)field(path, line, q, "y", 0);
      e->e.Anz = (int32_t)field(path, line, q, "anz", 0);
      e->e.InUse = (int8_t)field(path, line, q, "inuse", 0);
      e->e.Data = (uint32_t)field(path, line, q, "data", 1);
      e->e.Check = (uint32_t)field(path, line, q, "check", 1);
      if (idx >= count[k]) count[k] = (int)idx + 1;
    }
    if (!eol) break;
    p = eol + 1;
  }
  for (k = 0; k < K_COUNT; k++) {
    int i;
    for (i = 0; i < count[k]; i++)
      if (!entries[k][i].present) die("%s: %s %d is missing", path, kind_names[k], i);
  }
  free(text);
}

/* Original packed bytes of the same resource, if its pixels match. */
static const uint8_t *reference_match(DataFiles *ref, const Catalog *rc, int k, int i,
                                      const uint8_t *pix, int w, int h, const uint8_t *vga_pal,
                                      long *len, uint32_t *check)
{
  const MapFileDisk *e;
  const uint8_t *b;
  uint8_t *tmp;
  long n = (long)w * h;
  int ok;
  if (!ref || i >= rc->n[k]) return NULL;
  e = &rc->e[k][i];
  if (gfx_width(k, e) != w || gfx_height(k, e) != h || !!vga_pal != (k == K_BILD && bild_has_palette(e)))
    return NULL;
  if (!(b = blob(ref, e)) || b[0] != GFX_HEADER) return NULL;
  tmp = xmalloc((size_t)n);
  ok = gfx_decode(b + 1, gfx_payload(k, e) - 1, tmp, n, w) == n && !memcmp(tmp, pix, (size_t)n) &&
       (!vga_pal || !memcmp(b + e->Length - 768, vga_pal, 768));
  free(tmp);
  if (!ok) return NULL;
  *len = e->Length;
  *check = e->Check;
  return b;
}

void cmd_pack(const char *srcdir, const char *outdir, const char *reference)
{
  Entry *entries[K_COUNT];
  int count[K_COUNT], nlayouts, i, k, l, errors, reused = 0, encoded = 0;
  Layout layouts[MAX_FILES];
  DataFiles ref;
  Catalog rc;
  Dict dict;
  char path[1024], *text;
  uint8_t *buf;
  long len;
  int line;
  const char *err;

  memset(&ref, 0, sizeof(ref));
  memset(&rc, 0, sizeof(rc));
  if (reference) {
    ref.dir = reference;
    rc = load_catalog_in(reference);
  }
  out_path(path, sizeof(path), srcdir, "moas.txt");
  parse_manifest(path, entries, count, layouts, &nlayouts);
  if (mkdir_p(outdir) != 0) die_errno(outdir);

  /* dictionary */
  out_path(path, sizeof(path), srcdir, "pck.txt");
  text = read_text(path);
  if ((err = dict_parse_text(&dict, text, &line))) die("%s:%d: %s", path, line, err);
  free(text);
  buf = dict_to_pck(&dict, &len);
  out_path(path, sizeof(path), outdir, "GAME.PCK");
  write_file(path, buf, len);
  free(buf);

  /* scripts */
  {
    int n = count[K_PROG];
    char **texts = xmalloc(sizeof(char *) * (size_t)(n + 1));
    char **names = xmalloc(sizeof(char *) * (size_t)(n + 1));
    Prog *progs = xmalloc(sizeof(Prog) * (size_t)(n + 1));
    for (i = 0; i < n; i++) {
      out_path(path, sizeof(path), srcdir, "scripts/prog_%03d.mas", i);
      texts[i] = read_text(path);
      names[i] = xstrdup(path);
    }
    errors = script_assemble(texts, (const char **)names, n, &dict, progs);
    if (errors) die("%d error(s) in scripts", errors);
    for (i = 0; i < n; i++) {
      Entry *e = &entries[K_PROG][i];
      e->data = progs[i].code;
      e->e.Length = progs[i].len;
      e->e.Check = moas_chk(progs[i].code, progs[i].len);
      free(texts[i]);
      free(names[i]);
    }
    free(texts);
    free(names);
    free(progs);
  }

  /* graphics */
  for (k = K_BILD; k < K_COUNT; k++)
    for (i = 0; i < count[k]; i++) {
      Entry *e = &entries[k][i];
      int w, h, ew = gfx_width(k, &e->e), eh = gfx_height(k, &e->e), npal, has_pal;
      uint8_t *pix, rgb[768], vga[768];
      const uint8_t *orig;
      uint32_t check;
      out_path(path, sizeof(path), srcdir, "%s/%s_%03d.png", kind_names[k], kind_names[k], i);
      if ((err = png_read(path, &w, &h, &pix, rgb, &npal))) die("%s: %s", path, err);
      if (w != ew || h != eh)
        die("%s: is %dx%d, moas.txt says %dx%d (change x/y/anz there to resize)", path, w, h, ew, eh);
      has_pal = k == K_BILD && (e->e.x & 16384);
      if (has_pal) {
        if (npal < 256) fprintf(stderr, "moas-util: warning: %s has only %d palette entries\n", path, npal);
        rgb_to_vga(rgb, vga);
      }
      orig = reference_match(reference ? &ref : NULL, &rc, k, i, pix, w, h, has_pal ? vga : NULL, &len, &check);
      if (orig) {
        e->data = xmalloc((size_t)len);
        memcpy(e->data, orig, (size_t)len);
        e->e.Length = len;
        e->e.Check = check;
        reused++;
      } else {
        uint8_t *packed;
        if ((err = gfx_encode(pix, (long)w * h, &packed, &len))) die("%s: %s", path, err);
        if (has_pal) {
          packed = xrealloc(packed, (size_t)len + 768);
          memcpy(packed + len, vga, 768);
          len += 768;
        }
        e->data = packed;
        e->e.Length = len;
        e->e.Check = moas_chk(packed, len);
        encoded++;
      }
      free(pix);
    }

  /* GAME.N files, positions */
  for (l = 0; l < nlayouts; l++) {
    const Layout *lay = &layouts[l];
    uint8_t *out = NULL;
    long pos = 0;
    int j;
    for (j = 0; j < lay->nkinds; j++) {
      k = lay->kinds[j];
      for (i = 0; i < count[k]; i++) {
        Entry *e = &entries[k][i];
        if (e->e.FileNum != lay->num) continue;
        e->e.FilePos = pos;
        out = xrealloc(out, (size_t)(pos + e->e.Length));
        memcpy(out + pos, e->data, (size_t)e->e.Length);
        pos += e->e.Length;
      }
    }
    snprintf(path, sizeof(path), "GAME.%d", lay->num);
    {
      char full[1024];
      out_path(full, sizeof(full), outdir, "%s", path);
      write_file(full, out, pos);
    }
    free(out);
  }
  for (k = 0; k < K_COUNT; k++)
    for (i = 0; i < count[k]; i++) {
      int found = 0;
      for (l = 0; l < nlayouts; l++)
        for (int j = 0; j < layouts[l].nkinds; j++)
          found |= layouts[l].num == entries[k][i].e.FileNum && layouts[l].kinds[j] == k;
      if (!found) die("%s %d: GAME.%d has no layout line for %s", kind_names[k], i,
                      entries[k][i].e.FileNum, kind_names[k]);
    }

  /* GAME.2 */
  {
    long n = 8 + 30L * (count[0] + count[1] + count[2] + count[3]) + 4, o = 8;
    buf = xmalloc((size_t)n);
    for (k = 0; k < K_COUNT; k++) put_le(buf + 2 * k, 2, count[k]);
    for (k = 0; k < K_COUNT; k++)
      for (i = 0; i < count[k]; i++, o += 30)
        memcpy(buf + o, &entries[k][i].e, 30);
    put_le(buf + o, 4, moas_chk(buf, o));
    out_path(path, sizeof(path), outdir, "GAME.2");
    write_file(path, buf, n);
    free(buf);
  }

  /* persons */
  out_path(path, sizeof(path), srcdir, "persons.txt");
  buf = parse_persons(path, &len);
  out_path(path, sizeof(path), outdir, "GAME.1");
  write_file(path, buf, len);
  free(buf);

  /* copied as they are */
  {
    static const char *const raw[] = { "GAME.XXX", "LICENCE.DAT" };
    for (i = 0; i < 2; i++) {
      buf = read_in(srcdir, raw[i], &len);
      if (!buf) die("cannot read %s/%s", srcdir, raw[i]);
      out_path(path, sizeof(path), outdir, "%s", raw[i]);
      write_file(path, buf, len);
      free(buf);
    }
  }

  printf("packed %s -> %s: %d scripts, %d graphics (%d reused from %s, %d encoded)\n",
         srcdir, outdir, count[K_PROG], count[K_BILD] + count[K_INV] + count[K_PHASE],
         reused, reference ? reference : "-", encoded);
  for (k = 0; k < K_COUNT; k++) {
    for (i = 0; i < count[k]; i++) free(entries[k][i].data);
    free(entries[k]);
  }
  dict_free(&dict);
  free_files(&ref);
  if (reference) free_catalog(&rc);
}

/* ---- encoder test ---------------------------------------------------- */

void cmd_gfx_test(const char *datadir)
{
  DataFiles files;
  Catalog c;
  int k, i, same = 0, total = 0, smaller = 0;
  long orig_bytes = 0, new_bytes = 0;
  memset(&files, 0, sizeof(files));
  files.dir = datadir;
  c = load_catalog_in(datadir);
  for (k = K_BILD; k < K_COUNT; k++)
    for (i = 0; i < c.n[k]; i++) {
      const MapFileDisk *e = &c.e[k][i];
      const uint8_t *b = blob(&files, e);
      int w = gfx_width(k, e), h = gfx_height(k, e);
      long n = (long)w * h, plen = gfx_payload(k, e), len;
      uint8_t *pix, *packed, *check;
      const char *err;
      if (!b) die("%s %d: unreadable", kind_names[k], i);
      pix = xmalloc((size_t)n);
      check = xmalloc((size_t)n);
      if (gfx_decode(b + 1, plen - 1, pix, n, w) != n) die("%s %d: corrupt", kind_names[k], i);
      if ((err = gfx_encode(pix, n, &packed, &len))) die("%s %d: %s", kind_names[k], i, err);
      if (gfx_decode(packed + 1, len - 1, check, n, w) != n || memcmp(pix, check, (size_t)n))
        die("%s %d: encoder round trip FAILED", kind_names[k], i);
      total++;
      orig_bytes += plen;
      new_bytes += len;
      if (len == plen && !memcmp(packed, b, (size_t)len)) same++;
      else {
        if (len < plen) smaller++;
        printf("%s %d: differs (%ld bytes, original %ld)\n", kind_names[k], i, len, plen);
      }
      free(pix);
      free(check);
      free(packed);
    }
  printf("%d of %d graphics encode byte-identical; %d of the others smaller; "
         "total %ld bytes, original %ld\n", same, total, smaller, new_bytes, orig_bytes);
  free_files(&files);
  free_catalog(&c);
}
