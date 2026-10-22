/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef MOAS_PLATFORM_H
#define MOAS_PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif

/* Initialise video, input and timer. Returns 0 on success. */
int PlatformInit(void);
/* Android: extract APK assets to internal storage and set MOAS_DATADIR. */
void PlatformAndroidPrepareData(void);

/* Shut everything down. */
void PlatformShutdown(void);

/* Pump SDL events and update the global Mouse struct + keyboard state. */
void PlatformPumpEvents(void);

/* Present the current 320x200 Screen buffer + Palette to the window.
 * Call after any full or partial redraw that should become visible. */
void PlatformPresent(void);

/* Return milliseconds since start (compatible with original TimeCounter scale). */
short PlatformGetTicker(void);
/* Milliseconds since start. */
unsigned int PlatformGetMillis(void);

/* Sleep a few ms so we don't spin the CPU. */
void PlatformDelay(unsigned int ms);

/* Non-zero after SDL_QUIT / explicit quit request. */
int PlatformShouldQuit(void);

/* Touch overlay: 1=Save 2=Load pending after button release; 0 if none. */
int PlatformPollOverlayCmd(void);

/* Request process exit (window close, Q, Alt+F4, etc.). */
void PlatformRequestQuit(void);

/* Unwind like a quit (no autosave), then main() starts a new game. */
void PlatformRequestRestart(void);
/* Non-zero once if a restart was requested; resets the quit state. */
int PlatformTakeRestart(void);

/* Modal engine settings (language, speed, fullscreen). F12. */
void PlatformEngineMenu(void);

/* Current window / overlay state, for settings.ini. */
int PlatformIsFullscreen(void);
int PlatformTouchControlsOn(void);

/* Default language from the OS locale: "de" for German, else "en". */
const char *PlatformPreferredLanguage(void);

/* Touch interaction mode: 0=USE (default verbs), 1=LOOK (SIEHE on object tap). */
#define MOAS_TOUCH_USE  0
#define MOAS_TOUCH_LOOK 1
#ifdef DOS
/* No touch overlay on real DOS; always classic mouse verbs. */
#define PlatformGetTouchMode() MOAS_TOUCH_USE
#else
int PlatformGetTouchMode(void);
#endif

/* Keyboard (DOS-style) */
int PlatformKbhit(void);
int PlatformShiftDown(void);
int PlatformGetch(void);


/* Set the displayed (DAC) palette; VGA 6-bit values, upper bits ignored. */
void PlatformSetPalette(const unsigned char *pal768);
/* One DAC entry (outp 0x3c8/0x3c9 equivalent). */
void PlatformSetPaletteEntry(int index, int r, int g, int b);

/* Warp OS cursor so soft cursor and SDL agree (game coords 0..319 / 0..199). */
void PlatformSetMousePos(short x, short y);

/* Soft screen shake offset in pixels (logical). */
void PlatformSetShake(int ox, int oy);

/* 1 = force black present (ToggleScreen off). */
void PlatformSetBlank(int blank);


#ifdef __cplusplus
}
#endif

#endif /* MOAS_PLATFORM_H */
