/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "mu_script.h"
#include "mu_common.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

/* ---- GAME.PCK -------------------------------------------------------- */

/*
 * File layout (LoadPack in src/misc.c): header byte, then the pieces as
 * NUL-terminated strings, byte i (from 1) XORed with i-1, then the 4-byte
 * checksum of the header and the plain pieces.
 */
const char *dict_from_pck(Dict *d, const uint8_t *data, long len)
{
  uint8_t *plain;
  long c, start = 1;
  memset(d, 0, sizeof(*d));
  if (len < 5) return "GAME.PCK too short";
  len -= 4;
  plain = xmalloc((size_t)len);
  plain[0] = data[0];
  for (c = 1; c < len; c++) plain[c] = data[c] ^ (uint8_t)(c - 1);
  if (moas_chk(plain, len) != (uint32_t)(data[len] | (data[len + 1] << 8) |
                                         (data[len + 2] << 16) | ((uint32_t)data[len + 3] << 24))) {
    free(plain);
    return "GAME.PCK checksum mismatch";
  }
  if (len > 1 && plain[len - 1] != 0) { free(plain); return "GAME.PCK: last piece not terminated"; }
  d->header = plain[0];
  for (c = 1; c < len; c++)
    if (plain[c] == 0) {
      if (d->n >= 256) { free(plain); return "GAME.PCK: more than 256 pieces"; }
      d->piece[d->n++] = xstrdup((const char *)plain + start);
      start = c + 1;
    }
  free(plain);
  return NULL;
}

uint8_t *dict_to_pck(const Dict *d, long *len)
{
  long n = 1, c, i;
  uint8_t *buf;
  uint32_t chk;
  for (i = 0; i < d->n; i++) n += (long)strlen(d->piece[i]) + 1;
  buf = xmalloc((size_t)n + 4);
  buf[0] = d->header;
  for (c = 1, i = 0; i < d->n; i++) {
    size_t l = strlen(d->piece[i]) + 1;
    memcpy(buf + c, d->piece[i], l);
    c += (long)l;
  }
  chk = moas_chk(buf, n);
  for (c = 1; c < n; c++) buf[c] ^= (uint8_t)(c - 1);
  for (i = 0; i < 4; i++) buf[n + i] = (uint8_t)(chk >> (8 * i));
  *len = n + 4;
  return buf;
}

void dict_write_text(FILE *fp, const Dict *d)
{
  int i;
  fputs("; GAME.PCK: dictionary for script strings. A packed string is a list of\n"
        "; entry numbers (the first entry below is 0). Entry 0 is never used, a 0\n"
        "; byte ends the string. Script text may only use characters some entry\n"
        "; covers; the assembler picks the longest entry at each position.\n"
        "; At most 256 entries.\n", fp);
  fprintf(fp, "header %d\n", d->header);
  for (i = 0; i < d->n; i++) {
    put_cp850(fp, (const uint8_t *)d->piece[i], strlen(d->piece[i]));
    fprintf(fp, "  ; %d\n", i);
  }
}

static const char *skip_ws(const char *p)
{
  while (*p == ' ' || *p == '\t' || *p == '\r') p++;
  return p;
}

static int at_line_end(const char *p)
{
  p = skip_ws(p);
  return *p == 0 || *p == '\n' || *p == ';';
}

static const char *next_line(const char *p)
{
  while (*p && *p != '\n') p++;
  return *p ? p + 1 : p;
}

const char *dict_parse_text(Dict *d, const char *text, int *line)
{
  const char *p = text, *err;
  int have_header = 0;
  memset(d, 0, sizeof(*d));
  for (*line = 1; *p; p = next_line(p), (*line)++) {
    const char *q = skip_ws(p);
    if (at_line_end(q)) continue;
    if (!strncmp(q, "header", 6)) {
      char *end;
      long v = strtol(q + 6, &end, 10);
      if (v < 0 || v > 255 || !at_line_end(end)) return "bad header line";
      d->header = (uint8_t)v;
      have_header = 1;
    } else {
      uint8_t buf[256];
      if (parse_cp850_string(&q, buf, sizeof(buf), &err) < 0) return err;
      if (!at_line_end(q)) return "junk after string";
      if (d->n >= 256) return "more than 256 entries";
      d->piece[d->n++] = xstrdup((const char *)buf);
    }
  }
  return have_header ? NULL : "missing header line";
}

