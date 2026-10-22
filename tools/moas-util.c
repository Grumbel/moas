#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * moas-util — inspect and extract Mänick / MOAS game data (GAME.*).
 *
 *   moas-util [--datadir DIR] list
 *   moas-util [--datadir DIR] info
 *   moas-util [--datadir DIR] extract [--outdir DIR] [KIND]
 *   moas-util [--datadir DIR] licence
 *   moas-util [--datadir DIR] xxx
 *
 * KIND for extract: all | prog | bild | inventar | phase  (default: all)
 *
 * GAME.2 is the resource catalog: counts + MapFileDisk records pointing
 * into GAME.1 / GAME.3 / GAME.4 (and similar). Payloads are left raw
 * (often compressed with the engine's pack format).
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
#if defined(_WIN32)
#include <direct.h>
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

typedef struct {
  int16_t n_prog, n_bild, n_inv, n_phase;
  MapFileDisk *prog;
  MapFileDisk *bild;
  MapFileDisk *inv;
  MapFileDisk *phase;
} Catalog;

static const char *g_datadir = ".";
static const char *g_outdir = "extracted";

static void die(const char *msg)
{
  fprintf(stderr, "moas-util: %s\n", msg);
  exit(1);
}

static void die_errno(const char *what)
{
  fprintf(stderr, "moas-util: %s: %s\n", what, strerror(errno));
  exit(1);
}

static int path_join(char *out, size_t outsz, const char *dir, const char *name)
{
  int n = snprintf(out, outsz, "%s/%s", dir, name);
  return n > 0 && (size_t)n < outsz;
}

static int open_data(const char *name)
{
  char path[1024];
  int fd;
  if (!path_join(path, sizeof(path), g_datadir, name))
    die("path too long");
  fd = open(path, O_RDONLY);
  if (fd < 0) {
    /* case fold basename-style attempts */
    char folded[256];
    size_t i, n = strlen(name);
    if (n >= sizeof(folded)) return -1;
    for (i = 0; i < n; i++) folded[i] = (char)toupper((unsigned char)name[i]);
    folded[n] = 0;
    if (!path_join(path, sizeof(path), g_datadir, folded)) return -1;
    fd = open(path, O_RDONLY);
    if (fd >= 0) return fd;
    for (i = 0; i < n; i++) folded[i] = (char)tolower((unsigned char)name[i]);
    if (!path_join(path, sizeof(path), g_datadir, folded)) return -1;
    fd = open(path, O_RDONLY);
  }
  return fd;
}

static int16_t read_i16(int fd)
{
  uint8_t b[2];
  if (read(fd, b, 2) != 2) die("short read (i16)");
  return (int16_t)(b[0] | (b[1] << 8));
}

static void *xmalloc(size_t n)
{
  void *p = malloc(n ? n : 1);
  if (!p) die("out of memory");
  return p;
}

static char *xstrdup(const char *s)
{
  size_t n = strlen(s) + 1;
  char *p = xmalloc(n);
  memcpy(p, s, n);
  return p;
}

static MapFileDisk *read_table(int fd, int count)
{
  MapFileDisk *t;
  size_t bytes;
  if (count <= 0) return NULL;
  bytes = (size_t)count * sizeof(MapFileDisk);
  t = xmalloc(bytes);
  if (read(fd, t, bytes) != (ssize_t)bytes) die("short read (map table)");
  return t;
}

static Catalog load_catalog(void)
{
  Catalog c;
  int fd = open_data("GAME.2");
  if (fd < 0) die("cannot open GAME.2 (set --datadir to manick/)");
  memset(&c, 0, sizeof(c));
  c.n_prog  = read_i16(fd);
  c.n_bild  = read_i16(fd);
  c.n_inv   = read_i16(fd);
  c.n_phase = read_i16(fd);
  if (c.n_prog < 0 || c.n_bild < 0 || c.n_inv < 0 || c.n_phase < 0)
    die("GAME.2: negative counts");
  c.prog  = read_table(fd, c.n_prog);
  c.bild  = read_table(fd, c.n_bild);
  c.inv   = read_table(fd, c.n_inv);
  c.phase = read_table(fd, c.n_phase);
  close(fd);
  return c;
}

static void free_catalog(Catalog *c)
{
  free(c->prog);
  free(c->bild);
  free(c->inv);
  free(c->phase);
  memset(c, 0, sizeof(*c));
}

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
  int i;
  printf("# GAME.2 catalog  prog=%d  bild=%d  inventar=%d  phase=%d\n",
         c->n_prog, c->n_bild, c->n_inv, c->n_phase);
  printf("# kind     idx  file    pos       len       x     y   anz  inuse  checksum\n");
  for (i = 0; i < c->n_prog; i++)  print_entry("prog", i, &c->prog[i]);
  for (i = 0; i < c->n_bild; i++)  print_entry("bild", i, &c->bild[i]);
  for (i = 0; i < c->n_inv; i++)   print_entry("inv", i, &c->inv[i]);
  for (i = 0; i < c->n_phase; i++) print_entry("phase", i, &c->phase[i]);
}

