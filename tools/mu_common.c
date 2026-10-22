/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "mu_common.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#if defined(_WIN32)
#include <direct.h>
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

const char *const kind_names[K_COUNT] = { "prog", "bild", "inv", "phase" };
const char *g_datadir = ".";

void die(const char *fmt, ...)
{
  va_list ap;
  fputs("moas-util: ", stderr);
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);
  exit(1);
}

void die_errno(const char *what)
{
  die("%s: %s", what, strerror(errno));
}

void *xmalloc(size_t n)
{
  void *p = malloc(n ? n : 1);
  if (!p) die("out of memory");
  return p;
}

void *xrealloc(void *p, size_t n)
{
  p = realloc(p, n ? n : 1);
  if (!p) die("out of memory");
  return p;
}

char *xstrdup(const char *s)
{
  size_t n = strlen(s) + 1;
  char *p = xmalloc(n);
  memcpy(p, s, n);
  return p;
}

int path_join(char *out, size_t outsz, const char *dir, const char *name)
{
  int n = snprintf(out, outsz, "%s/%s", dir, name);
  return n > 0 && (size_t)n < outsz;
}

static int mkdir_one(const char *path)
{
  struct stat st;
  if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) return 0;
#if defined(_WIN32)
  return _mkdir(path);
#else
  return mkdir(path, 0755);
#endif
}

int mkdir_p(const char *path)
{
  char buf[1024];
  size_t i, n = strlen(path);
  if (n >= sizeof(buf)) return -1;
  memcpy(buf, path, n + 1);
  for (i = 1; i < n; i++)
    if (buf[i] == '/') {
      buf[i] = 0;
      mkdir_one(buf);
      buf[i] = '/';
    }
  return mkdir_one(buf);
}

int open_in(const char *dir, const char *name)
{
  char path[1024], folded[256];
  size_t i, n = strlen(name);
  int fd;
  if (!path_join(path, sizeof(path), dir, name)) die("path too long");
  fd = open(path, O_RDONLY | O_BINARY);
  if (fd >= 0 || n >= sizeof(folded)) return fd;
  /* case fold basename-style attempts */
  for (i = 0; i <= n; i++) folded[i] = (char)toupper((unsigned char)name[i]);
  if (path_join(path, sizeof(path), dir, folded) && (fd = open(path, O_RDONLY | O_BINARY)) >= 0)
    return fd;
  for (i = 0; i <= n; i++) folded[i] = (char)tolower((unsigned char)name[i]);
  if (path_join(path, sizeof(path), dir, folded))
    fd = open(path, O_RDONLY | O_BINARY);
  return fd;
}

int open_data(const char *name)
{
  return open_in(g_datadir, name);
}

static uint8_t *read_fd(int fd, long *len)
{
  struct stat st;
  uint8_t *buf;
  long got = 0;
  if (fstat(fd, &st) != 0) { close(fd); return NULL; }
  buf = xmalloc((size_t)st.st_size + 1);
  while (got < (long)st.st_size) {
    ssize_t n = read(fd, buf + got, (size_t)(st.st_size - got));
    if (n <= 0) { close(fd); free(buf); return NULL; }
    got += n;
  }
  close(fd);
  buf[got] = 0;
  *len = got;
  return buf;
}

uint8_t *read_file(const char *path, long *len)
{
  int fd = open(path, O_RDONLY | O_BINARY);
  return fd < 0 ? NULL : read_fd(fd, len);
}

uint8_t *read_in(const char *dir, const char *name, long *len)
{
  int fd = open_in(dir, name);
  return fd < 0 ? NULL : read_fd(fd, len);
}

void write_file(const char *path, const void *data, long len)
{
  FILE *fp = fopen(path, "wb");
  if (!fp) die("cannot write %s: %s", path, strerror(errno));
  if ((len > 0 && fwrite(data, 1, (size_t)len, fp) != (size_t)len) | (fclose(fp) != 0))
    die("short write %s", path);
}

uint32_t moas_chk(const uint8_t *p, long n)
{
  uint8_t b1 = 0, b2 = 0, b3 = 0, b4 = 0;
  long i;
  for (i = 0; i < n; i++) {
    b1 += p[i];
    b2 ^= p[i];
    b3++;
    b4 ^= (uint8_t)(p[i] + b3);
  }
  return b1 | ((uint32_t)b2 << 8) | ((uint32_t)b3 << 16) | ((uint32_t)b4 << 24);
}

Catalog catalog_parse(const uint8_t *g2, long len)
{
  Catalog c;
  long need = 8;
  int k;
  memset(&c, 0, sizeof(c));
  if (len < 8) die("GAME.2 truncated");
  for (k = 0; k < K_COUNT; k++) {
    c.n[k] = (int16_t)(g2[2 * k] | (g2[2 * k + 1] << 8));
    if (c.n[k] < 0) die("GAME.2: negative counts");
    need += (long)c.n[k] * (long)sizeof(MapFileDisk);
  }
  if (need > len) die("GAME.2 truncated");
  need = 8;
  for (k = 0; k < K_COUNT; k++) {
    size_t bytes = (size_t)c.n[k] * sizeof(MapFileDisk);
    c.e[k] = xmalloc(bytes);
    memcpy(c.e[k], g2 + need, bytes);
    need += (long)bytes;
  }
  return c;
}

