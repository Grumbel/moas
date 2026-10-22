/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "lang.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "moas.h"

#ifndef DOS
#include "datapath.h"
#endif

/* Verb templates are assigned in LangInit (also set at compile time in moas.c). */
extern char *__Benutze, *__BenutzeMit, *__Siehe, *__Sprich, *__Bewege, *__BewegeMit;

static const char *ui_fallback[UI_COUNT] = {
  [UI_VERB_USE]      = "BENUTZE %s",
  [UI_VERB_USE_WITH] = "BENUTZE %s MIT %s",
  [UI_VERB_LOOK]     = "SCHAU %s AN",
  [UI_VERB_TALK]     = "REDE MIT %s",
  [UI_VERB_MOVE]     = "BEWEGE %s",
  [UI_VERB_MOVE_DIR] = "BEWEGE %s IN RICHTUNG %s",
  [UI_SAVE_TITLE]    = "Speichern:",
  [UI_LOAD_TITLE]    = "Laden:",
  [UI_SAVE_FAIL]     = "Speichern fehlgeschlagen",
  [UI_LOAD_FAIL]     = "Laden fehlgeschlagen",
  [UI_SLOT_NONE]     = "Kein Slot gewaehlt",
  [UI_SLOT_OF]       = "%i von %i",
  [UI_OV_SAVE]       = "SAVE",
  [UI_OV_LOAD]       = "LOAD",
  [UI_OV_ESC]        = "ESC",
  [UI_OV_SKIP]       = "SKIP",
  [UI_OV_USE]        = "USE",
  [UI_OV_LOOK]       = "LOOK",
  [UI_OV_MENU]       = "MENU",
  [UI_MENU_OPTIONS]  = "Optionen (F12)",
};

static const char *ui_names[UI_COUNT] = {
  [UI_VERB_USE]      = "verb.use",
  [UI_VERB_USE_WITH] = "verb.use_with",
  [UI_VERB_LOOK]     = "verb.look",
  [UI_VERB_TALK]     = "verb.talk",
  [UI_VERB_MOVE]     = "verb.move",
  [UI_VERB_MOVE_DIR] = "verb.move_dir",
  [UI_SAVE_TITLE]    = "save.title",
  [UI_LOAD_TITLE]    = "load.title",
  [UI_SAVE_FAIL]     = "save.fail",
  [UI_LOAD_FAIL]     = "load.fail",
  [UI_SLOT_NONE]     = "slot.none",
  [UI_SLOT_OF]       = "slot.of",
  [UI_OV_SAVE]       = "overlay.save",
  [UI_OV_LOAD]       = "overlay.load",
  [UI_OV_ESC]        = "overlay.esc",
  [UI_OV_SKIP]       = "overlay.skip",
  [UI_OV_USE]        = "overlay.use",
  [UI_OV_LOOK]       = "overlay.look",
  [UI_OV_MENU]       = "overlay.menu",
  [UI_MENU_OPTIONS]  = "menu.options",
};

static char *ui_str[UI_COUNT];
static char lang_code[16] = "de";

/* Script map: key UTF-8 (German), value CP850 (for the game font). */
typedef struct {
  char *key;
  char *val;
} LangEnt;

static LangEnt *script_tab;
static int script_n, script_cap;
static char lookup_buf[400];

const char *Ui(int id)
{
  if (id < 0 || id >= UI_COUNT)
    return "";
  if (ui_str[id] && ui_str[id][0])
    return ui_str[id];
  return ui_fallback[id] ? ui_fallback[id] : "";
}

const char *LangGetCode(void)
{
  return lang_code;
}

void LangSetCode(const char *code)
{
  size_t i, n;
  if (!code || !code[0])
    return;
  n = 0;
  for (i = 0; code[i] && n < sizeof(lang_code) - 1; i++) {
    char c = code[i];
    if (c == '/' || c == '\\' || c == '.')
      break;
    if (isalnum((unsigned char)c) || c == '_' || c == '-')
      lang_code[n++] = (char)tolower((unsigned char)c);
  }
  lang_code[n] = 0;
  if (!lang_code[0])
    strcpy(lang_code, "de");
}

static char *xstrdup_local(const char *s)
{
  size_t n = strlen(s) + 1;
  char *p = (char *)malloc(n);
  if (p)
    memcpy(p, s, n);
  return p;
}

