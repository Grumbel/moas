/* SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com> */
/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef MOAS_LANG_H
#define MOAS_LANG_H

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
  UI_OV_ESC,
  UI_OV_SKIP,
  UI_OV_USE,
  UI_OV_LOOK,
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

#endif
