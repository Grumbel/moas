#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * moas-util — inspect, extract and rebuild Mänick / MOAS game data (GAME.*).
 *
 *   moas-util [--datadir DIR] list | info | licence | xxx
 *   moas-util [--datadir DIR] extract [--outdir DIR] [KIND]
 *   moas-util [--datadir DIR] extract-png [--outdir DIR] [--palette N] [KIND]
 *   moas-util [--datadir DIR] disasm [N]
 *   moas-util [--datadir DIR] strings [FMT]
 *   moas-util [--datadir DIR] unpack --outdir DIR
 *   moas-util [--srcdir DIR] pack --outdir DIR [--reference DIR]
 *
 * GAME.2 is the resource catalog: counts + MapFileDisk records pointing
 * into GAME.3 (scripts) and GAME.4 (graphics). unpack turns all data files
 * into a source tree (text and PNG, see mu_data.c), pack builds them back.
 */

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "mu_common.h"
#include "mu_data.h"
#include "mu_gfx.h"
#include "mu_png.h"
#include "mu_script.h"

static const char *g_outdir = "extracted";

static void print_entry(const char *kind, int idx, const MapFileDisk *e)
{
  printf("%-8s %4d  file=%d  pos=%8d  len=%8d  x=%5d y=%5d anz=%5d  inuse=%d  chk=0x%08x\n",
         kind, idx,
         (int)e->FileNum, (int)e->FilePos, (int)e->Length,
         (int)e->x, (int)e->y, (int)e->Anz,
         (int)e->InUse, (unsigned)e->Check);
}

static void cmd_list(const Catalog *c)
{
  int i, k;
  printf("# GAME.2 catalog  prog=%d  bild=%d  inventar=%d  phase=%d\n",
         c->n[K_PROG], c->n[K_BILD], c->n[K_INV], c->n[K_PHASE]);
  printf("# kind     idx  file    pos       len       x     y   anz  inuse  checksum\n");
  for (k = 0; k < K_COUNT; k++)
    for (i = 0; i < c->n[k]; i++) print_entry(kind_names[k], i, &c->e[k][i]);
}

static long file_size_fd(int fd)
{
  struct stat st;
  if (fstat(fd, &st) != 0) return -1;
  return (long)st.st_size;
}

/* GAME.PCK dictionary for script strings. */
static Dict g_dict;
static int g_dict_loaded;

static void load_dict(void)
{
  long len;
  uint8_t *buf;
  if (g_dict_loaded) return;
  g_dict_loaded = 1;
  buf = read_in(g_datadir, "GAME.PCK", &len);
  if (buf && dict_from_pck(&g_dict, buf, len) != NULL) memset(&g_dict, 0, sizeof(g_dict));
  free(buf);
}

static void unpack_to(char *out, size_t outsz, const uint8_t *packed)
{
  load_dict();
  out[0] = 0;
  if (!packed) return;
  if (g_dict.n <= 0) {
    strncpy(out, (const char *)packed, outsz - 1);
    out[outsz - 1] = 0;
    return;
  }
  dict_unpack(&g_dict, packed, out, outsz);
}

static int load_prog_blob(const Catalog *cat, int idx, uint8_t **out, int *out_len)
{
  char srcname[32];
  const MapFileDisk *e;
  int fd;
  uint8_t *buf;
  ssize_t n;

  if (idx < 0 || idx >= cat->n[K_PROG]) return -1;
  e = &cat->e[K_PROG][idx];
  if (e->Length <= 0 || e->FileNum < 1) {
    *out = NULL;
    *out_len = 0;
    return 0;
  }
  snprintf(srcname, sizeof(srcname), "GAME.%d", (int)e->FileNum);
  fd = open_data(srcname);
  if (fd < 0) return -1;
  if (lseek(fd, (off_t)e->FilePos, SEEK_SET) < 0) { close(fd); return -1; }
  buf = xmalloc((size_t)e->Length);
  n = read(fd, buf, (size_t)e->Length);
  close(fd);
  if (n != (ssize_t)e->Length) { free(buf); return -1; }
  *out = buf;
  *out_len = (int)e->Length;
  return 0;
}

static void cmd_info(void)
{
  static const char *names[] = {
    "GAME.1", "GAME.2", "GAME.3", "GAME.4",
    "GAME.PCK", "GAME.XXX", "GAME.MAP",
    "LICENCE.DAT", "Licence.Dat", "licence.dat",
    NULL
  };
  int i;
  Catalog c;
  printf("datadir: %s\n", g_datadir);
  for (i = 0; names[i]; i++) {
    int fd = open_data(names[i]);
    if (fd < 0) {
      printf("  %-14s  (missing)\n", names[i]);
      continue;
    }
    printf("  %-14s  %9ld bytes\n", names[i], file_size_fd(fd));
    close(fd);
  }
  c = load_catalog();
  printf("catalog: prog=%d bild=%d inventar=%d phase=%d\n",
         c.n[K_PROG], c.n[K_BILD], c.n[K_INV], c.n[K_PHASE]);
  free_catalog(&c);
}

