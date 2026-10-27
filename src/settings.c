/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "settings.h"
#include "datapath.h"
#include "debug.h"
#include "lang.h"
#include "platform.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

extern short TotalSpeed, SpeechSpeed;
extern unsigned short GlobalVolume;

#define SETTINGS_FILE "settings.ini"

/* -1: not in the file. */
static int cfg_total_speed = -1;
static int cfg_speech_speed = -1;
static int cfg_volume = -1;
static char last_written[256];

static int settings_path(char *buf, size_t buflen, const char *suffix)
{
  int n = snprintf(buf, buflen, "%s/%s%s", MoasConfigDir(), SETTINGS_FILE, suffix);
  return (n < 0 || (size_t)n >= buflen) ? -1 : 0;
}

static int parse_int(const char *s, int lo, int hi)
{
  char *end;
  long v = strtol(s, &end, 10);
  if (end == s || v < lo || v > hi)
    return -1;
  return (int)v;
}

static int parse_bool(const char *s)
{
  if (!strcmp(s, "1") || !strcmp(s, "on") || !strcmp(s, "true") || !strcmp(s, "yes"))
    return 1;
  if (!strcmp(s, "0") || !strcmp(s, "off") || !strcmp(s, "false") || !strcmp(s, "no"))
    return 0;
  return -1;
}

static char *trim(char *s)
{
  char *e;
  while (*s == ' ' || *s == '\t')
    s++;
  e = s + strlen(s);
  while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n'))
    *--e = 0;
  return s;
}

void MoasSettingsLoad(void)
{
  char path[4096], line[256];
  FILE *f;

  if (settings_path(path, sizeof(path), "") != 0)
    return;
  f = fopen(path, "r");
  if (!f)
    return;
  while (fgets(line, sizeof(line), f)) {
    char *key = trim(line), *val, *eq;
    int b;
    if (!*key || *key == '#' || *key == ';' || *key == '[')
      continue;
    eq = strchr(key, '=');
    if (!eq)
      continue;
    *eq = 0;
    key = trim(key);
    val = trim(eq + 1);
    if (!strcmp(key, "language")) {
      if (!moas_lang_explicit && *val)
        LangSetCode(val);
    } else if (!strcmp(key, "fullscreen")) {
#if !defined(__EMSCRIPTEN__) && !defined(__ANDROID__)
      /* Browsers only go fullscreen from a user gesture; Android always is. */
      b = parse_bool(val);
      if (moas_fullscreen < 0 && b >= 0)
        moas_fullscreen = b;
#endif
    } else if (!strcmp(key, "touch_buttons")) {
#ifndef __ANDROID__
      /* Android: the MENU button is the only way into the options menu,
       * so an older settings.ini with touch_buttons=0 must not hide it. */
      b = parse_bool(val);
      if (moas_touch_controls < 0 && b >= 0)
        moas_touch_controls = b;
#endif
    } else if (!strcmp(key, "aspect")) {
      b = MoasParseAspect(val);
      if (moas_aspect < 0 && b >= 0)
        moas_aspect = b;
    } else if (!strcmp(key, "game_speed")) {
      cfg_total_speed = parse_int(val, 1, 8);
    } else if (!strcmp(key, "speech_speed")) {
      cfg_speech_speed = parse_int(val, 1, 24);
    } else if (!strcmp(key, "volume")) {
      cfg_volume = parse_int(val, 0, 255);
    }
  }
  fclose(f);
  MoasDbg(MOAS_DBG_LOAD, "settings loaded from %s", path);
}

void MoasSettingsApplyGame(void)
{
  if (cfg_total_speed > 0)
    TotalSpeed = (short)cfg_total_speed;
  if (cfg_speech_speed > 0)
    SpeechSpeed = (short)cfg_speech_speed;
  if (cfg_volume >= 0)
    GlobalVolume = (unsigned short)cfg_volume;
}

void MoasSettingsSave(void)
{
  char path[4096], tmp[4096], text[sizeof(last_written)];
  const char *lang = LangGetCode();
  FILE *f;
  int n;

  n = snprintf(text, sizeof(text),
               "# MOAS settings (written by the game; F12 options menu)\n"
               "language=%s\n"
               "fullscreen=%d\n"
               "touch_buttons=%d\n"
               "aspect=%s\n"
               "game_speed=%d\n"
               "speech_speed=%d\n"
               "volume=%d\n",
               (lang && lang[0]) ? lang : "de",
               PlatformIsFullscreen(), PlatformTouchControlsOn(),
               PlatformAspect43() ? "4:3" : "8:5",
               (int)TotalSpeed, (int)SpeechSpeed, (int)GlobalVolume);
  if (n < 0 || (size_t)n >= sizeof(text) || !strcmp(text, last_written))
    return;
  if (settings_path(path, sizeof(path), "") != 0 ||
      settings_path(tmp, sizeof(tmp), ".tmp") != 0)
    return;
  f = fopen(tmp, "w");
  if (!f) {
    MoasLog("moas: cannot write %s\n", tmp);
    return;
  }
  if (fputs(text, f) < 0) {
    fclose(f);
    remove(tmp);
    return;
  }
  if (fclose(f) != 0) {
    remove(tmp);
    return;
  }
#ifdef _WIN32
  remove(path); /* rename does not replace on Windows */
#endif
  if (rename(tmp, path) != 0) {
    MoasLog("moas: cannot replace %s\n", path);
    remove(tmp);
    return;
  }
  strcpy(last_written, text);
#ifdef __EMSCRIPTEN__
  EM_ASM({
    if (typeof FS !== 'undefined' && FS.syncfs) {
      FS.syncfs(false, function(err) {
        if (err) console.warn('moas: IDBFS sync failed', err);
      });
    }
  });
#endif
}