void dict_free(Dict *d)
{
  int i;
  for (i = 0; i < d->n; i++) free(d->piece[i]);
  memset(d, 0, sizeof(*d));
}

void dict_unpack(const Dict *d, const uint8_t *packed, char *out, size_t outsz)
{
  size_t used = 0;
  out[0] = 0;
  for (; *packed; packed++) {
    const char *piece = *packed < d->n ? d->piece[*packed] : "";
    size_t l = strlen(piece);
    if (used + l >= outsz) break;
    memcpy(out + used, piece, l + 1);
    used += l;
  }
}

int dict_pack(const Dict *d, const uint8_t *text, uint8_t *out, size_t outsz, int *bad)
{
  size_t n = 0, i = 0, len = strlen((const char *)text);
  while (i < len) {
    int k, best = -1;
    size_t bl = 0;
    for (k = 1; k < d->n; k++) {
      size_t l = strlen(d->piece[k]);
      if (l > bl && !strncmp((const char *)text + i, d->piece[k], l)) { best = k; bl = l; }
    }
    if (best < 0 || n + 1 >= outsz) { *bad = (int)i; return -1; }
    out[n++] = (uint8_t)best;
    i += bl;
  }
  out[n] = 0;
  return (int)n;
}

/* ---- opcode table ---------------------------------------------------- */

/*
 * Operand kinds: w short, b byte, s packed string (always last),
 * a address in this program, p program number, A address in program p.
 */
typedef struct {
  const char *name;
  const char *args;
} OpInfo;

static const OpInfo op_table[128] = {
  [0]  = { "var_add_const",       "ww" },
  [1]  = { "var_add_var",         "ww" },
  [2]  = { "add_person",          "wwwwwwwb" },
  [4]  = { "click_close",         "w" },
  [6]  = { "teak_effect",         "w" },
  [7]  = { "close_window",        "" },
  [10] = { "abort_redraw",        "" },
  [12] = { "say_wait",            "w" },
  [13] = { "conditional_walk",    "wwwwwwww" },
  [14] = { "click_or_enter",      "w" },
  [15] = { "get_person_x",        "ww" },
  [16] = { "get_person_y",        "ww" },
  [17] = { "call_proc",           "p" },
  [18] = { "if",                  "wwwwa" },
  [19] = { "set_ghost_level",     "ww" },
  [20] = { "if_person_in",        "wwwwwa" },
  [21] = { "if_wasnt_here",       "wa" },
  [22] = { "goto_proc",           "pA" },
  [23] = { "arm_key_abort",       "" },
  [26] = { "print_var",           "w" },
  [27] = { "add_area_label",      "wwwww" },
  [28] = { "add_area_jumper",     "wwwww" },
  [29] = { "clear_screen_init",   "" },
  [30] = { "ontime_jump",         "wpA" },
  [31] = { "open_window",         "w" },
  [32] = { "var_rand_const",      "ww" },
  [33] = { "var_rand_var",        "ww" },
  [34] = { "set_person_z",        "ww" },
  [35] = { "remove_person",       "w" },
  [38] = { "var_set_const",       "ww" },
  [39] = { "var_set_var",         "ww" },
  [40] = { "set_object",          "ww" },
  [41] = { "blend",               "www" },
  [42] = { "load_bild",           "www" },
  [43] = { "stop_all_walking",    "" },
  [44] = { "print_sentence",      "s" },
  [45] = { "unmark_area",         "w" },
  [46] = { "unmark_area2",        "w" },
  [47] = { "wait",                "w" },
  [48] = { "return_not_found",    "" },
  [50] = { "return_found",        "" },
  [51] = { "restart",             "" },
  [52] = { "blend_in",            "w" },
  [53] = { "blend_out",           "w" },
  [54] = { "adress",              "ww" },
  [55] = { "wait_while_walking",  "" },
  [56] = { "parse_if",            "wwwwwa" },
  [57] = { "set_person",          "wwwwwwwb" },
  [58] = { "shake",               "ww" },
  [59] = { "init_ontimes",        "" },
  [60] = { "if_eq_print",         "wws" },
  [61] = { "execute",             "pA" },
  [62] = { "add_power_parse",     "ws" },
  [63] = { "check_file",          "wwww" },
  [64] = { "display_message",     "s" },
  [65] = { "add_description",     "ws" },
  [66] = { "add_say_window",      "wwws" },
  [67] = { "add_option",          "wwwws" },
  [68] = { "add_dialog",          "w" },
  [69] = { "init_dialogs",        "" },
  [70] = { "remove_dialog",       "w" },
  [71] = { "reset_dialog",        "w" },
  [72] = { "select_from_dialog",  "ww" },
  [73] = { "toggle_dialog_point", "www" },
  [74] = { "if3",                 "wwwwwwwwwwwwa" },
  [75] = { "toggle_screen",       "w" },
  [76] = { "internal_error",      "w" },
  [77] = { "packing_off",         "" },
  [78] = { "select_from_dialog2", "ww" },
  [79] = { "menu_return",         "" },
  [80] = { "if2",                 "wwwwwwwwa" },
  [81] = { "add_say_window2",     "wwws" },
  [82] = { "add_walk_area",       "wwwwww" },
  [83] = { "remove_statists",     "" },
  [84] = { "print_sentence_plus", "s" },
  [85] = { "say_properties",      "wwww" },
  [86] = { "close_say",           "w" },
  [87] = { "blend2",              "wwww" },
  [88] = { "walk_person",         "www" },
  [89] = { "code_text",           "ws" },
  [90] = { "print_sentence_ret",  "s" },
  [91] = { "set_room_name",       "s" },
  [92] = { "say_if",              "wwws" },
  [93] = { "palette_cycle",       "www" },
  [94] = { "look_keys",           "" },
  [95] = { "load_music",          "w" },
  [96] = { "iwalk",               "www" },
  [97] = { "say_if_plus",         "wwws" },
  [98] = { "say_if_plus2",        "wwws" },
  [99] = { "say_if_found",        "wwws" },
};