static long file_size_fd(int fd)
{
  struct stat st;
  if (fstat(fd, &st) != 0) return -1;
  return (long)st.st_size;
}

/* GAME.PCK dictionary for script UnPack. */
static char *g_codes[257];
static int g_anz_codes;
static int g_pack_loaded;
static char *g_pack_buf;

static void load_pack_codes(void)
{
  int fd;
  long sz;
  int c, value = 0, NEW = 1;
  if (g_pack_loaded) return;
  g_pack_loaded = 1;
  fd = open_data("GAME.PCK");
  if (fd < 0) return;
  sz = file_size_fd(fd);
  if (sz < 5) { close(fd); return; }
  g_pack_buf = xmalloc((size_t)(sz - 4));
  if (read(fd, g_pack_buf, (size_t)(sz - 4)) != (ssize_t)(sz - 4)) {
    free(g_pack_buf);
    g_pack_buf = NULL;
    close(fd);
    return;
  }
  close(fd);
  g_anz_codes = 0;
  for (c = 1; c < (int)(sz - 4); c++, value++) {
    g_pack_buf[c] ^= (char)(value & 0xff);
    if (NEW) {
      if (g_anz_codes < 256)
        g_codes[g_anz_codes++] = g_pack_buf + c;
      NEW = 0;
    }
    if (g_pack_buf[c] == 0) NEW = 1;
  }
}

static void unpack_to(char *out, size_t outsz, const uint8_t *packed)
{
  size_t used = 0;
  int i, l;
  load_pack_codes();
  out[0] = 0;
  if (!packed) return;
  if (g_anz_codes <= 0) {
    strncpy(out, (const char *)packed, outsz - 1);
    out[outsz - 1] = 0;
    return;
  }
  l = (int)strlen((const char *)packed);
  for (i = 0; i < l; i++) {
    unsigned idx = (unsigned char)packed[i];
    const char *piece = (idx < (unsigned)g_anz_codes) ? g_codes[idx] : NULL;
    size_t plen;
    if (!piece) continue;
    plen = strlen(piece);
    if (used + plen >= outsz - 1) break;
    memcpy(out + used, piece, plen);
    used += plen;
    out[used] = 0;
  }
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
         c.n_prog, c.n_bild, c.n_inv, c.n_phase);
  free_catalog(&c);
}

static int mkdir_p(const char *path)
{
  struct stat st;
  if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) return 0;
#if defined(_WIN32)
  if (_mkdir(path) == 0) return 0;
#else
  if (mkdir(path, 0755) == 0) return 0;
#endif
  return -1;
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
    errs += extract_table("prog", c->prog, c->n_prog);
  if (!kind_filter || !strcmp(kind_filter, "all") || !strcmp(kind_filter, "bild"))
    errs += extract_table("bild", c->bild, c->n_bild);
  if (!kind_filter || !strcmp(kind_filter, "all") || !strcmp(kind_filter, "inv") ||
      !strcmp(kind_filter, "inventar"))
    errs += extract_table("inv", c->inv, c->n_inv);
  if (!kind_filter || !strcmp(kind_filter, "all") || !strcmp(kind_filter, "phase"))
    errs += extract_table("phase", c->phase, c->n_phase);

  if (errs)
    fprintf(stderr, "moas-util: %d extract error(s)\n", errs);
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


/* ---- script disassembler (prog resources) ---- */

