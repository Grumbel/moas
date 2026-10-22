/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-2.0-or-later */
/* 8-bit indexed PNG reader (no zlib dependency), shared by the game
 * (language picture overrides) and moas-util. */
#include "pngread.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t get_be32(const uint8_t *p)
{
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

/* ---- inflate (RFC 1951), after Mark Adler's puff.c ------------------- */

typedef struct {
  const uint8_t *in;
  size_t inlen, inpos;
  uint32_t bitbuf;
  int bitcnt;
  uint8_t *out;
  size_t outlen, outcap;
  int err;
} Inflate;

typedef struct {
  short count[16];
  short symbol[288];
} Huffman;

static int inf_bits(Inflate *s, int need)
{
  uint32_t val = s->bitbuf;
  while (s->bitcnt < need) {
    if (s->inpos >= s->inlen) { s->err = 1; return 0; }
    val |= (uint32_t)s->in[s->inpos++] << s->bitcnt;
    s->bitcnt += 8;
  }
  s->bitbuf = val >> need;
  s->bitcnt -= need;
  return (int)(val & ((1u << need) - 1));
}

static void inf_put(Inflate *s, uint8_t b)
{
  if (s->outlen >= s->outcap) {
    size_t cap = s->outcap ? s->outcap * 2 : 65536;
    uint8_t *p = realloc(s->out, cap);
    if (!p) { s->err = 1; return; }
    s->out = p;
    s->outcap = cap;
  }
  s->out[s->outlen++] = b;
}

static int inf_decode(Inflate *s, const Huffman *h)
{
  int len, code = 0, first = 0, index = 0;
  for (len = 1; len < 16; len++) {
    int count;
    code |= inf_bits(s, 1);
    count = h->count[len];
    if (code - count < first) return h->symbol[index + (code - first)];
    index += count;
    first += count;
    first <<= 1;
    code <<= 1;
  }
  s->err = 1;
  return 0;
}

static int inf_construct(Huffman *h, const short *length, int n)
{
  short offs[16];
  int sym, len, left = 1;
  for (len = 0; len < 16; len++) h->count[len] = 0;
  for (sym = 0; sym < n; sym++) h->count[length[sym]]++;
  if (h->count[0] == n) return 0;
  for (len = 1; len < 16; len++) {
    left <<= 1;
    left -= h->count[len];
    if (left < 0) return left;
  }
  offs[1] = 0;
  for (len = 1; len < 15; len++) offs[len + 1] = offs[len] + h->count[len];
  for (sym = 0; sym < n; sym++)
    if (length[sym] != 0) h->symbol[offs[length[sym]]++] = (short)sym;
  return left;
}

static void inf_codes(Inflate *s, const Huffman *lencode, const Huffman *distcode)
{
  static const short lbase[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
  static const short lext[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
    3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
  static const short dbase[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
    257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
  static const short dext[30] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
    7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };
  int symbol;
  do {
    symbol = inf_decode(s, lencode);
    if (s->err) return;
    if (symbol < 256) inf_put(s, (uint8_t)symbol);
    else if (symbol > 256) {
      int len, dist;
      symbol -= 257;
      if (symbol >= 29) { s->err = 1; return; }
      len = lbase[symbol] + inf_bits(s, lext[symbol]);
      symbol = inf_decode(s, distcode);
      if (s->err || symbol >= 30) { s->err = 1; return; }
      dist = dbase[symbol] + inf_bits(s, dext[symbol]);
      if (s->err || (size_t)dist > s->outlen) { s->err = 1; return; }
      while (len-- && !s->err) inf_put(s, s->out[s->outlen - dist]);
    }
  } while (symbol != 256);
}

static int inflate_raw(Inflate *s)
{
  int last;
  do {
    int type;
    last = inf_bits(s, 1);
    type = inf_bits(s, 2);
    if (s->err) return -1;
    if (type == 0) {
      unsigned len;
      s->bitbuf = 0;
      s->bitcnt = 0;
      if (s->inpos + 4 > s->inlen) return -1;
      len = s->in[s->inpos] | (s->in[s->inpos + 1] << 8);
      if ((unsigned)(s->in[s->inpos + 2] | (s->in[s->inpos + 3] << 8)) != (~len & 0xffff)) return -1;
      s->inpos += 4;
      if (s->inpos + len > s->inlen) return -1;
      while (len-- && !s->err) inf_put(s, s->in[s->inpos++]);
    } else if (type == 1) {
      static Huffman lencode, distcode;
      static int built;
      if (!built) {
        short lengths[288];
        int sym;
        for (sym = 0; sym < 144; sym++) lengths[sym] = 8;
        for (; sym < 256; sym++) lengths[sym] = 9;
        for (; sym < 280; sym++) lengths[sym] = 7;
        for (; sym < 288; sym++) lengths[sym] = 8;
        inf_construct(&lencode, lengths, 288);
        for (sym = 0; sym < 30; sym++) lengths[sym] = 5;
        inf_construct(&distcode, lengths, 30);
        built = 1;
      }
      inf_codes(s, &lencode, &distcode);
    } else if (type == 2) {
      static const short order[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
      short lengths[320];
      Huffman lencode, distcode;
      int nlen = inf_bits(s, 5) + 257, ndist = inf_bits(s, 5) + 1, ncode = inf_bits(s, 4) + 4;
      int index;
      if (nlen > 286 || ndist > 30) return -1;
      for (index = 0; index < ncode; index++) lengths[order[index]] = (short)inf_bits(s, 3);
      for (; index < 19; index++) lengths[order[index]] = 0;
      if (inf_construct(&lencode, lengths, 19) != 0) return -1;
      index = 0;
      while (index < nlen + ndist) {
        int symbol = inf_decode(s, &lencode), len = 0;
        if (s->err) return -1;
        if (symbol < 16) { lengths[index++] = (short)symbol; continue; }
        if (symbol == 16) {
          if (index == 0) return -1;
          len = lengths[index - 1];
          symbol = 3 + inf_bits(s, 2);
        } else if (symbol == 17) symbol = 3 + inf_bits(s, 3);
        else symbol = 11 + inf_bits(s, 7);
        if (index + symbol > nlen + ndist) return -1;
        while (symbol--) lengths[index++] = (short)len;
      }
      if (lengths[256] == 0) return -1;
      if (inf_construct(&lencode, lengths, nlen) < 0) return -1;
      if (inf_construct(&distcode, lengths + nlen, ndist) < 0) return -1;
      inf_codes(s, &lencode, &distcode);
    } else return -1;
    if (s->err) return -1;
  } while (!last);
  return 0;
}

static int paeth(int a, int b, int c)
{
  int p = a + b - c, pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
  return (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c);
}

const char *png_read_indexed(const uint8_t *f, long len, int *w, int *h, uint8_t **pix,
                             uint8_t *pal, int *npal)
{
  static const uint8_t sig[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n' };
  long pos = 8;
  uint8_t *idat = NULL, *prev, *out;
  size_t idat_len = 0, stride;
  int width = 0, height = 0, depth = 0, have_ihdr = 0, x, y;
  Inflate s;

  if (len < 8 || memcmp(f, sig, 8)) return "not a PNG file";
  memset(pal, 0, 768);
  *npal = 0;
  while (pos + 12 <= len) {
    uint32_t n = get_be32(f + pos);
    const uint8_t *type = f + pos + 4, *data = f + pos + 8;
    if (n > (uint32_t)(len - pos - 12)) break;
    if (!memcmp(type, "IHDR", 4) && n >= 13) {
      width = (int)get_be32(data);
      height = (int)get_be32(data + 4);
      depth = data[8];
      if (data[9] != 3) { free(idat); return "not an indexed-colour PNG (save it as indexed/palette, 256 colours)"; }
      if (data[12] != 0) { free(idat); return "interlaced PNGs are not supported"; }
      if (depth != 1 && depth != 2 && depth != 4 && depth != 8) { free(idat); return "unsupported bit depth"; }
      have_ihdr = 1;
    } else if (!memcmp(type, "PLTE", 4)) {
      *npal = (int)(n / 3 > 256 ? 256 : n / 3);
      memcpy(pal, data, (size_t)*npal * 3);
    } else if (!memcmp(type, "IDAT", 4)) {
      uint8_t *p = realloc(idat, idat_len + n);
      if (!p) { free(idat); return "out of memory"; }
      idat = p;
      memcpy(idat + idat_len, data, n);
      idat_len += n;
    } else if (!memcmp(type, "IEND", 4)) break;
    pos += 12 + (long)n;
  }
  if (!have_ihdr || width <= 0 || height <= 0 || width > 65535 || height > 65535) { free(idat); return "missing or bad IHDR"; }
  if (idat_len < 6) { free(idat); return "missing image data"; }

  memset(&s, 0, sizeof(s));
  s.in = idat + 2;  /* skip zlib header; Adler-32 is not checked */
  s.inlen = idat_len - 2;
  if (inflate_raw(&s) != 0) { free(idat); free(s.out); return "corrupt compressed data"; }
  free(idat);

  stride = ((size_t)width * depth + 7) / 8;
  if (s.outlen < (stride + 1) * (size_t)height) { free(s.out); return "image data too short"; }
  prev = calloc(1, stride);
  out = malloc((size_t)width * height);
  if (!prev || !out) { free(prev); free(out); free(s.out); return "out of memory"; }
  for (y = 0; y < height; y++) {
    uint8_t *row = s.out + (stride + 1) * (size_t)y + 1;
    int filter = row[-1];
    size_t i;
    for (i = 0; i < stride; i++) {
      int a = i ? row[i - 1] : 0, b = prev[i], c = i ? prev[i - 1] : 0;
      switch (filter) {
        case 0: break;
        case 1: row[i] = (uint8_t)(row[i] + a); break;
        case 2: row[i] = (uint8_t)(row[i] + b); break;
        case 3: row[i] = (uint8_t)(row[i] + ((a + b) >> 1)); break;
        case 4: row[i] = (uint8_t)(row[i] + paeth(a, b, c)); break;
        default: free(prev); free(out); free(s.out); return "bad PNG filter type";
      }
    }
    for (x = 0; x < width; x++) {
      int bitpos = x * depth;
      out[(size_t)y * width + x] = (uint8_t)((row[bitpos >> 3] >> (8 - depth - (bitpos & 7))) & ((1 << depth) - 1));
    }
    memcpy(prev, row, stride);
  }
  free(prev);
  free(s.out);
  *w = width;
  *h = height;
  *pix = out;
  return NULL;
}