#define MAX_ARGS 16

const char *script_op_name(uint8_t op)
{
  return op < 128 ? op_table[op].name : NULL;
}

/* Bytes of fixed operands, plus the string offset if there is one. */
static int fixed_size(const char *args, int *soff)
{
  int n = 1;
  *soff = -1;
  for (; *args; args++) {
    if (*args == 'b') n += 1;
    else if (*args == 's') *soff = n;
    else n += 2;
  }
  return n;
}

int script_string_offset(uint8_t op)
{
  int soff;
  if (!script_op_name(op)) return -1;
  fixed_size(op_table[op].args, &soff);
  return soff;
}

int script_insn_size(const uint8_t *code, int len, int pc)
{
  int soff, n, i;
  if (pc >= len || !script_op_name(code[pc])) return -1;
  n = fixed_size(op_table[code[pc]].args, &soff);
  if (soff < 0) return pc + n <= len ? n : -1;
  for (i = pc + soff; i < len; i++)
    if (code[i] == 0) return i + 1 - pc;
  return -1;
}

static int rd16(const uint8_t *p)
{
  return (int16_t)(p[0] | (p[1] << 8));
}

/* ---- disassembler ---------------------------------------------------- */

typedef struct {
  uint8_t *boundary;   /* len+1 entries: instruction starts (and end) */
  uint8_t *target;     /* len+1 entries: referenced by an address operand */
  int tail;            /* start of undecodable bytes, or len */
} ProgMap;

static void map_prog(const Prog *p, ProgMap *m)
{
  int pc = 0, sz;
  m->boundary = xmalloc((size_t)p->len + 1);
  m->target = xmalloc((size_t)p->len + 1);
  memset(m->boundary, 0, (size_t)p->len + 1);
  memset(m->target, 0, (size_t)p->len + 1);
  while ((sz = script_insn_size(p->code, p->len, pc)) > 0) {
    m->boundary[pc] = 1;
    pc += sz;
  }
  m->tail = pc;
  m->boundary[pc] = 1;
}

/* Mark the targets of program idx's address operands. */
static void mark_targets(const Prog *progs, int nprog, ProgMap *maps, int idx)
{
  const Prog *p = &progs[idx];
  int pc = 0;
  while (pc < maps[idx].tail) {
    const char *a = op_table[p->code[pc]].args;
    int off = 1, proc = -1;
    for (; *a; a++) {
      int v = (*a == 'b' || *a == 's') ? 0 : (uint16_t)rd16(p->code + pc + off);
      if (*a == 'p') proc = v;
      if (*a == 'a' && v <= p->len && maps[idx].boundary[v]) maps[idx].target[v] = 1;
      if (*a == 'A' && proc >= 0 && proc < nprog && v <= progs[proc].len && maps[proc].boundary[v])
        maps[proc].target[v] = 1;
      off += *a == 'b' ? 1 : 2;
    }
    pc += script_insn_size(p->code, p->len, pc);
  }
}

