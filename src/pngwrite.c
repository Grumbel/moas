/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "pngwrite.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t g_crc_table[256];

static uint32_t crc32_update(uint32_t crc, const uint8_t *p, size_t n)
{
  size_t i;
  if (!g_crc_table[1]) {
    uint32_t k, j, v;
    for (k = 0; k < 256; k++) {
      v = k;
      for (j = 0; j < 8; j++) v = (v & 1) ? 0xedb88320u ^ (v >> 1) : v >> 1;
      g_crc_table[k] = v;
    }
  }
  for (i = 0; i < n; i++) crc = g_crc_table[(crc ^ p[i]) & 0xff] ^ (crc >> 8);
  return crc;
}

static void put_be32(uint8_t *p, uint32_t v)
{
  p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
  p[2] = (uint8_t)(v >> 8);  p[3] = (uint8_t)v;
}


static int png_chunk(FILE *fp, const char *type, const uint8_t *data, size_t n)
{
  uint8_t hdr[8], tail[4];
  uint32_t crc;
  put_be32(hdr, (uint32_t)n);
  memcpy(hdr + 4, type, 4);
  crc = crc32_update(0xffffffffu, hdr + 4, 4);
  crc = crc32_update(crc, data, n) ^ 0xffffffffu;
  put_be32(tail, crc);
  return fwrite(hdr, 1, 8, fp) == 8 && (n == 0 || fwrite(data, 1, n, fp) == n) &&
         fwrite(tail, 1, 4, fp) == 4 ? 0 : -1;
}

/* zlib stream uses stored (uncompressed) deflate blocks. */
int png_write_indexed(const char *path, const uint8_t *pix, int w, int h,
                      const uint8_t *pal, const uint8_t *alpha)
{
  static const uint8_t sig[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n' };
  uint8_t ihdr[13];
  size_t raw_len = (size_t)(w + 1) * (size_t)h;
  size_t nblocks = raw_len / 65535 + 1;
  size_t zlen = 2 + raw_len + nblocks * 5 + 4;
  uint8_t *z = malloc(zlen), *raw = malloc(raw_len);
  size_t i, o = 0, pos = 0;
  uint32_t s1 = 1, s2 = 0;
  FILE *fp;
  int y, rc, ntrns = 0;

  if (!z || !raw) {
    free(z); free(raw);
    errno = ENOMEM;
    return -1;
  }
  for (y = 0; y < h; y++) {
    raw[(size_t)y * (w + 1)] = 0; /* filter: none */
    memcpy(raw + (size_t)y * (w + 1) + 1, pix + (size_t)y * w, (size_t)w);
  }
  z[o++] = 0x78; z[o++] = 0x01;
  do {
    size_t n = raw_len - pos > 65535 ? 65535 : raw_len - pos;
    z[o++] = (pos + n == raw_len) ? 1 : 0;
    z[o++] = (uint8_t)n; z[o++] = (uint8_t)(n >> 8);
    z[o++] = (uint8_t)~n; z[o++] = (uint8_t)(~n >> 8);
    memcpy(z + o, raw + pos, n);
    o += n;
    pos += n;
  } while (pos < raw_len);
  for (i = 0; i < raw_len; i++) {
    s1 = (s1 + raw[i]) % 65521;
    s2 = (s2 + s1) % 65521;
  }
  put_be32(z + o, (s2 << 16) | s1);
  o += 4;

  put_be32(ihdr, (uint32_t)w);
  put_be32(ihdr + 4, (uint32_t)h);
  ihdr[8] = 8; ihdr[9] = 3; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
  if (alpha)
    for (i = 0; i < 256; i++)
      if (alpha[i] != 255) ntrns = (int)i + 1;

  fp = fopen(path, "wb");
  if (!fp) {
    free(z); free(raw);
    return -1;
  }
  errno = 0;
  rc = fwrite(sig, 1, 8, fp) == 8 ? 0 : -1;
  if (!rc) rc = png_chunk(fp, "IHDR", ihdr, 13);
  if (!rc) rc = png_chunk(fp, "PLTE", pal, 768);
  if (!rc && ntrns) rc = png_chunk(fp, "tRNS", alpha, (size_t)ntrns);
  if (!rc) rc = png_chunk(fp, "IDAT", z, o);
  if (!rc) rc = png_chunk(fp, "IEND", NULL, 0);
  if (fclose(fp) != 0) rc = -1;
  if (rc && !errno) errno = EIO;
  free(z);
  free(raw);
  return rc;
}
