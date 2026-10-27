/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef MOAS_LANG_H
#define MOAS_LANG_H

#include <stddef.h>

/*
 * Index-based UI strings + UnPack script translation (no gettext).
 *
 * UI:   Ui(UI_…) → table from lang/<code>.po (msgctxt) or .po
 * Script: LangLookup(UnPack) via same .po (msgid→msgstr) or .script
 *
 * Select with --lang=CODE or MOAS_LANG. Default "de" uses built-in German UI
 * and leaves script text as in GAME.PCK.
 */

enum {
  UI_VERB_USE = 0,
  UI_VERB_USE_WITH,
  UI_VERB_LOOK,
  UI_VERB_TALK,
  UI_VERB_MOVE,
  UI_VERB_MOVE_DIR,
  UI_SAVE_TITLE,
  UI_LOAD_TITLE,
  UI_SAVE_FAIL,
  UI_LOAD_FAIL,
  UI_SLOT_NONE,
  UI_SLOT_OF,       /* "%i von %i" / "%i of %i" */
  UI_OV_SAVE,
  UI_OV_LOAD,
  UI_OV_SKIP_SCENE,
  UI_OV_SKIP_TEXT,
  UI_OV_USE,
  UI_OV_LOOK,
  UI_OV_OPTIONS,
  UI_OV_HINT,
  UI_MENU_OPTIONS,  /* host entry in the game's own menu (prog 1) */
  /* SDL options menu (F12) and key legend; SDL font has A-Z only, no umlauts */
  UI_OPT_TITLE,
  UI_OPT_HINT_TOUCH,
  UI_OPT_HINT_KEYS,
  UI_OPT_LANGUAGE,
  UI_OPT_FULLSCREEN,
  UI_OPT_ASPECT,
  UI_OPT_TOUCH,
  UI_OPT_SPEED,
  UI_OPT_SPEECH,
  UI_OPT_RESUME,
  UI_OPT_RESTART,
  UI_OPT_RESTART_SURE,
  UI_OPT_QUIT,
  UI_OPT_SETTINGS,
  UI_OPT_BACK,
  UI_OPT_ON,
  UI_OPT_OFF,
  UI_KEY_SAVE,
  UI_KEY_LOAD,
  UI_KEY_OPTIONS,
  UI_KEY_SKIP_TEXT,
  UI_KEY_SKIP_SCENE,
  UI_KEY_HOTSPOTS,
  UI_COUNT
};

/* Built-in German fallbacks; Ui() never returns NULL. */
const char *Ui(int id);

/* Parse --lang / MOAS_LANG (call from MoasParseArgs). */
void LangSetCode(const char *code);

/* Load lang/<code>.po after datadir is known. Re-points verb globals. */
void LangInit(void);

/* After UnPack: if a translation exists, return CP850 bytes for the font. */
const char *LangLookup(const char *cp850_source);

const char *LangGetCode(void);

/* Finds lang/<rel> (rel e.g. "en.po" or "en/phase/phase_053.png") in
 * $MOAS_LANGDIR, next to the datadir, or in ./lang. 0 and the path in
 * `path` if found, else -1. */
int LangFindFile(char *path, size_t pathsz, const char *rel);

/* Languages with a pack in lang/ (plus built-in "de"), in options-menu order.
 * LangFind: the table's copy of `code`, or NULL if it isn't one of them. */
const char *LangFind(const char *code);
/* The language `dir` steps (+1/-1) after `code`, wrapping around. */
const char *LangStep(const char *code, int dir);

#endif