static void put_db(FILE *fp, const uint8_t *b, int n)
{
  int i;
  for (i = 0; i < n; i++)
    fprintf(fp, "%s0x%02x", i % 16 == 0 ? (i ? "\n        .db " : "        .db ") : ", ", b[i]);
  fputc('\n', fp);
}

void script_disasm(FILE *fp, int idx, const Prog *progs, int nprog, const Dict *d)
{
  ProgMap *maps = xmalloc(sizeof(ProgMap) * (size_t)nprog);
  const Prog *p = &progs[idx];
  int i, pc = 0;

  for (i = 0; i < nprog; i++) map_prog(&progs[i], &maps[i]);
  for (i = 0; i < nprog; i++) mark_targets(progs, nprog, maps, i);

  fprintf(fp, "; MOAS script, program %d (GAME.3). Assembled by moas-util pack.\n"
              "; Labels L_xxxx are named after their original address. The address\n"
              "; operand of goto_proc, execute and ontime_jump is a label in the program\n"
              "; given by the operand before it. \"; @xxxx\" comments are the original\n"
              "; addresses, as shown by the engine's script debug log.\n\n", idx);
  while (pc < maps[idx].tail) {
    uint8_t op = p->code[pc];
    const char *a = op_table[op].args;
    int off = 1, proc = -1, first = 1, sz = script_insn_size(p->code, p->len, pc);
    int soff = script_string_offset(op);

    /* A string that doesn't re-pack to the same bytes stays raw. */
    if (soff >= 0) {
      char text[1024];
      uint8_t again[1024];
      int bad;
      dict_unpack(d, p->code + pc + soff, text, sizeof(text));
      if (dict_pack(d, (const uint8_t *)text, again, sizeof(again), &bad) < 0 ||
          strcmp((const char *)again, (const char *)p->code + pc + soff)) {
        if (maps[idx].target[pc]) fprintf(fp, "L_%04x:\n", pc);
        fprintf(fp, "        ; %s with a string the dictionary packs differently\n", op_table[op].name);
        put_db(fp, p->code + pc, sz);
        pc += sz;
        continue;
      }
    }
    if (maps[idx].target[pc]) fprintf(fp, "L_%04x:\n", pc);
    fprintf(fp, "        %s", op_table[op].name);
    for (; *a; a++, first = 0) {
      fputs(first ? " " : ", ", fp);
      if (*a == 's') {
        char text[1024];
        dict_unpack(d, p->code + pc + off, text, sizeof(text));
        put_cp850(fp, (const uint8_t *)text, strlen(text));
      } else if (*a == 'b') {
        fprintf(fp, "%d", p->code[pc + off]);
        off += 1;
      } else {
        int v = rd16(p->code + pc + off), u = (uint16_t)v;
        if (*a == 'p') proc = v;
        if (*a == 'a' && u <= p->len && maps[idx].target[u])
          fprintf(fp, "L_%04x", u);
        else if (*a == 'A' && proc >= 0 && proc < nprog && u <= progs[proc].len && maps[proc].target[u])
          fprintf(fp, "L_%04x", u);
        else if (*a == 'a' || *a == 'A')
          fprintf(fp, "%d", u);
        else
          fprintf(fp, "%d", v);
        off += 2;
      }
    }
    fprintf(fp, "  ; @%04x\n", pc);
    pc += sz;
  }
  if (maps[idx].target[pc]) fprintf(fp, "L_%04x:\n", pc);
  if (pc < p->len) {
    fprintf(fp, "        ; %d bytes that don't decode as instructions\n", p->len - pc);
    put_db(fp, p->code + pc, p->len - pc);
    if (maps[idx].target[p->len]) fprintf(fp, "L_%04x:\n", p->len);
  }

  for (i = 0; i < nprog; i++) { free(maps[i].boundary); free(maps[i].target); }
  free(maps);
}

/* ---- assembler ------------------------------------------------------- */

typedef struct {
  long num;
  char *label;          /* or NULL */
  uint8_t *packed;      /* string operand */
} Arg;

typedef struct {
  int line;
  int op;               /* -1: .db */
  int nargs;
  Arg args[MAX_ARGS];
  uint8_t *raw;         /* .db bytes */
  int size, addr;
} Item;

typedef struct {
  char *name;
  int addr, line;
} Label;

typedef struct {
  Item *items;
  int nitems;
  Label *labels;
  int nlabels;
  int size;
} Unit;

