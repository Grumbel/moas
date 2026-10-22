/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef MOAS_SETTINGS_H
#define MOAS_SETTINGS_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Port-side user settings in <config dir>/settings.ini (key=value lines):
 * language, fullscreen, touch buttons, game/speech speed, volume.
 * Command line and MOAS_LANG win over the file. Game and speech speed are a
 * user setting: savegames still store them but loading keeps the current ones.
 */
#ifdef DOS
#define MoasSettingsLoad()      ((void)0)
#define MoasSettingsApplyGame() ((void)0)
#define MoasSettingsSave()      ((void)0)
#else
/* After MoasInitDataPath, before LangInit / InitAll. */
void MoasSettingsLoad(void);
/* After InitAll: speeds and volume (InitAll resets them). */
void MoasSettingsApplyGame(void);
/* Write the current state (options menus closed, shutdown). */
void MoasSettingsSave(void);
#endif

#ifdef __cplusplus
}
#endif

#endif