typedef struct {
  int size;          /* fixed size in bytes including opcode; 0 = special */
  const char *name;
  const char *fmt;   /* printf-style over int16 params; NULL = dump raw shorts */
} OpInfo;

/* size: total bytes including opcode for sequential path. 0 = string (see insn_size). */
static const OpInfo op_table[128] = {
  [0]  = { 5,  "var_add_const",      "var[%d] += %d" },
  [1]  = { 5,  "var_add_var",        "var[%d] += var[%d]" },
  [2]  = { 16, "add_person",         NULL },
  [4]  = { 3,  "click_close",        "mode=%d" },
  [6]  = { 3,  "teak_effect",        "%d" },
  [7]  = { 1,  "close_window",       NULL },
  [10] = { 1,  "abort_redraw",       NULL },
  [12] = { 3,  "say_wait",           "%d" },
  [13] = { 17, "conditional_walk",   NULL },
  [14] = { 3,  "click_or_enter",     "mode=%d" },
  [15] = { 5,  "get_person_x",       "person=%d -> var[%d]" },
  [16] = { 5,  "get_person_y",       "person=%d -> var[%d]" },
  [17] = { 3,  "call_proc",          "proc=%d" },
  [18] = { 11, "if",                 NULL },
  [19] = { 5,  "set_ghost_level",    "person=%d level=%d" },
  [20] = { 13, "if_person_in",       NULL },
  [21] = { 5,  "if_wasnt_here",      "var=%d else_addr=%d" },
  [22] = { 5,  "goto_proc",          "proc=%d addr=%d" },
  [23] = { 1,  "arm_key_abort",      NULL },
  [26] = { 3,  "print_var",          "var[%d]" },
  [27] = { 11, "add_area_label",     NULL },
  [28] = { 11, "add_area_jumper",    NULL },
  [29] = { 1,  "clear_screen_init",  NULL },
  [30] = { 7,  "ontime_jump",        "t=%d proc=%d addr=%d" },
  [31] = { 3,  "open_window",        "%d" },
  [32] = { 5,  "var_rand_const",     "var[%d] = rand(%d)+1" },
  [33] = { 5,  "var_rand_var",       "var[%d] = rand(var[%d])+1" },
  [34] = { 5,  "set_person_z",       "person=%d z=%d" },
  [35] = { 3,  "remove_person",      "%d" },
  [38] = { 5,  "var_set_const",      "var[%d] = %d" },
  [39] = { 5,  "var_set_var",        "var[%d] = var[%d]" },
  [40] = { 5,  "set_object",         "obj=%d val=%d" },
  [41] = { 7,  "blend",              NULL },
  [42] = { 7,  "load_bild",          NULL },
  [43] = { 1,  "stop_all_walking",   NULL },
  [44] = { 0,  "print_sentence",     "text" },
  [45] = { 3,  "unmark_area",        "%d" },
  [46] = { 3,  "unmark_area2",       "%d" },
  [47] = { 3,  "wait",               "%d" },
  [48] = { 1,  "return_not_found",   NULL },
  [50] = { 1,  "return_found",       NULL },
  [51] = { 1,  "restart",            NULL },
  [52] = { 3,  "blend_in",           "%d" },
  [53] = { 3,  "blend_out",          "%d" },
  [54] = { 5,  "adress",             "%d %d" },
  [55] = { 1,  "wait_while_walking", NULL },
  [56] = { 13, "parse_if",           NULL },
  [57] = { 16, "set_person",         NULL },
  [58] = { 5,  "shake",              "%d %d" },
  [59] = { 1,  "init_ontimes",       NULL },
  [60] = { 0,  "if_eq_print",        "text" },
  [61] = { 5,  "execute",            "proc=%d addr=%d" },
  [62] = { 0,  "add_power_parse",    "text" },
  [63] = { 9,  "check_file",         NULL },
  [64] = { 0,  "display_message",    "text" },
  [65] = { 0,  "add_description",    "text" },
  [66] = { 0,  "add_say_window",     "text" },
  [67] = { 0,  "add_option",         "text" },
  [68] = { 3,  "add_dialog",         "%d" },
  [69] = { 1,  "init_dialogs",       NULL },
  [70] = { 3,  "remove_dialog",      "%d" },
  [71] = { 3,  "reset_dialog",       "%d" },
  [72] = { 5,  "select_from_dialog", "%d %d" },
  [73] = { 7,  "toggle_dialog_point","%d %d %d" },
  [74] = { 27, "if3",                NULL },
  [75] = { 3,  "toggle_screen",      "%d" },
  [76] = { 3,  "internal_error",     "code=%d" },
  [77] = { 1,  "packing_off",        NULL },
  [78] = { 5,  "select_from_dialog", "%d %d" },
  [79] = { 1,  "menu_return",        NULL },
  [80] = { 19, "if2",                NULL },
  [81] = { 0,  "add_say_window2",    "text" },
  [82] = { 13, "add_walk_area",      NULL },
  [83] = { 1,  "remove_statists",    NULL },
  [84] = { 0,  "print_sentence_plus","text" },
  [85] = { 9,  "say_properties",     NULL },
  [86] = { 3,  "close_say",          "%d" },
  [87] = { 9,  "blend2",             NULL },
  [88] = { 7,  "walk_person",        "p=%d x=%d y=%d" },
  [89] = { 0,  "code_text",          "text" },
  [90] = { 0,  "print_sentence_ret", "text" },
  [91] = { 0,  "set_room_name",      "text" },
  [92] = { 0,  "say_if",              "text" },
  [93] = { 7,  "palette_cycle",      NULL },
  [94] = { 1,  "look_keys",          NULL },
  [95] = { 3,  "load_music",         "%d" },
  [96] = { 7,  "iwalk",              NULL },
  [97] = { 0,  "say_if_plus",         "text" },
  [98] = { 0,  "say_if_plus2",        "text" },
  [99] = { 0,  "say_if_found",       "text" },
};