static int g_errors;

static void asm_error(const char *file, int line, const char *fmt, ...) MU_PRINTF(3, 4);

static void asm_error(const char *file, int line, const char *fmt, ...)
{
  va_list ap;
  fprintf(stderr, "%s:%d: error: ", file, line);
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);
  g_errors++;
}

static int ident_len(const char *p)
{
  int n = 0;
  if (!(isalpha((unsigned char)*p) || *p == '_' || *p == '.')) return 0;
  while (isalnum((unsigned char)p[n]) || p[n] == '_' || (n == 0 && p[n] == '.')) n++;
  return n;
}

/* Integer literal: decimal or 0x hex. */
static int parse_number(const char **pp, long *v)
{
  const char *p = *pp;
  char *end;
  int neg = 0;
  if (*p == '-') { neg = 1; p++; }
  if (!isdigit((unsigned char)*p)) return 0;
  if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) *v = strtol(p + 2, &end, 16);
  else *v = strtol(p, &end, 10);
  if (end == p || isalnum((unsigned char)*end) || *end == '_') return 0;
  if (neg) *v = -*v;
  *pp = end;
  return 1;
}

static const Label *find_label(const Unit *u, const char *name)
{
  int i;
  for (i = 0; i < u->nlabels; i++)
    if (!strcmp(u->labels[i].name, name)) return &u->labels[i];
  return NULL;
}

static void parse_unit(const char *file, const char *text, const Dict *d, Unit *u)
{
  const char *p = text;
  int line, addr = 0, cap = 0, lcap = 0;
  memset(u, 0, sizeof(*u));
  for (line = 1; *p; p = next_line(p), line++) {
    const char *q = skip_ws(p), *err;
    int n;
    Item it;

    /* labels */
    while ((n = ident_len(q)) > 0 && q[n] == ':') {
      char *name = xmalloc((size_t)n + 1);
      memcpy(name, q, (size_t)n);
      name[n] = 0;
      if (find_label(u, name)) asm_error(file, line, "label '%s' defined twice", name);
      if (u->nlabels >= lcap) {
        lcap = lcap ? lcap * 2 : 64;
        u->labels = xrealloc(u->labels, sizeof(Label) * (size_t)lcap);
      }
      u->labels[u->nlabels].name = name;
      u->labels[u->nlabels].addr = addr;
      u->labels[u->nlabels].line = line;
      u->nlabels++;
      q = skip_ws(q + n + 1);
    }
    if (at_line_end(q)) continue;

    memset(&it, 0, sizeof(it));
    it.line = line;
    it.addr = addr;
    n = ident_len(q);
    if (n == 3 && !strncmp(q, ".db", 3)) {
      int cap_raw = 0;
      it.op = -1;
      q += 3;
      for (;;) {
        long v;
        q = skip_ws(q);
        if (!parse_number(&q, &v) || v < -128 || v > 255) {
          asm_error(file, line, ".db: expected a byte value");
          break;
        }
        if (it.size >= cap_raw) {
          cap_raw = cap_raw ? cap_raw * 2 : 16;
          it.raw = xrealloc(it.raw, (size_t)cap_raw);
        }
        it.raw[it.size++] = (uint8_t)v;
        q = skip_ws(q);
        if (*q != ',') break;
        q++;
      }
      if (!at_line_end(q)) asm_error(file, line, ".db: junk at end of line");
    } else {
      int op, soff, bad;
      const char *a;
      for (op = 0; op < 128; op++)
        if (op_table[op].name && (int)strlen(op_table[op].name) == n &&
            !strncmp(op_table[op].name, q, (size_t)n)) break;
      if (n == 0 || op == 128) {
        asm_error(file, line, "unknown instruction '%.*s'", n ? n : 1, q);
        continue;
      }
      it.op = op;
      it.size = fixed_size(op_table[op].args, &soff);
      q += n;
      bad = 0;
      for (a = op_table[op].args; *a && !bad; a++) {
        Arg *arg = &it.args[it.nargs++];
        q = skip_ws(q);
        if (a != op_table[op].args) {
          if (*q != ',') { asm_error(file, line, "%s: expected %d operands", op_table[op].name, (int)strlen(op_table[op].args)); bad = 1; break; }
          q = skip_ws(q + 1);
        }
        if (*a == 's') {
          uint8_t text[1024], packed[1024];
          int badpos, len;
          if (parse_cp850_string(&q, text, sizeof(text), &err) < 0) { asm_error(file, line, "%s", err); bad = 1; break; }
          len = dict_pack(d, text, packed, sizeof(packed), &badpos);
          if (len < 0) {
            asm_error(file, line, "no dictionary entry (pck.txt) covers \"%s\"", (const char *)text + badpos);
            bad = 1;
            break;
          }
          arg->packed = xmalloc((size_t)len + 1);
          memcpy(arg->packed, packed, (size_t)len + 1);
          it.size += len + 1;
        } else if (parse_number(&q, &arg->num)) {
          if (*a == 'b' ? (arg->num < -128 || arg->num > 255) : (arg->num < -32768 || arg->num > 65535))
            asm_error(file, line, "operand %d out of range", it.nargs);
        } else if ((*a == 'a' || *a == 'A') && (n = ident_len(q)) > 0) {
          arg->label = xmalloc((size_t)n + 1);
          memcpy(arg->label, q, (size_t)n);
          arg->label[n] = 0;
          q += n;
        } else {
          asm_error(file, line, "%s: bad operand %d", op_table[op].name, it.nargs);
          bad = 1;
        }
      }
      if (!bad && *skip_ws(q) == ',')
        asm_error(file, line, "%s: expected %d operands", op_table[op].name, (int)strlen(op_table[op].args));
      else if (!bad && !at_line_end(q))
        asm_error(file, line, "%s: junk at end of line", op_table[op].name);
    }
    if (u->nitems >= cap) {
      cap = cap ? cap * 2 : 256;
      u->items = xrealloc(u->items, sizeof(Item) * (size_t)cap);
    }
    u->items[u->nitems++] = it;
    addr += it.size;
  }
  u->size = addr;
  if (addr > 65535) asm_error(file, line, "program is larger than 64 KiB");
}