Catalog load_catalog_in(const char *dir)
{
  long len;
  uint8_t *g2 = read_in(dir, "GAME.2", &len);
  Catalog c;
  if (!g2) die("cannot open %s/GAME.2 (set --datadir to manick/)", dir);
  c = catalog_parse(g2, len);
  free(g2);
  return c;
}

Catalog load_catalog(void)
{
  return load_catalog_in(g_datadir);
}

void free_catalog(Catalog *c)
{
  int k;
  for (k = 0; k < K_COUNT; k++) free(c->e[k]);
  memset(c, 0, sizeof(*c));
}

int bild_has_palette(const MapFileDisk *e)
{
  return (e->x & 16384) && e->Length > 768;
}

int gfx_width(int kind, const MapFileDisk *e)
{
  switch (kind) {
    case K_BILD:  return e->x & ~16384;
    case K_INV:   return 32;
    case K_PHASE: return e->x;
    default:      return 0;
  }
}

int gfx_height(int kind, const MapFileDisk *e)
{
  switch (kind) {
    case K_BILD:  return e->y;
    case K_INV:   return 24;
    case K_PHASE: return e->y * (e->Anz > 0 ? e->Anz : 1); /* frames stacked */
    default:      return 0;
  }
}

long gfx_payload(int kind, const MapFileDisk *e)
{
  return (kind == K_BILD && bild_has_palette(e)) ? e->Length - 768 : e->Length;
}

/* ---- CP850 <-> UTF-8 ------------------------------------------------- */

static const char *const cp850_hi[128] = {
  "Ç","ü","é","â","ä","à","å","ç","ê","ë","è","ï","î","ì","Ä","Å",
  "É","æ","Æ","ô","ö","ò","û","ù","ÿ","Ö","Ü","ø","£","Ø","×","ƒ",
  "á","í","ó","ú","ñ","Ñ","ª","º","¿","®","¬","½","¼","¡","«","»",
  "░","▒","▓","│","┤","Á","Â","À","©","╣","║","╗","╝","¢","¥","┐",
  "└","┴","┬","├","─","┼","ã","Ã","╚","╔","╩","╦","╠","═","╬","¤",
  "ð","Ð","Ê","Ë","È","ı","Í","Î","Ï","┘","┌","█","▄","¦","Ì","▀",
  "Ó","ß","Ô","Ò","õ","Õ","µ","þ","Þ","Ú","Û","Ù","ý","Ý","¯","´",
  NULL,"±","‗","¾","¶","§","÷","¸","°","¨","·","¹","³","²","■",NULL,
};

void put_cp850(FILE *fp, const uint8_t *s, size_t n)
{
  size_t i;
  fputc('"', fp);
  for (i = 0; i < n; i++) {
    uint8_t c = s[i];
    if (c == '"' || c == '\\') { fputc('\\', fp); fputc(c, fp); }
    else if (c >= 32 && c < 127) fputc(c, fp);
    else if (c >= 128 && cp850_hi[c - 128]) fputs(cp850_hi[c - 128], fp);
    else fprintf(fp, "\\x%02x", c);   /* control, soft hyphen, nbsp */
  }
  fputc('"', fp);
}

static int hexval(int c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

int parse_cp850_string(const char **pp, uint8_t *out, size_t outsz, const char **err)
{
  const char *p = *pp;
  size_t n = 0;
  if (*p != '"') { *err = "expected '\"'"; return -1; }
  p++;
  for (;;) {
    int byte = -1;
    if (*p == 0 || *p == '\n') { *err = "unterminated string"; return -1; }
    if (*p == '"') { p++; break; }
    if (*p == '\\') {
      p++;
      if (*p == '"' || *p == '\\') byte = (uint8_t)*p++;
      else if (*p == 'x' && hexval(p[1]) >= 0 && hexval(p[2]) >= 0) {
        byte = hexval(p[1]) * 16 + hexval(p[2]);
        p += 3;
      } else { *err = "bad escape (use \\\" \\\\ or \\xNN)"; return -1; }
    } else if ((uint8_t)*p < 128) {
      byte = (uint8_t)*p++;
    } else {
      int i;
      for (i = 0; i < 128; i++) {
        size_t l;
        if (!cp850_hi[i]) continue;
        l = strlen(cp850_hi[i]);
        if (!strncmp(p, cp850_hi[i], l)) { byte = 128 + i; p += l; break; }
      }
      if (byte < 0) { *err = "character not in code page 850"; return -1; }
    }
    if (n + 1 >= outsz) { *err = "string too long"; return -1; }
    out[n++] = (uint8_t)byte;
  }
  out[n] = 0;
  *pp = p;
  return (int)n;
}