static int16_t rd_i16(const uint8_t *p)
{
  return (int16_t)(p[0] | (p[1] << 8));
}

/* String ops: payload after opcode is bytes until NUL; total size = base + strlen.
 * base is bytes before the C string (including opcode). */
static int string_insn_size(const uint8_t *code, int len, int pc, int base_before_string)
{
  int i = pc + base_before_string;
  if (i >= len) return -1;
  while (i < len && code[i] != 0) i++;
  if (i >= len) return -1;
  return (i + 1) - pc; /* include NUL */
}

static int insn_size(const uint8_t *code, int len, int pc)
{
  uint8_t op;
  int sz;
  if (pc >= len) return -1;
  op = code[pc];
  if (op >= 128 || op_table[op].name == NULL) {
    return 1;
  }
  sz = op_table[op].size;
  if (sz > 0) return sz;
  /* Variable: Adresse += base + strlen(string); string starts after base-1 bytes from pc
   * (base includes the opcode). */
  switch (op) {
    case 44: /* +=2+strlen, string at Parameter[0] = pc+1 — wait: +=2+strlen means base 2 */
    case 64: /* +=2+strlen */
    case 84:
    case 91:
      return string_insn_size(code, len, pc, 1); /* string at pc+1; size=2+strlen => base_before=1 includes only op, +strlen+NUL = 2+strlen if string at pc+1 */
    case 60: /* +=6+strlen — short, short, string */
      return string_insn_size(code, len, pc, 5);
    case 62: /* +=4+strlen */
    case 65:
    case 89:
      return string_insn_size(code, len, pc, 3);
    case 66: /* +=8+strlen */
    case 81:
      return string_insn_size(code, len, pc, 7);
    case 67: /* +=10+strlen */
      return string_insn_size(code, len, pc, 9);
    case 90: /* string at Parameter, then return */
      return string_insn_size(code, len, pc, 1);
    case 92: /* fall through to 99: Adresse+=8+strlen(Parameter[3]) */
    case 97:
    case 98:
    case 99:
      return string_insn_size(code, len, pc, 7);
    default:
      return 1;
  }
}

static void print_cp850_str(const uint8_t *s, int max)
{
  int i;
  putchar('"');
  for (i = 0; s[i] && i < max; i++) {
    unsigned char c = s[i];
    if (c >= 32 && c < 127 && c != '"' && c != '\\') putchar((char)c);
    else if (c == '\n') fputs("\\n", stdout);
    else if (c == 0x81) fputs("ü", stdout);
    else if (c == 0x84) fputs("ä", stdout);
    else if (c == 0x8e) fputs("Ä", stdout);
    else if (c == 0x94) fputs("ö", stdout);
    else if (c == 0x99) fputs("Ö", stdout);
    else if (c == 0x9a) fputs("Ü", stdout);
    else if (c == 0xe1) fputs("ß", stdout);
    else printf("\\x%02x", c);
  }
  putchar('"');
}

