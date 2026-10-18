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
  if (mkdir(path, 0755) == 0) return 0;
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
    "\n"
    "Examples:\n"
    "  %s --datadir manick info\n"
    "  %s --datadir manick list | less\n"
    "  %s --datadir manick extract --outdir /tmp/moas-out bild\n",
    argv0, argv0, argv0, argv0);
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
    } else if (!strcmp(cmd, "extract") && !strcmp(extract_kind, "all")) {
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
  if (!strcmp(cmd, "list") || !strcmp(cmd, "extract")) {
    cat = load_catalog();
    if (!strcmp(cmd, "list"))
      cmd_list(&cat);
    else
      cmd_extract(&cat, extract_kind);
    free_catalog(&cat);
    return 0;
  }

  fprintf(stderr, "moas-util: unknown command '%s'\n", cmd);
  usage(argv[0]);
  return 1;
}
