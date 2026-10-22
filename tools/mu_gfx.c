/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Packed graphics format, as read by UniversalUnpack:
 *
 *   header byte (always 4), N, N x (code, colour), then a token stream:
 *     code len       run: colour x (len+3), len 255 means 516
 *     251 lo hi      copy (f>>11)+16 pixels from (f&2047)+len back
 *     252/253/254    copy (f>>14)+12/8/4 pixels from (f&16383)+len back
 *     255 lo hi len  copy len pixels from absolute offset f
 *     248..250       copy two pixels from the row(s) above
 *     other          literal pixel
 *   Codes are pixel values the image doesn't use, so they can't be
 *   literals; neither can 248..255.
 *
 * The original packer is lost. gfx_encode() follows its observed rules:
 * longest earlier match (earliest one on ties) ending at least two pixels
 * before the current one and at least 7 back, encoded relative if it fits,
 * else absolute; a run wins if it covers at least as many pixels per byte.
 * Code bytes are the lowest unused values, assigned to colours in order of
 * first use. That reproduces 98 of the 235 Mänick graphics byte for byte;
 * the rest come out slightly different but decode to the same pixels.
 */
#include "mu_gfx.h"
#include "mu_common.h"

#include <stdlib.h>
#include <string.h>

long gfx_decode(const uint8_t *src, long size, uint8_t *dst, long cap, long xs)
{
  /* Abk[] in grafix.c: { xp1, yp1, xp2, yp2 } */
  static const int abk[3][4] = { { 1, -1, 1, -1 }, { 0, -2, 0, -2 }, { 0, -1, 0, -1 } };
  short codes[256];
  long c, t = 0, i;
  int n;

  if (size < 1) return -1;
  for (i = 0; i < 256; i++) codes[i] = -1;
  n = src[0];
  if (1 + 2 * n > size) return -1;
  for (i = 0; i < n; i++)
    codes[src[1 + 2 * i]] = src[2 + 2 * i];
  c = 1 + 2 * n;

  while (c < size) {
    uint8_t b = src[c];
    if (b >= 248) {
      if (b <= 250) {
        const int *a = abk[b - 248];
        long s1 = t + xs * a[1] + a[0], s2 = t + xs * a[3] + a[2] + 1;
        if (s1 < 0 || s2 < 0 || s1 >= cap || s2 >= cap || t + 2 > cap) return -1;
        dst[t] = dst[s1];
        dst[t + 1] = dst[s2];
        t += 2;
        c += 1;
      } else if (b == 255) {
        long f, len;
        if (c + 4 > size) return -1;
        f = src[c + 1] | (src[c + 2] << 8);
        len = src[c + 3];
        if (f + len > t || t + len > cap) return -1;
        for (i = 0; i < len; i++) dst[t + i] = dst[f + i];
        t += len;
        c += 4;
      } else {
        long f, len, dist;
        if (c + 3 > size) return -1;
        f = src[c + 1] | (src[c + 2] << 8);
        switch (b) {
          case 251: len = (f >> 11) + 16; dist = (f & 2047) + len;  break;
          case 252: len = (f >> 14) + 12; dist = (f & 16383) + len; break;
          case 253: len = (f >> 14) + 8;  dist = (f & 16383) + len; break;
          default:  len = (f >> 14) + 4;  dist = (f & 16383) + len; break;
        }
        if (t - dist < 0 || t + len > cap) return -1;
        for (i = 0; i < len; i++) dst[t + i] = dst[t - dist + i];
        t += len;
        c += 3;
      }
    } else if (codes[b] != -1) {
      long len;
      if (c + 2 > size) return -1;
      len = (src[c + 1] != 255) ? src[c + 1] + 3 : 516;
      if (t + len > cap) return -1;
      memset(dst + t, codes[b], (size_t)len);
      t += len;
      c += 2;
    } else {
      if (t >= cap) return -1;
      dst[t++] = b;
      c++;
    }
  }
  return t;
}

/* ---- encoder --------------------------------------------------------- */

enum { T_LIT, T_RUN, T_REL, T_ABS };

typedef struct {
  uint8_t kind, value;   /* value: pixel (LIT, RUN) or opcode (REL) */
  long len, arg;         /* arg: distance (REL) or offset (ABS) */
} Token;

typedef struct {
  uint8_t *buf;
  long len, cap;
} Out;

static void out_byte(Out *o, int b)
{
  if (o->len >= o->cap) {
    o->cap = o->cap ? o->cap * 2 : 4096;
    o->buf = xrealloc(o->buf, (size_t)o->cap);
  }
  o->buf[o->len++] = (uint8_t)b;
}

static long run_at(const uint8_t *t, long n, long p)
{
  long r = 1;
  while (p + r < n && t[p + r] == t[p]) r++;
  return r;
}

/* Longest earlier match at p; source ends at least 2 before p, starts at
 * least 7 before p. */
static long find_match(const uint8_t *t, long n, long p, long *src)
{
  long i, best = 0;
  for (i = 0; i <= p - 7; i++) {
    long len = 0, max = p - i - 2;
    if (max > 255) max = 255;
    if (max <= best) break;   /* later sources only get shorter limits */
    while (len < max && p + len < n && t[i + len] == t[p + len]) len++;
    if (len > best) { best = len; *src = i; }
  }
  return best;
}