static void disasm_buffer(const uint8_t *code, int len, int prog_index)
{
  int pc = 0;
  printf("; program %d  %d bytes\n", prog_index, len);
  while (pc < len) {
    uint8_t op = code[pc];
    int sz = insn_size(code, len, pc);
    int i, nshort;
    const OpInfo *info = (op < 128) ? &op_table[op] : NULL;

    printf("%04x: ", pc);
    if (sz < 1) {
      printf("db 0x%02x  ; truncated\n", op);
      break;
    }
    for (i = 0; i < sz && i < 12; i++)
      printf("%02x ", code[pc + i]);
    for (; i < 12; i++)
      printf("   ");

    if (!info || !info->name) {
      /* Likely padding or a size error; show rest as data once. */
      printf("; non-code/unknown 0x%02x — remaining %d bytes (padding or desync)\n",
             op, len - pc);
      printf("; ");
      {
        int j;
        for (j = pc; j < len && j < pc + 32; j++)
          printf("%02x ", code[j]);
        if (len - pc > 32) printf("...");
        printf("\n");
      }
      break;
    }

    printf("%s", info->name);
    nshort = (sz - 1) / 2;
    if (info->size == 0) {
      /* string-ish */
      int off = 1;
      if (op == 60) off = 5;
      if (op == 62 || op == 65 || op == 89) off = 3;
      if (op == 66 || op == 81) off = 7;
      if (op == 67) off = 9;
      if (op == 92 || op == 97 || op == 98 || op == 99) off = 7;
      if (nshort >= 1 && off >= 3)
        printf("  %d", (int)rd_i16(code + pc + 1));
      if (pc + off < len) {
        char ubuf[400];
        unpack_to(ubuf, sizeof(ubuf), code + pc + off);
        putchar(' ');
        print_cp850_str((const uint8_t *)ubuf, (int)strlen(ubuf));
      }
    } else if (info->fmt && nshort > 0) {
      int16_t a = 0, b = 0, c = 0, d = 0;
      if (nshort >= 1) a = rd_i16(code + pc + 1);
      if (nshort >= 2) b = rd_i16(code + pc + 3);
      if (nshort >= 3) c = rd_i16(code + pc + 5);
      if (nshort >= 4) d = rd_i16(code + pc + 7);
      printf("  ");
      printf(info->fmt, a, b, c, d);
    } else if (nshort > 0) {
      printf("  ");
      for (i = 0; i < nshort && i < 8; i++)
        printf("%d%s", (int)rd_i16(code + pc + 1 + i * 2),
               i + 1 < nshort && i + 1 < 8 ? ", " : "");
    }
    putchar('\n');
    pc += sz;
  }
}

