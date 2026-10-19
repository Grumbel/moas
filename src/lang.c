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
  [UI_OV_SAVE]       = "ov.save",
  [UI_OV_LOAD]       = "ov.load",
  [UI_OV_ESC]        = "ov.esc",
  [UI_OV_SKIP]       = "ov.skip",
  [UI_OV_USE]        = "ov.use",
  [UI_OV_LOOK]       = "ov.look",
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

/* Minimal CP850 → UTF-8 for lookup keys. */
static void cp850_to_utf8(const unsigned char *in, char *out, size_t outsz)
{
  size_t o = 0;
  int i;
  for (i = 0; in[i] && o + 4 < outsz; i++) {
    unsigned char c = in[i];
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
        out[o++] = '?';
        continue;
    }
    while (*u && o + 1 < outsz)
      out[o++] = *u++;
  }
  out[o] = 0;
}

/* UTF-8 → CP850 for values shown with the game font (ASCII + German extras). */
static void utf8_to_cp850(const char *in, char *out, size_t outsz)
{
  size_t o = 0;
  const unsigned char *p = (const unsigned char *)in;
  while (*p && o + 1 < outsz) {
    if (*p < 0x80) {
      out[o++] = (char)*p++;
      continue;
    }
    /* 2-byte UTF-8 */
    if ((p[0] & 0xe0) == 0xc0 && p[1]) {
      unsigned int cp = ((p[0] & 0x1f) << 6) | (p[1] & 0x3f);
      unsigned char c = '?';
      if (cp == 0xe4) c = 0x84;      /* ä */
      else if (cp == 0xf6) c = 0x94; /* ö */
      else if (cp == 0xfc) c = 0x81; /* ü */
      else if (cp == 0xc4) c = 0x8e; /* Ä */
      else if (cp == 0xd6) c = 0x99; /* Ö */
      else if (cp == 0xdc) c = 0x9a; /* Ü */
      else if (cp == 0xdf) c = 0xe1; /* ß */
      out[o++] = (char)c;
      p += 2;
      continue;
    }
    /* skip other sequences */
    if ((p[0] & 0xf0) == 0xe0 && p[1] && p[2])
      p += 3;
    else if ((p[0] & 0xf8) == 0xf0 && p[1] && p[2] && p[3])
      p += 4;
    else
      p++;
    out[o++] = '?';
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

static void ui_set(int id, const char *text_utf8)
{
  char buf[256];
  if (id < 0 || id >= UI_COUNT || !text_utf8)
    return;
  utf8_to_cp850(text_utf8, buf, sizeof(buf));
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

static void trim_inplace(char *s)
{
  char *e;
  while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
    memmove(s, s + 1, strlen(s));
  e = s + strlen(s);
  while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n'))
    *--e = 0;
}

static void load_ui_file(const char *path)
{
  FILE *f = fopen(path, "rb");
  char line[512];
  if (!f)
    return;
  while (fgets(line, sizeof(line), f)) {
    char *tab, *val;
    int id;
    if (line[0] == '#' || line[0] == '\n' || line[0] == '\r')
      continue;
    tab = strchr(line, '\t');
    if (!tab)
      continue;
    *tab = 0;
    val = tab + 1;
    trim_inplace(line);
    trim_inplace(val);
    id = ui_id_from_name(line);
    if (id >= 0)
      ui_set(id, val);
  }
  fclose(f);
}

static void load_script_file(const char *path)
{
  FILE *f = fopen(path, "rb");
  char line[800];
  if (!f)
    return;
  while (fgets(line, sizeof(line), f)) {
    char *tab, *val;
    if (line[0] == '#' || line[0] == '\n' || line[0] == '\r')
      continue;
    tab = strchr(line, '\t');
    if (!tab)
      continue;
    *tab = 0;
    val = tab + 1;
    trim_inplace(line);
    trim_inplace(val);
    if (line[0] && val[0])
      script_add(line, val);
  }
  fclose(f);
  script_sort();
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

  if (open_lang_file(path, sizeof(path), "ui") == 0) {
    load_ui_file(path);
    fprintf(stderr, "moas: lang UI %s\n", path);
  } else {
    fprintf(stderr, "moas: lang: no %s.ui (using built-in German UI)\n", lang_code);
  }

  if (open_lang_file(path, sizeof(path), "script") == 0) {
    load_script_file(path);
    fprintf(stderr, "moas: lang script %s (%d entries)\n", path, script_n);
  } else {
    fprintf(stderr, "moas: lang: no %s.script (scripts stay German)\n", lang_code);
  }

  apply_verb_globals();
  (void)lookup_buf;
}