/* CP850 → UTF-8 for lookup keys (German umlauts + common punct). */
static void cp850_to_utf8(const unsigned char *in, char *out, size_t outsz)
{
  size_t o = 0;
  int i;
  for (i = 0; in[i] && o + 4 < outsz; i++) {
    unsigned char c = in[i];
    const char *u = NULL;
    if (c >= 32 && c < 127) {
      if (c == '`')
        c = '\'';
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
      /* acute/grave often used as apostrophe-like in DOS text */
      case 0xb4: u = "'"; break; /* ´ */
      case 0xa8: u = "\""; break; /* ¨ */
      case '\n': u = "\n"; break;
      default:
        /* keep byte as-is if it is a single-byte that might match keys poorly —
         * prefer '?' only for truly unknown high bytes */
        out[o++] = '?';
        continue;
    }
    while (*u && o + 1 < outsz)
      out[o++] = *u++;
  }
  out[o] = 0;
}

/*
 * UTF-8 → bytes the bitmap font can show (CharAvail in print.c):
 *   A-Z ÄÖÜß  - .!?,:;' " 0-9 <>()[]  a-z äöü
 * Smart quotes/dashes/ellipsis collapse to ASCII so they are not drawn as '?'.
 * '%' stays: verbs and power-parse lines are printf formats ("USE %s").
 */
static void utf8_to_cp850(const char *in, char *out, size_t outsz)
{
  size_t o = 0;
  const unsigned char *p = (const unsigned char *)in;
  while (*p && o + 1 < outsz) {
    if (*p < 0x80) {
      unsigned char c = *p++;
      /* glyphs not in CharAvail → nearest safe ASCII */
      if (c == '_' || c == '@')
        c = ' ';
      else if (c == '`')
        c = '\'';
      else if (c == '#' || c == '$' || c == '&' || c == '*'
               || c == '+' || c == '/' || c == '=' || c == '\\' || c == '^'
               || c == '{' || c == '}' || c == '|' || c == '~')
        c = ' ';
      out[o++] = (char)c;
      continue;
    }
    if ((p[0] & 0xe0) == 0xc0 && p[1]) {
      unsigned int cp = ((p[0] & 0x1f) << 6) | (p[1] & 0x3f);
      unsigned char c = 0;
      if (cp == 0xe4) c = 0x84;      /* ä */
      else if (cp == 0xf6) c = 0x94; /* ö */
      else if (cp == 0xfc) c = 0x81; /* ü */
      else if (cp == 0xc4) c = 0x8e; /* Ä */
      else if (cp == 0xd6) c = 0x99; /* Ö */
      else if (cp == 0xdc) c = 0x9a; /* Ü */
      else if (cp == 0xdf) c = 0xe1; /* ß */
      else if (cp == 0xa0) c = ' ';  /* nbsp */
      else if (cp == 0xb4 || cp == 0x60) c = '\'';
      else
        c = ' '; /* unknown Latin-1-ish → space, not '?' */
      out[o++] = (char)c;
      p += 2;
      continue;
    }
    if ((p[0] & 0xf0) == 0xe0 && p[1] && p[2]) {
      unsigned int cp = ((p[0] & 0x0f) << 12) | ((p[1] & 0x3f) << 6) | (p[2] & 0x3f);
      const char *rep = " ";
      if (cp == 0x2018 || cp == 0x2019 || cp == 0x201a || cp == 0x2032)
        rep = "'";
      else if (cp == 0x201c || cp == 0x201d || cp == 0x201e || cp == 0x2033)
        rep = "\"";
      else if (cp == 0x2013 || cp == 0x2014 || cp == 0x2212)
        rep = "-";
      else if (cp == 0x2026)
        rep = "...";
      while (*rep && o + 1 < outsz)
        out[o++] = *rep++;
      p += 3;
      continue;
    }
    if ((p[0] & 0xf8) == 0xf0 && p[1] && p[2] && p[3]) {
      p += 4;
      if (o + 1 < outsz)
        out[o++] = ' ';
      continue;
    }
    p++;
    if (o + 1 < outsz)
      out[o++] = ' ';
  }
  out[o] = 0;
}