static const char *tokenize(const uint8_t *t, long n, const int has_code[256],
                            Token **out, long *ntok)
{
  Token *tok = NULL;
  long k = 0, cap = 0, p = 0;
  while (p < n) {
    long run = run_at(t, n, p), src = 0, len = find_match(t, n, p, &src), cost = 1;
    Token m;
    m.kind = T_LIT; m.value = t[p]; m.len = 1; m.arg = 0;
    if (len >= 4) {
      long d = p - src;
      if (len <= 15 && d - len <= 16383) {
        m.kind = T_REL; m.value = len <= 7 ? 254 : len <= 11 ? 253 : 252;
        m.len = len; m.arg = d; cost = 3;
      } else if (len >= 16 && len <= 47 && d - len <= 2047) {
        m.kind = T_REL; m.value = 251; m.len = len; m.arg = d; cost = 3;
      } else if (src < 65536) {
        m.kind = T_ABS; m.len = len; m.arg = src; cost = 4;
      }
    }
    if (has_code[t[p]] && run >= 3) {
      long r = run >= 516 ? 516 : run > 257 ? 257 : run;
      /* run wins on pixels per byte: r/2 >= len/cost */
      if (r * cost >= m.len * 2 || m.kind == T_LIT) {
        m.kind = T_RUN; m.value = t[p]; m.len = r; m.arg = 0;
      }
    }
    if (m.kind == T_LIT && t[p] >= 248) {
      static char msg[96];
      free(tok);
      snprintf(msg, sizeof(msg), "pixel %ld has colour %d; colours 248-255 need runs of 3 or more",
               p, t[p]);
      return msg;
    }
    if (k >= cap) {
      cap = cap ? cap * 2 : 1024;
      tok = xrealloc(tok, (size_t)cap * sizeof(Token));
    }
    tok[k++] = m;
    p += m.len;
  }
  *out = tok;
  *ntok = k;
  return NULL;
}

const char *gfx_encode(const uint8_t *pix, long n, uint8_t **out, long *outlen)
{
  int used[256] = { 0 }, has_code[256] = { 0 }, code_of[256];
  long runpix[256] = { 0 }, p, k, ntok;
  int unused[256], nunused = 0, ncand = 0, cand[256], i, ncodes = 0;
  Token *tok;
  const char *err;
  Out o = { NULL, 0, 0 };

  for (p = 0; p < n; p++) used[pix[p]] = 1;
  for (i = 0; i < 248; i++)
    if (!used[i]) unused[nunused++] = i;
  for (p = 0; p < n; ) {
    long r = run_at(pix, n, p);
    if (r >= 3) runpix[pix[p]] += r;
    p += r;
  }
  /* Colours with runs may get a code; 248..255 first, they can't be
   * literals. There are only as many codes as unused pixel values. */
  for (i = 0; i < 256; i++)
    if (runpix[i]) cand[ncand++] = i;
  for (i = 0; i < ncand; i++) {
    int j, best = i;
    for (j = i + 1; j < ncand; j++) {
      int a = cand[j], b = cand[best];
      if ((a >= 248) > (b >= 248) || ((a >= 248) == (b >= 248) && runpix[a] > runpix[b])) best = j;
    }
    j = cand[i]; cand[i] = cand[best]; cand[best] = j;
  }
  for (i = 0; i < ncand && i < nunused; i++) has_code[cand[i]] = 1;

  err = tokenize(pix, n, has_code, &tok, &ntok);
  if (err) return err;

  /* Code table: colours in order of first use by a run. */
  for (i = 0; i < 256; i++) code_of[i] = -1;
  for (k = 0; k < ntok; k++)
    if (tok[k].kind == T_RUN && code_of[tok[k].value] < 0) {
      code_of[tok[k].value] = unused[ncodes];
      ncodes++;
    }
  if (ncodes > 255) { free(tok); return "too many colours with runs"; }

  out_byte(&o, GFX_HEADER);
  out_byte(&o, ncodes);
  {
    int c;
    for (c = 0; c < ncodes; c++)
      for (i = 0; i < 256; i++)
        if (code_of[i] == unused[c]) { out_byte(&o, unused[c]); out_byte(&o, i); }
  }
  for (k = 0; k < ntok; k++) {
    const Token *m = &tok[k];
    switch (m->kind) {
      case T_LIT:
        out_byte(&o, m->value);
        break;
      case T_RUN:
        out_byte(&o, code_of[m->value]);
        out_byte(&o, m->len == 516 ? 255 : (int)(m->len - 3));
        break;
      case T_REL: {
        long off = m->arg - m->len, f;
        switch (m->value) {
          case 251: f = ((m->len - 16) << 11) | off; break;
          case 252: f = ((m->len - 12) << 14) | off; break;
          case 253: f = ((m->len - 8) << 14) | off;  break;
          default:  f = ((m->len - 4) << 14) | off;  break;
        }
        out_byte(&o, m->value);
        out_byte(&o, (int)(f & 255));
        out_byte(&o, (int)(f >> 8));
        break;
      }
      default:
        out_byte(&o, 255);
        out_byte(&o, (int)(m->arg & 255));
        out_byte(&o, (int)(m->arg >> 8));
        out_byte(&o, (int)m->len);
    }
  }
  free(tok);
  *out = o.buf;
  *outlen = o.len;
  return NULL;
}