int script_assemble(char **texts, const char **names, int nprog, const Dict *d, Prog *progs)
{
  Unit *units = xmalloc(sizeof(Unit) * (size_t)(nprog ? nprog : 1));
  int i, j, k, nprog_emit = nprog;

  g_errors = 0;
  for (i = 0; i < nprog; i++) parse_unit(names[i], texts[i], d, &units[i]);
  if (g_errors) nprog_emit = 0;   /* items may be incomplete */

  for (i = 0; i < nprog_emit; i++) {
    Unit *u = &units[i];
    uint8_t *code = xmalloc((size_t)u->size + 1);
    int pc = 0;
    for (j = 0; j < u->nitems; j++) {
      Item *it = &u->items[j];
      const char *a;
      int proc = -1;
      if (it->op < 0) {
        memcpy(code + pc, it->raw, (size_t)it->size);
        pc += it->size;
        continue;
      }
      code[pc++] = (uint8_t)it->op;
      for (a = op_table[it->op].args, k = 0; *a && k < it->nargs; a++, k++) {
        Arg *arg = &it->args[k];
        long v = arg->num;
        if (*a == 's') {
          size_t l = strlen((const char *)arg->packed) + 1;
          memcpy(code + pc, arg->packed, l);
          pc += (int)l;
          continue;
        }
        if (arg->label) {
          const Unit *in = *a == 'a' ? u : (proc >= 0 && proc < nprog ? &units[proc] : NULL);
          const Label *l = in ? find_label(in, arg->label) : NULL;
          if (!in) asm_error(names[i], it->line, "label '%s' in unknown program %d", arg->label, proc);
          else if (!l) asm_error(names[i], it->line, "undefined label '%s'%s", arg->label,
                                 *a == 'A' ? " in the target program" : "");
          v = l ? l->addr : 0;
        }
        if (*a == 'p') proc = (int)v;
        if (*a == 'b') code[pc++] = (uint8_t)v;
        else {
          code[pc++] = (uint8_t)(v & 255);
          code[pc++] = (uint8_t)((v >> 8) & 255);
        }
      }
    }
    progs[i].code = code;
    progs[i].len = u->size;
  }

  for (i = 0; i < nprog; i++) {
    for (j = 0; j < units[i].nitems; j++) {
      for (k = 0; k < units[i].items[j].nargs; k++) {
        free(units[i].items[j].args[k].label);
        free(units[i].items[j].args[k].packed);
      }
      free(units[i].items[j].raw);
    }
    for (j = 0; j < units[i].nlabels; j++) free(units[i].labels[j].name);
    free(units[i].items);
    free(units[i].labels);
  }
  free(units);
  return g_errors;
}