static int ui_id_from_name(const char *name)
{
  int i;
  for (i = 0; i < UI_COUNT; i++)
    if (ui_names[i] && !strcmp(ui_names[i], name))
      return i;
  /* also accept numeric index */
  if (name[0] >= '0' && name[0] <= '9') {
    int v = atoi(name);
    if (v >= 0 && v < UI_COUNT)
      return v;
  }
  return -1;
}

/*
 * Conversion letters of a printf format, in order ("BENUTZE %s MIT %s" → "ss").
 * Flags/width are not used by any UI string and make the format unsafe.
 * Returns -1 on anything but %s, %i, %d, %%.
 */
static int format_signature(const char *fmt, char *sig, size_t sigsz)
{
  size_t n = 0;
  for (; *fmt; fmt++) {
    if (*fmt != '%')
      continue;
    fmt++;
    if (*fmt == 0)
      return -1; /* trailing lone '%' */
    if (*fmt == '%')
      continue;
    if (*fmt != 's' && *fmt != 'i' && *fmt != 'd')
      return -1;
    if (n + 1 >= sigsz)
      return -1;
    sig[n++] = (*fmt == 'd') ? 'i' : *fmt;
  }
  sig[n] = 0;
  return 0;
}

/*
 * Translations of format strings go to sprintf with the German arguments:
 * a mismatch would crash. Warn and keep the German text instead.
 */
static int same_format(const char *orig, const char *tr, const char *what)
{
  char want[8], got[8];

  if (!strcmp(orig, tr))
    return 1;
  if (format_signature(orig, want, sizeof(want)) == 0
      && format_signature(tr, got, sizeof(got)) == 0
      && strcmp(want, got) == 0)
    return 1;
  fprintf(stderr, "moas: lang: %s: \"%s\" does not match format of \"%s\"; ignored\n",
          what, tr, orig);
  return 0;
}

static void ui_set(int id, const char *text_utf8)
{
  char buf[256];
  if (id < 0 || id >= UI_COUNT || !text_utf8)
    return;
  utf8_to_cp850(text_utf8, buf, sizeof(buf));
  if (!same_format(ui_fallback[id], buf, ui_names[id]))
    return;
  free(ui_str[id]);
  ui_str[id] = xstrdup_local(buf[0] ? buf : text_utf8);
}

static void script_add(const char *key_utf8, const char *val_utf8)
{
  char val_cp[400];
  LangEnt *e;
  if (!key_utf8 || !key_utf8[0] || !val_utf8)
    return;
  utf8_to_cp850(val_utf8, val_cp, sizeof(val_cp));
  if (!same_format(key_utf8, val_cp, "script"))
    return;
  if (script_n >= script_cap) {
    int nc = script_cap ? script_cap * 2 : 512;
    LangEnt *n = (LangEnt *)realloc(script_tab, (size_t)nc * sizeof(LangEnt));
    if (!n)
      return;
    script_tab = n;
    script_cap = nc;
  }
  e = &script_tab[script_n++];
  e->key = xstrdup_local(key_utf8);
  e->val = xstrdup_local(val_cp[0] ? val_cp : val_utf8);
}

static int script_cmp(const void *a, const void *b)
{
  const LangEnt *ea = (const LangEnt *)a;
  const LangEnt *eb = (const LangEnt *)b;
  return strcmp(ea->key, eb->key);
}

static void script_sort(void)
{
  if (script_n > 1)
    qsort(script_tab, (size_t)script_n, sizeof(LangEnt), script_cmp);
}

const char *LangLookup(const char *cp850_source)
{
  char key[400];
  int lo, hi;
  if (!cp850_source || !cp850_source[0] || script_n <= 0)
    return NULL;
  cp850_to_utf8((const unsigned char *)cp850_source, key, sizeof(key));
  lo = 0;
  hi = script_n - 1;
  while (lo <= hi) {
    int mid = lo + (hi - lo) / 2;
    int c = strcmp(key, script_tab[mid].key);
    if (c == 0)
      return script_tab[mid].val;
    if (c < 0)
      hi = mid - 1;
    else
      lo = mid + 1;
  }
  return NULL;
}


/* ---- minimal gettext .po reader (msgid / msgstr / msgctxt) ------------ */

static void str_append(char *out, size_t outsz, size_t *used, const char *piece)
{
  size_t n = strlen(piece);
  if (*used + n >= outsz)
    n = outsz - *used - 1;
  if (n > 0) {
    memcpy(out + *used, piece, n);
    *used += n;
    out[*used] = 0;
  }
}