static int extract_one(const char *kind, int idx, const MapFileDisk *e)
{
  char srcname[32], outpath[1024], outname[64];
  uint8_t *buf;
  int infd, outfd;
  ssize_t n;

  if (e->Length <= 0 || e->FileNum < 1) {
    printf("skip  %s[%d]  (empty or FileNum=%d len=%d)\n",
           kind, idx, (int)e->FileNum, (int)e->Length);
    return 0;
  }

  snprintf(srcname, sizeof(srcname), "GAME.%d", (int)e->FileNum);
  infd = open_data(srcname);
  if (infd < 0) {
    fprintf(stderr, "moas-util: cannot open %s for %s[%d]\n", srcname, kind, idx);
    return -1;
  }
  if (lseek(infd, (off_t)e->FilePos, SEEK_SET) == (off_t)-1) {
    fprintf(stderr, "moas-util: seek %s pos %d failed\n", srcname, (int)e->FilePos);
    close(infd);
    return -1;
  }
  buf = xmalloc((size_t)e->Length);
  n = read(infd, buf, (size_t)e->Length);
  close(infd);
  if (n != (ssize_t)e->Length) {
    fprintf(stderr, "moas-util: short read %s[%d] from %s\n", kind, idx, srcname);
    free(buf);
    return -1;
  }

  snprintf(outname, sizeof(outname), "%s_%04d.bin", kind, idx);
  if (!path_join(outpath, sizeof(outpath), g_outdir, outname)) {
    free(buf);
    die("outdir path too long");
  }
  outfd = open(outpath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (outfd < 0) {
    fprintf(stderr, "moas-util: cannot write %s: %s\n", outpath, strerror(errno));
    free(buf);
    return -1;
  }
  if (write(outfd, buf, (size_t)e->Length) != (ssize_t)e->Length) {
    fprintf(stderr, "moas-util: short write %s\n", outpath);
    close(outfd);
    free(buf);
    return -1;
  }
  close(outfd);
  free(buf);
  printf("wrote %s  (%d bytes from %s @ %d)\n",
         outpath, (int)e->Length, srcname, (int)e->FilePos);
  return 0;
}

static int extract_table(const char *kind, const MapFileDisk *t, int n)
{
  int i, errs = 0;
  for (i = 0; i < n; i++)
    if (extract_one(kind, i, &t[i]) < 0) errs++;
  return errs;
}

static void cmd_extract(const Catalog *c, const char *kind_filter)
{
  int errs = 0;
  if (mkdir_p(g_outdir) != 0)
    die_errno(g_outdir);

  if (!kind_filter || !strcmp(kind_filter, "all") || !strcmp(kind_filter, "prog"))
    errs += extract_table("prog", c->e[K_PROG], c->n[K_PROG]);
  if (!kind_filter || !strcmp(kind_filter, "all") || !strcmp(kind_filter, "bild"))
    errs += extract_table("bild", c->e[K_BILD], c->n[K_BILD]);
  if (!kind_filter || !strcmp(kind_filter, "all") || !strcmp(kind_filter, "inv") ||
      !strcmp(kind_filter, "inventar"))
    errs += extract_table("inv", c->e[K_INV], c->n[K_INV]);
  if (!kind_filter || !strcmp(kind_filter, "all") || !strcmp(kind_filter, "phase"))
    errs += extract_table("phase", c->e[K_PHASE], c->n[K_PHASE]);

  if (errs)
    fprintf(stderr, "moas-util: %d extract error(s)\n", errs);
}

/* ---- graphics → PNG -------------------------------------------------- */

static int g_palette_bild = -1;   /* --palette N: force palette of bild N */

static uint8_t *load_blob(const MapFileDisk *e)
{
  char srcname[32];
  uint8_t *buf;
  int fd;
  ssize_t n;
  if (e->Length <= 0 || e->FileNum < 1) return NULL;
  snprintf(srcname, sizeof(srcname), "GAME.%d", (int)e->FileNum);
  fd = open_data(srcname);
  if (fd < 0) return NULL;
  if (lseek(fd, (off_t)e->FilePos, SEEK_SET) == (off_t)-1) { close(fd); return NULL; }
  buf = xmalloc((size_t)e->Length);
  n = read(fd, buf, (size_t)e->Length);
  close(fd);
  if (n != (ssize_t)e->Length) { free(buf); return NULL; }
  return buf;
}

/* Copy the 768-byte palette trailing bild idx into pal. */
static int load_bild_palette(const Catalog *c, int idx, uint8_t *pal)
{
  uint8_t *buf;
  if (idx < 0 || idx >= c->n[K_BILD] || !bild_has_palette(&c->e[K_BILD][idx])) return -1;
  buf = load_blob(&c->e[K_BILD][idx]);
  if (!buf) return -1;
  memcpy(pal, buf + c->e[K_BILD][idx].Length - 768, 768);
  free(buf);
  return 0;
}

/* Decode one resource and write it as PNG. pal is VGA 6-bit. */
static int png_one(int kind, int idx, const MapFileDisk *e, const uint8_t *pal,
                   const uint8_t *alpha)
{
  char outname[64], outpath[1024];
  int w = gfx_width(kind, e), h = gfx_height(kind, e);
  long n = (long)w * h, payload = gfx_payload(kind, e);
  uint8_t *blob, *pix, rgb[768];
  int rc;

  if (w <= 0 || h <= 0 || payload < 1) {
    printf("skip  %s[%d]  (empty)\n", kind_names[kind], idx);
    return 0;
  }
  blob = load_blob(e);
  if (!blob) {
    fprintf(stderr, "moas-util: cannot read %s[%d]\n", kind_names[kind], idx);
    return -1;
  }
  pix = xmalloc((size_t)n);
  if (gfx_decode(blob + 1, payload - 1, pix, n, w) != n) {
    fprintf(stderr, "moas-util: %s[%d]: corrupt or unsupported pixel stream\n", kind_names[kind], idx);
    free(blob);
    free(pix);
    return -1;
  }
  free(blob);
  snprintf(outname, sizeof(outname), "%s_%04d.png", kind_names[kind], idx);
  if (!path_join(outpath, sizeof(outpath), g_outdir, outname)) die("outdir path too long");
  vga_to_rgb(pal, rgb);
  rc = png_write(outpath, pix, w, h, rgb, alpha);
  free(pix);
  if (rc == 0) printf("wrote %s  (%dx%d)\n", outpath, w, h);
  return rc;
}

static int want_kind(const char *filter, const char *kind)
{
  return !filter || !strcmp(filter, "all") || !strcmp(filter, kind) ||
         (!strcmp(kind, "inv") && !strcmp(filter, "inventar"));
}

static void cmd_extract_png(const Catalog *c, const char *kind_filter)
{
  uint8_t sprite_pal[768], room_pal[768], tmp[768], alpha[256];
  int i, errs = 0, have_room = 0;

  if (!want_kind(kind_filter, "bild") && !want_kind(kind_filter, "inv") &&
      !want_kind(kind_filter, "phase")) {
    fprintf(stderr, "moas-util: unknown extract-png kind '%s' (all|bild|inv|phase)\n",
            kind_filter);
    exit(1);
  }
  if (mkdir_p(g_outdir) != 0)
    die_errno(g_outdir);

  /* Sprites carry no palette of their own: use --palette N or the first
   * room palette. */
  memset(sprite_pal, 0, sizeof(sprite_pal));
  if (g_palette_bild >= 0) {
    if (load_bild_palette(c, g_palette_bild, sprite_pal) != 0) {
      fprintf(stderr, "moas-util: bild %d has no palette\n", g_palette_bild);
      exit(1);
    }
  } else {
    for (i = 0; i < c->n[K_BILD]; i++)
      if (load_bild_palette(c, i, sprite_pal) == 0) break;
  }
  memcpy(room_pal, sprite_pal, sizeof(room_pal));

  if (want_kind(kind_filter, "bild")) {
    for (i = 0; i < c->n[K_BILD]; i++) {
      const MapFileDisk *e = &c->e[K_BILD][i];
      if (bild_has_palette(e)) {
        /* Room picture: 320 wide, palette trails the pixel stream. */
        if (load_bild_palette(c, i, tmp) == 0) {
          if (g_palette_bild < 0) memcpy(room_pal, tmp, sizeof(room_pal));
          have_room = 1;
        }
        errs += png_one(K_BILD, i, e, g_palette_bild >= 0 ? sprite_pal : tmp, NULL) < 0;
      } else if (i == 0) {
        /* Bild 0: boxed overlay, 255 is transparent (DisplayBox). */
        memset(alpha, 255, sizeof(alpha));
        alpha[255] = 0;
        errs += png_one(K_BILD, i, e, sprite_pal, alpha) < 0;
      } else {
        /* Info maps (LadeInfoBild): 320 wide, shown with the preceding room
         * palette. */
        errs += png_one(K_BILD, i, e, have_room ? room_pal : sprite_pal, NULL) < 0;
      }
    }
  }
  /* Sprites: 254 is the glass colour; 0 shows through as well. */
  memset(alpha, 255, sizeof(alpha));
  alpha[0] = alpha[254] = 0;
  if (want_kind(kind_filter, "inv"))
    for (i = 0; i < c->n[K_INV]; i++)
      errs += png_one(K_INV, i, &c->e[K_INV][i], sprite_pal, alpha) < 0;
  if (want_kind(kind_filter, "phase"))
    for (i = 0; i < c->n[K_PHASE]; i++)
      /* Animation frames are stacked vertically. */
      errs += png_one(K_PHASE, i, &c->e[K_PHASE][i], sprite_pal, alpha) < 0;

  if (errs)
    fprintf(stderr, "moas-util: %d extract-png error(s)\n", errs);
}

static void cmd_licence(void)
{
  int fd = open_data("LICENCE.DAT");
  char buf[256];
  ssize_t n;
  long sz;
  if (fd < 0) fd = open_data("Licence.Dat");
  if (fd < 0) fd = open_data("licence.dat");
  if (fd < 0) die("cannot open LICENCE.DAT");
  sz = file_size_fd(fd);
  if (sz < 4 || sz >= (long)sizeof(buf)) {
    close(fd);
    die("LICENCE.DAT unexpected size");
  }
  n = read(fd, buf, (size_t)(sz - 4));
  close(fd);
  if (n < 0) die("read licence");
  buf[n] = 0;
  /* CP850-ish: print bytes; high bytes as escapes */
  {
    int i;
    for (i = 0; i < n; i++) {
      unsigned char c = (unsigned char)buf[i];
      if (c >= 32 && c < 127) putchar((char)c);
      else if (c == '\n' || c == '\r' || c == '\t') putchar((char)c);
      else printf("\\x%02x", c);
    }
    putchar('\n');
  }
}

static void cmd_xxx(void)
{
  /* GAME.XXX: 7 fonts; each: 1 byte SizeY, 2 byte column count, then columns */
  int fd = open_data("GAME.XXX");
  int f;
  if (fd < 0) die("cannot open GAME.XXX");
  printf("# GAME.XXX fonts\n");
  for (f = 0; f < 7; f++) {
    uint8_t size_y;
    uint8_t cnt_lo, cnt_hi;
    int cols, c, gaps = 0;
    if (read(fd, &size_y, 1) != 1) die("GAME.XXX truncated (SizeY)");
    if (read(fd, &cnt_lo, 1) != 1 || read(fd, &cnt_hi, 1) != 1)
      die("GAME.XXX truncated (cols)");
    cols = cnt_lo | (cnt_hi << 8);
    for (c = 0; c < cols; c++) {
      uint8_t trip[3];
      if (read(fd, trip, 3) != 3) die("GAME.XXX truncated (glyph data)");
      if (trip[0] == 0 && trip[1] == 0 && trip[2] == 0) gaps++;
    }
    printf("font %d  SizeY=%u  columns=%d  gap_markers=%d\n",
           f, (unsigned)size_y, cols, gaps);
  }
  close(fd);
}



/* ---- script disassembler -------------------------------------------- */

static void cmd_disasm(const Catalog *cat, int prog_index)
{
  int n = cat->n[K_PROG], i;
  Prog *progs = xmalloc(sizeof(Prog) * (size_t)(n + 1));
  load_dict();
  for (i = 0; i < n; i++) {
    uint8_t *buf = NULL;
    int len = 0;
    if (load_prog_blob(cat, i, &buf, &len) != 0) die("cannot load program %d", i);
    progs[i].code = buf;
    progs[i].len = len;
  }
  if (prog_index >= n) die("no program %d", prog_index);
  for (i = 0; i < n; i++)
    if (prog_index < 0 || i == prog_index) {
      script_disasm(stdout, i, progs, n, &g_dict);
      if (prog_index < 0) putchar('\n');
    }
  for (i = 0; i < n; i++) free(progs[i].code);
  free(progs);
}

/* Instruction walk for `strings`: unknown opcodes are skipped a byte at a
 * time, as this command always did. */
static int strings_insn_size(const uint8_t *code, int len, int pc)
{
  int sz = script_insn_size(code, len, pc);
  if (sz < 0 && pc < len && !script_op_name(code[pc])) return 1;
  return sz;
}

/* ---- extract translatable strings ------------------------------------ */

static void cp850_to_utf8(const uint8_t *s, char *out, size_t outsz)
{
  size_t o = 0;
  int i;
  for (i = 0; s[i] && o + 4 < outsz; i++) {
    unsigned char c = s[i];
    const char *u = NULL;
    if (c >= 32 && c < 127) {
      out[o++] = (char)c;
      continue;
    }
    switch (c) {
      case 0x81: u = "ü"; break;
      case 0x84: u = "ä"; break;
      case 0x8e: u = "Ä"; break;
      case 0x94: u = "ö"; break;
      case 0x99: u = "Ö"; break;
      case 0x9a: u = "Ü"; break;
      case 0xe1: u = "ß"; break;
      case '\n': u = "\n"; break;
      default:
        if (o + 6 < outsz) {
          int n = snprintf(out + o, outsz - o, "\\x%02x", c);
          if (n > 0) o += (size_t)n;
        }
        continue;
    }
    while (*u && o + 1 < outsz) out[o++] = *u++;
  }
  out[o] = 0;
}

static void json_escape(const char *s, char *out, size_t outsz)
{
  size_t o = 0;
  for (; *s && o + 7 < outsz; s++) {
    unsigned char c = (unsigned char)*s;
    if (c == '"' || c == '\\') {
      out[o++] = '\\';
      out[o++] = (char)c;
    } else if (c == '\n') {
      out[o++] = '\\';
      out[o++] = 'n';
    } else if (c == '\r') {
      out[o++] = '\\';
      out[o++] = 'r';
    } else if (c == '\t') {
      out[o++] = '\\';
      out[o++] = 't';
    } else if (c < 32) {
      int n = snprintf(out + o, outsz - o, "\\u%04x", c);
      if (n > 0) o += (size_t)n;
    } else {
      out[o++] = (char)c;
    }
  }
  out[o] = 0;
}

typedef struct {
  char *id;
  char *text;   /* UTF-8 */
  char *ref;    /* source location */
} StrRow;

static StrRow *g_rows;
static int g_nrows, g_rows_cap;

static void rows_add(const char *id, const char *text_utf8, const char *ref)
{
  StrRow *r;
  if (g_nrows >= g_rows_cap) {
    int nc = g_rows_cap ? g_rows_cap * 2 : 256;
    g_rows = realloc(g_rows, (size_t)nc * sizeof(StrRow));
    if (!g_rows) die("out of memory");
    g_rows_cap = nc;
  }
  r = &g_rows[g_nrows++];
  r->id = xstrdup(id);
  r->text = xstrdup(text_utf8);
  r->ref = xstrdup(ref);
}

static void extract_prog_strings(const Catalog *cat, int prog_index)
{
  uint8_t *buf = NULL;
  int len = 0, pc = 0;
  if (load_prog_blob(cat, prog_index, &buf, &len) != 0 || !buf)
    return;
  while (pc < len) {
    uint8_t op = buf[pc];
    int sz = strings_insn_size(buf, len, pc);
    int soff = script_string_offset(op);
    if (sz < 1) break;
    if (soff >= 0 && pc + soff < len) {
      const uint8_t *packed = buf + pc + soff;
      char unpacked[512], utf8[1024], id[64], ref[96];
      const char *name = script_op_name(op);
      unpack_to(unpacked, sizeof(unpacked), packed);
      if (unpacked[0]) {
        cp850_to_utf8((const uint8_t *)unpacked, utf8, sizeof(utf8));
        snprintf(id, sizeof(id), "script.p%d.0x%04x", prog_index, pc);
        snprintf(ref, sizeof(ref), "prog %d @ %04x op %s",
                 prog_index, pc, name ? name : "?");
        rows_add(id, utf8, ref);
      }
    }
    pc += sz;
  }
  free(buf);
}

static void extract_pck_strings(void)
{
  int i;
  char utf8[512], id[32], ref[32];
  load_dict();
  for (i = 0; i < g_dict.n; i++) {
    if (!g_dict.piece[i][0]) continue;
    cp850_to_utf8((const uint8_t *)g_dict.piece[i], utf8, sizeof(utf8));
    snprintf(id, sizeof(id), "pck.%d", i);
    snprintf(ref, sizeof(ref), "GAME.PCK token %d", i);
    rows_add(id, utf8, ref);
  }
}

/* Hardcoded UI from the SDL port (not in GAME.PCK). */
static void extract_ui_strings(void)
{
  static const char *const ui[][2] = {
    { "ui.verb.use", "BENUTZE %s" },
    { "ui.verb.use_with", "BENUTZE %s MIT %s" },
    { "ui.verb.look", "SCHAU %s AN" },
    { "ui.verb.talk", "REDE MIT %s" },
    { "ui.verb.move", "BEWEGE %s" },
    { "ui.verb.move_dir", "BEWEGE %s IN RICHTUNG %s" },
    { "ui.save.title", "Speichern:" },
    { "ui.load.title", "Laden:" },
    { "ui.save.fail", "Speichern fehlgeschlagen" },
    { "ui.load.fail", "Laden fehlgeschlagen" },
    { "ui.slot.none", "Kein Slot gewaehlt" },
    { "ui.slot.of", "%i von %i" },
    { "ui.overlay.save", "SAVE" },
    { "ui.overlay.load", "LOAD" },
    { "ui.overlay.skip_scene", "SKIP SCENE" },
    { "ui.overlay.skip_text", "SKIP TEXT" },
    { "ui.overlay.use", "USE" },
    { "ui.overlay.look", "LOOK" },
    { "ui.overlay.options", "OPTIONS" },
    { "ui.overlay.hint", "HINT" },
    { "ui.menu.options", "Optionen (F12)" },
    { "ui.opt.title", "OPTIONEN" },
    { "ui.opt.hint_touch", "ZEILE ANTIPPEN - DANEBEN TIPPEN SCHLIESST" },
    { "ui.opt.hint_keys", "KLICK ODER PFEILE/ENTER - ESC/F12 SCHLIESST" },
    { "ui.opt.language", "SPRACHE" },
    { "ui.opt.fullscreen", "VOLLBILD" },
    { "ui.opt.aspect", "BILDFORMAT" },
    { "ui.opt.touch", "TOUCH-TASTEN" },
    { "ui.opt.speed", "SPIELTEMPO" },
    { "ui.opt.speech", "SPRECHTEMPO" },
    { "ui.opt.resume", "WEITER" },
    { "ui.opt.restart", "NEU STARTEN" },
    { "ui.opt.restart_sure", "NEU STARTEN - SICHER?" },
    { "ui.opt.quit", "BEENDEN" },
    { "ui.opt.settings", "EINSTELLUNGEN" },
    { "ui.opt.back", "ZURUECK" },
    { "ui.opt.on", "AN" },
    { "ui.opt.off", "AUS" },
    { "ui.key.save", "SPEICHERN" },
    { "ui.key.load", "LADEN" },
    { "ui.key.options", "OPTIONEN" },
    { "ui.key.skip_text", "TEXT UEBERSPRINGEN" },
    { "ui.key.skip_scene", "SZENE UEBERSPRINGEN" },
    { "ui.key.hotspots", "HOTSPOTS ZEIGEN" },
    { "ui.key.use", "BENUTZEN" },
    { "ui.key.look", "ANSEHEN" },
    { NULL, NULL }
  };
  int i;
  for (i = 0; ui[i][0]; i++)
    rows_add(ui[i][0], ui[i][1], "source UI");
}

static void emit_json(void)
{
  /* Index-based catalog for translation tools:
   *   { "source_language": "de",
   *     "strings": [ { "id", "source", "target", "note" }, ... ] }
   * Fill "target", then derive lang/<code>.ui / .script for the engine.
   */
  int i;
  char esc[2048], idesc[256], notee[512];
  printf("{\n");
  printf("  \"source_language\": \"de\",\n");
  printf("  \"strings\": [\n");
  for (i = 0; i < g_nrows; i++) {
    json_escape(g_rows[i].id, idesc, sizeof(idesc));
    json_escape(g_rows[i].text, esc, sizeof(esc));
    json_escape(g_rows[i].ref, notee, sizeof(notee));
    printf("    {\"id\": \"%s\", \"source\": \"%s\", \"target\": \"\", \"note\": \"%s\"}%s\n",
           idesc, esc, notee, i + 1 < g_nrows ? "," : "");
  }
  printf("  ]\n");
  printf("}\n");
}

/* Flat { "id": "source", ... } for pipelines that want a simple map. */
static void emit_json_map(void)
{
  int i;
  char esc[2048], idesc[256];
  printf("{\n");
  for (i = 0; i < g_nrows; i++) {
    json_escape(g_rows[i].id, idesc, sizeof(idesc));
    json_escape(g_rows[i].text, esc, sizeof(esc));
    printf("  \"%s\": \"%s\"%s\n",
           idesc, esc, i + 1 < g_nrows ? "," : "");
  }
  printf("}\n");
}

static void emit_pot(void)
{
  int i;
  printf("# MOAS / Mänick Mänschion — extracted strings\n");
  printf("# Charset: UTF-8\n");
  printf("msgid \"\"\nmsgstr \"\"\n"
         "\"Content-Type: text/plain; charset=UTF-8\\n\"\n"
         "\"Language: de\\n\"\n\n");
  for (i = 0; i < g_nrows; i++) {
    char esc[2048];
    json_escape(g_rows[i].text, esc, sizeof(esc)); /* close enough for pot */
    printf("#. %s\n", g_rows[i].ref);
    printf("#: %s\n", g_rows[i].id);
    {
      char idesc[256];
      json_escape(g_rows[i].id, idesc, sizeof(idesc));
      printf("msgctxt \"%s\"\n", idesc);
    }
    printf("msgid \"%s\"\n", esc);
    printf("msgstr \"\"\n\n");
  }
}

static void emit_tsv(void)
{
  int i;
  printf("id\ttext\tref\n");
  for (i = 0; i < g_nrows; i++) {
    /* TSV: escape tabs/newlines in text */
    char *t = g_rows[i].text;
    printf("%s\t", g_rows[i].id);
    for (; *t; t++) {
      if (*t == '\t') fputs("\\t", stdout);
      else if (*t == '\n') fputs("\\n", stdout);
      else putchar(*t);
    }
    printf("\t%s\n", g_rows[i].ref);
  }
}

static void cmd_strings(const Catalog *cat, const char *fmt)
{
  int i;
  g_nrows = 0;
  extract_ui_strings();
  extract_pck_strings();
  for (i = 0; i < cat->n[K_PROG]; i++)
    extract_prog_strings(cat, i);

  if (!fmt || !fmt[0] || !strcmp(fmt, "json"))
    emit_json();
  else if (!strcmp(fmt, "json-map"))
    emit_json_map();
  else if (!strcmp(fmt, "pot"))
    emit_pot();
  else if (!strcmp(fmt, "tsv"))
    emit_tsv();
  else {
    fprintf(stderr, "moas-util: unknown strings format '%s' (json|json-map|pot|tsv)\n", fmt);
    exit(1);
  }

  fprintf(stderr, "moas-util: extracted %d string occurrences\n", g_nrows);
}


static void usage(const char *argv0)
{
  fprintf(stderr,
    "Usage: %s [options] <command> [args]\n"
    "\n"
    "Inspect, extract and rebuild MOAS / Mänick game data (GAME.* files).\n"
    "\n"
    "Options:\n"
    "  --datadir DIR   Directory with GAME.1, GAME.2, … (default: . or $MOAS_DATADIR)\n"
    "  --outdir DIR    Output directory (default: extracted)\n"
    "  --srcdir DIR    pack: source tree written by unpack (default: .)\n"
    "  --reference DIR pack: reuse the packed graphics of these data files where\n"
    "                  the pixels are unchanged (needed for identical output)\n"
    "  --palette N     extract-png: use the palette of bild N for every image\n"
    "  -h, --help      This help\n"
    "\n"
    "Commands:\n"
    "  info              File sizes and catalog counts\n"
    "  list              List every catalog entry in GAME.2\n"
    "  extract [KIND]    Write raw resource blobs (KIND: all|prog|bild|inv|phase)\n"
    "  extract-png [KIND]  Decode graphics to PNG (KIND: all|bild|inv|phase)\n"
    "  licence           Print LICENCE.DAT text\n"
    "  xxx               Summarise GAME.XXX font tables\n"
    "  disasm [N]        Disassemble script program N (default: all)\n"
    "  strings [FMT]     Extract translatable text (json|json-map|pot|tsv, default json)\n"
    "  unpack            Convert all data files into an editable source tree in --outdir\n"
    "                    (scripts as text, graphics as PNG, catalog/persons as text)\n"
    "  pack              Build the data files from --srcdir into --outdir\n"
    "  gfx-test          Re-encode every graphic, report which come out identical\n"
    "\n"
    "Examples:\n"
    "  %s --datadir manick info\n"
    "  %s --datadir manick list | less\n"
    "  %s --datadir manick extract --outdir /tmp/moas-out bild\n"
    "  %s --datadir manick extract-png --outdir /tmp/moas-png\n"
    "  %s --datadir manick disasm 0\n"
    "  %s --datadir manick strings pot > manick.pot\n"
    "  %s --datadir manick strings json > manick.de.json\n"
    "  %s --datadir manick --outdir manick-src unpack\n"
    "  %s --srcdir manick-src --outdir build/data --reference manick pack\n",
    argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0, argv0);
}

int main(int argc, char **argv)
{
  const char *cmd = NULL;
  const char *extract_kind = "all";
  const char *srcdir = ".", *reference = NULL;
  int have_outdir = 0;
  Catalog cat;
  int i;
  const char *env;

  env = getenv("MOAS_DATADIR");
  if (env && env[0]) g_datadir = env;

  for (i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
      usage(argv[0]);
      return 0;
    }
    if (!strcmp(argv[i], "--datadir") && i + 1 < argc) {
      g_datadir = argv[++i];
      continue;
    }
    if (!strncmp(argv[i], "--datadir=", 10)) {
      g_datadir = argv[i] + 10;
      continue;
    }
    if (!strcmp(argv[i], "--outdir") && i + 1 < argc) {
      g_outdir = argv[++i];
      have_outdir = 1;
      continue;
    }
    if (!strncmp(argv[i], "--outdir=", 9)) {
      g_outdir = argv[i] + 9;
      have_outdir = 1;
      continue;
    }
    if (!strcmp(argv[i], "--srcdir") && i + 1 < argc) {
      srcdir = argv[++i];
      continue;
    }
    if (!strncmp(argv[i], "--srcdir=", 9)) {
      srcdir = argv[i] + 9;
      continue;
    }
    if (!strcmp(argv[i], "--reference") && i + 1 < argc) {
      reference = argv[++i];
      continue;
    }
    if (!strncmp(argv[i], "--reference=", 12)) {
      reference = argv[i] + 12;
      continue;
    }
    if (!strcmp(argv[i], "--palette") && i + 1 < argc) {
      g_palette_bild = atoi(argv[++i]);
      continue;
    }
    if (!strncmp(argv[i], "--palette=", 10)) {
      g_palette_bild = atoi(argv[i] + 10);
      continue;
    }
    if (argv[i][0] == '-') {
      fprintf(stderr, "moas-util: unknown option '%s'\n", argv[i]);
      usage(argv[0]);
      return 1;
    }
    if (!cmd) {
      cmd = argv[i];
    } else if ((!strcmp(cmd, "extract") || !strcmp(cmd, "extract-png") || !strcmp(cmd, "disasm") || !strcmp(cmd, "strings"))
               && !strcmp(extract_kind, "all")) {
      extract_kind = argv[i];
    } else {
      fprintf(stderr, "moas-util: unexpected argument '%s'\n", argv[i]);
      return 1;
    }
  }

  if (!cmd) {
    usage(argv[0]);
    return 1;
  }

  if (!strcmp(cmd, "info")) {
    cmd_info();
    return 0;
  }
  if (!strcmp(cmd, "licence")) {
    cmd_licence();
    return 0;
  }
  if (!strcmp(cmd, "xxx")) {
    cmd_xxx();
    return 0;
  }
  if (!strcmp(cmd, "unpack") || !strcmp(cmd, "pack")) {
    /* Writing a whole tree into ./extracted by accident is unhelpful. */
    if (!have_outdir) die("%s needs --outdir", cmd);
    if (!strcmp(cmd, "unpack")) cmd_unpack(g_datadir, g_outdir);
    else cmd_pack(srcdir, g_outdir, reference);
    return 0;
  }
  if (!strcmp(cmd, "gfx-test")) {
    cmd_gfx_test(g_datadir);
    return 0;
  }
  if (!strcmp(cmd, "list") || !strcmp(cmd, "extract") || !strcmp(cmd, "extract-png")
      || !strcmp(cmd, "disasm") || !strcmp(cmd, "strings")) {
    int prog_idx = -1;
    cat = load_catalog();
    if (!strcmp(cmd, "list"))
      cmd_list(&cat);
    else if (!strcmp(cmd, "extract"))
      cmd_extract(&cat, extract_kind);
    else if (!strcmp(cmd, "extract-png"))
      cmd_extract_png(&cat, extract_kind);
    else if (!strcmp(cmd, "strings")) {
      const char *fmt = "json";
      if (strcmp(extract_kind, "all") != 0)
        fmt = extract_kind;
      else {
        for (i = 1; i < argc; i++) {
          if (!strcmp(argv[i], "strings") && i + 1 < argc && argv[i+1][0] != '-') {
            fmt = argv[i+1];
            break;
          }
        }
      }
      cmd_strings(&cat, fmt);
    } else {
      /* disasm [N] — N already consumed as extract_kind if present */
      if (strcmp(extract_kind, "all") != 0) {
        prog_idx = atoi(extract_kind);
      } else {
        /* check leftover: for disasm, second arg was stored in extract_kind only if extract */
        for (i = 1; i < argc; i++) {
          if (!strcmp(argv[i], "disasm") && i + 1 < argc && argv[i+1][0] != '-') {
            prog_idx = atoi(argv[i+1]);
            break;
          }
        }
      }
      cmd_disasm(&cat, prog_idx);
    }
    free_catalog(&cat);
    return 0;
  }

  fprintf(stderr, "moas-util: unknown command '%s'\n", cmd);
  usage(argv[0]);
  return 1;
}