static int load_prog_blob(const Catalog *cat, int idx, uint8_t **out, int *out_len)
{
  char srcname[32];
  const MapFileDisk *e;
  int fd;
  uint8_t *buf;
  ssize_t n;

  if (idx < 0 || idx >= cat->n_prog) return -1;
  e = &cat->prog[idx];
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

static void cmd_disasm(const Catalog *cat, int prog_index)
{
  uint8_t *buf = NULL;
  int len = 0;
  if (prog_index < 0) {
    int i;
    for (i = 0; i < cat->n_prog; i++) {
      if (load_prog_blob(cat, i, &buf, &len) == 0 && buf) {
        disasm_buffer(buf, len, i);
        putchar('\n');
        free(buf);
        buf = NULL;
      }
    }
    return;
  }
  if (load_prog_blob(cat, prog_index, &buf, &len) != 0 || !buf)
    die("cannot load program");
  disasm_buffer(buf, len, prog_index);
  free(buf);
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

/* Byte offset of packed C-string relative to opcode (op at pc). */
static int string_param_off(uint8_t op)
{
  switch (op) {
    case 44: case 64: case 84: case 90: case 91: return 1;
    case 62: case 65: case 89: return 3;
    case 60: return 5;
    case 66: case 81: case 92: case 97: case 98: case 99: return 7;
    case 67: return 9;
    default: return -1;
  }
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
  if (!r->id || !r->text || !r->ref) die("out of memory");
}

static void extract_prog_strings(const Catalog *cat, int prog_index)
{
  uint8_t *buf = NULL;
  int len = 0, pc = 0;
  if (load_prog_blob(cat, prog_index, &buf, &len) != 0 || !buf)
    return;
  while (pc < len) {
    uint8_t op = buf[pc];
    int sz = insn_size(buf, len, pc);
    int soff = string_param_off(op);
    if (sz < 1) break;
    if (soff >= 0 && pc + soff < len) {
      const uint8_t *packed = buf + pc + soff;
      char unpacked[512], utf8[1024], id[64], ref[96];
      const OpInfo *info = (op < 128) ? &op_table[op] : NULL;
      unpack_to(unpacked, sizeof(unpacked), packed);
      if (unpacked[0]) {
        cp850_to_utf8((const uint8_t *)unpacked, utf8, sizeof(utf8));
        snprintf(id, sizeof(id), "script.p%d.0x%04x", prog_index, pc);
        snprintf(ref, sizeof(ref), "prog %d @ %04x op %s",
                 prog_index, pc, info && info->name ? info->name : "?");
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
  load_pack_codes();
  for (i = 0; i < g_anz_codes; i++) {
    if (!g_codes[i] || !g_codes[i][0]) continue;
    cp850_to_utf8((const uint8_t *)g_codes[i], utf8, sizeof(utf8));
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
    { "ui.overlay.esc", "ESC" },
    { "ui.overlay.skip", "SKIP" },
    { "ui.overlay.use", "USE" },
    { "ui.overlay.look", "LOOK" },
    { "ui.overlay.menu", "MENU" },
    { "ui.overlay.hint", "HINT" },
    { "ui.menu.options", "Optionen (F12)" },
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
  for (i = 0; i < cat->n_prog; i++)
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
    "Inspect and extract MOAS / Mänick game data (GAME.* files).\n"
    "\n"
    "Options:\n"
    "  --datadir DIR   Directory with GAME.1, GAME.2, … (default: . or $MOAS_DATADIR)\n"
    "  --outdir DIR    Extract output directory (default: extracted)\n"
    "  -h, --help      This help\n"
    "\n"
    "Commands:\n"
    "  info              File sizes and catalog counts\n"
    "  list              List every catalog entry in GAME.2\n"
    "  extract [KIND]    Write raw resource blobs (KIND: all|prog|bild|inv|phase)\n"
    "  licence           Print LICENCE.DAT text\n"
    "  xxx               Summarise GAME.XXX font tables\n"
    "  disasm [N]        Disassemble script program N (default: all)\n"
    "  strings [FMT]     Extract translatable text (json|json-map|pot|tsv, default json)\n"
    "\n"
    "Examples:\n"
    "  %s --datadir manick info\n"
    "  %s --datadir manick list | less\n"
    "  %s --datadir manick extract --outdir /tmp/moas-out bild\n"
    "  %s --datadir manick disasm 0\n"
    "  %s --datadir manick strings pot > manick.pot\n"
    "  %s --datadir manick strings json > manick.de.json\n",
    argv0, argv0, argv0, argv0, argv0, argv0, argv0);
}

int main(int argc, char **argv)
{
  const char *cmd = NULL;
  const char *extract_kind = "all";
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
      continue;
    }
    if (!strncmp(argv[i], "--outdir=", 9)) {
      g_outdir = argv[i] + 9;
      continue;
    }
    if (argv[i][0] == '-') {
      fprintf(stderr, "moas-util: unknown option '%s'\n", argv[i]);
      usage(argv[0]);
      return 1;
    }
    if (!cmd) {
      cmd = argv[i];
    } else if ((!strcmp(cmd, "extract") || !strcmp(cmd, "disasm") || !strcmp(cmd, "strings"))
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
  if (!strcmp(cmd, "list") || !strcmp(cmd, "extract") || !strcmp(cmd, "disasm")
      || !strcmp(cmd, "strings")) {
    int prog_idx = -1;
    cat = load_catalog();
    if (!strcmp(cmd, "list"))
      cmd_list(&cat);
    else if (!strcmp(cmd, "extract"))
      cmd_extract(&cat, extract_kind);
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