/* Decode one "..." PO string segment into out (appended). Advances *pp past it. */
static int po_take_quoted(const char **pp, char *out, size_t outsz, size_t *used)
{
  const char *p = *pp;
  while (*p == ' ' || *p == '\t')
    p++;
  if (*p != '"')
    return -1;
  p++;
  while (*p && *p != '"') {
    if (*p == '\\' && p[1]) {
      p++;
      if (*p == 'n')
        str_append(out, outsz, used, "\n");
      else if (*p == 't')
        str_append(out, outsz, used, "\t");
      else if (*p == 'r')
        str_append(out, outsz, used, "\r");
      else if (*p == '"' || *p == '\\') {
        char b[2];
        b[0] = *p;
        b[1] = 0;
        str_append(out, outsz, used, b);
      } else {
        char b[2];
        b[0] = *p;
        b[1] = 0;
        str_append(out, outsz, used, b);
      }
      p++;
    } else {
      char b[2];
      b[0] = *p++;
      b[1] = 0;
      str_append(out, outsz, used, b);
    }
  }
  if (*p == '"')
    p++;
  *pp = p;
  return 0;
}

/* Read msgid/msgstr body: optional text on same line + continuation lines of only "..." */
static int po_read_string_body(FILE *f, char *first_rest, char *out, size_t outsz)
{
  size_t used = 0;
  char line[1024];
  const char *p;
  out[0] = 0;
  p = first_rest;
  while (*p == ' ' || *p == '\t')
    p++;
  if (*p == '"') {
    if (po_take_quoted(&p, out, outsz, &used) != 0)
      return -1;
  }
  /* continuation lines */
  for (;;) {
    long pos = ftell(f);
    if (!fgets(line, sizeof(line), f))
      break;
    p = line;
    while (*p == ' ' || *p == '\t')
      p++;
    if (*p == '"') {
      if (po_take_quoted(&p, out, outsz, &used) != 0)
        return -1;
      continue;
    }
    /* not a continuation — push back by seeking (works for real files) */
    fseek(f, pos, SEEK_SET);
    break;
  }
  return 0;
}

static void po_commit(const char *ctxt, const char *msgid, const char *msgstr)
{
  int id;
  if (!msgid || !msgid[0])
    return; /* header or empty */
  if (!msgstr)
    msgstr = "";
  /* UI by msgctxt (ui.verb.use or verb.use) */
  if (ctxt && ctxt[0]) {
    const char *name = ctxt;
    if (!strncmp(name, "ui.", 3))
      name += 3;
    id = ui_id_from_name(name);
    if (id >= 0)
      ui_set(id, msgstr);
  } else {
    /* UI by matching German fallback as msgid */
    for (id = 0; id < UI_COUNT; id++) {
      if (ui_fallback[id] && !strcmp(ui_fallback[id], msgid)) {
        ui_set(id, msgstr);
        break;
      }
    }
  }
  /* Script map: full German line → translation */
  if (msgstr[0])
    script_add(msgid, msgstr);
}

static void load_po_file(const char *path)
{
  FILE *f = fopen(path, "rb");
  char line[1024];
  char ctxt[256], msgid[400], msgstr[400];
  int have_id = 0, have_str = 0, have_ctx = 0;

  if (!f)
    return;
  ctxt[0] = msgid[0] = msgstr[0] = 0;

  while (fgets(line, sizeof(line), f)) {
    char *p = line;
    while (*p == ' ' || *p == '\t')
      p++;
    if (*p == '#' || *p == '\n' || *p == '\r' || *p == 0)
      continue;

    if (!strncmp(p, "msgctxt", 7) && (p[7] == ' ' || p[7] == '\t' || p[7] == '"')) {
      if (have_id && have_str)
        po_commit(have_ctx ? ctxt : NULL, msgid, msgstr);
      have_id = have_str = 0;
      have_ctx = 1;
      ctxt[0] = 0;
      po_read_string_body(f, p + 7, ctxt, sizeof(ctxt));
      continue;
    }
    if (!strncmp(p, "msgid_plural", 12)) {
      /* skip plural forms for now */
      char skip[400];
      po_read_string_body(f, p + 12, skip, sizeof(skip));
      continue;
    }
    if (!strncmp(p, "msgid", 5) && (p[5] == ' ' || p[5] == '\t' || p[5] == '"')) {
      if (have_id && have_str)
        po_commit(have_ctx ? ctxt : NULL, msgid, msgstr);
      have_id = 1;
      have_str = 0;
      msgid[0] = 0;
      msgstr[0] = 0;
      po_read_string_body(f, p + 5, msgid, sizeof(msgid));
      continue;
    }
    if (!strncmp(p, "msgstr", 6) && (p[6] == ' ' || p[6] == '\t' || p[6] == '"' || p[6] == '[')) {
      /* msgstr or msgstr[N] — only use msgstr / msgstr[0] */
      char *rest = p + 6;
      if (*rest == '[') {
        if (rest[1] != '0') {
          char skip[400];
          while (*rest && *rest != ' ' && *rest != '\t' && *rest != '"')
            rest++;
          po_read_string_body(f, rest, skip, sizeof(skip));
          continue;
        }
        while (*rest && *rest != ' ' && *rest != '\t' && *rest != '"')
          rest++;
      }
      have_str = 1;
      msgstr[0] = 0;
      po_read_string_body(f, rest, msgstr, sizeof(msgstr));
      continue;
    }
  }
  if (have_id && have_str)
    po_commit(have_ctx ? ctxt : NULL, msgid, msgstr);
  fclose(f);
  script_sort();
}

static int open_lang_file(char *path, size_t pathsz, const char *suffix)
{
  const char *data;
  FILE *f;
  /* 1) $MOAS_LANGDIR/code.suffix */
  {
    const char *ld = getenv("MOAS_LANGDIR");
    if (ld && ld[0]) {
      snprintf(path, pathsz, "%s/%s.%s", ld, lang_code, suffix);
      f = fopen(path, "rb");
      if (f) {
        fclose(f);
        return 0;
      }
    }
  }
  /* 2) next to datadir: <datadir>/../lang/code.suffix or <datadir>/lang/ */
#ifndef DOS
  data = getenv("MOAS_DATADIR");
  if (data && data[0]) {
    snprintf(path, pathsz, "%s/lang/%s.%s", data, lang_code, suffix);
    f = fopen(path, "rb");
    if (f) {
      fclose(f);
      return 0;
    }
    snprintf(path, pathsz, "%s/../lang/%s.%s", data, lang_code, suffix);
    f = fopen(path, "rb");
    if (f) {
      fclose(f);
      return 0;
    }
  }
#endif
  /* 3) ./lang/code.suffix */
  snprintf(path, pathsz, "lang/%s.%s", lang_code, suffix);
  f = fopen(path, "rb");
  if (f) {
    fclose(f);
    return 0;
  }
  return -1;
}

static void apply_verb_globals(void)
{
  __Benutze    = (char *)Ui(UI_VERB_USE);
  __BenutzeMit = (char *)Ui(UI_VERB_USE_WITH);
  __Siehe      = (char *)Ui(UI_VERB_LOOK);
  __Sprich     = (char *)Ui(UI_VERB_TALK);
  __Bewege     = (char *)Ui(UI_VERB_MOVE);
  __BewegeMit  = (char *)Ui(UI_VERB_MOVE_DIR);
}

void LangInit(void)
{
  char path[1024];
  int i;

  /* Reset UI overrides */
  for (i = 0; i < UI_COUNT; i++) {
    free(ui_str[i]);
    ui_str[i] = NULL;
  }
  /* Clear script table */
  for (i = 0; i < script_n; i++) {
    free(script_tab[i].key);
    free(script_tab[i].val);
  }
  free(script_tab);
  script_tab = NULL;
  script_n = script_cap = 0;

  if (!strcmp(lang_code, "de")) {
    apply_verb_globals();
    return;
  }

  /* Runtime language pack: only lang/<code>.po */
  if (open_lang_file(path, sizeof(path), "po") == 0) {
    load_po_file(path);
    fprintf(stderr, "moas: lang %s (%d translated script lines)\n", path, script_n);
  } else {
    fprintf(stderr, "moas: lang: missing lang/%s.po - UI/scripts stay German\n",
            lang_code);
  }

  apply_verb_globals();
  (void)lookup_buf;
}
