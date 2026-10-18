/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef MOAS_PLATFORM_H
#define MOAS_PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif

/* Initialise video, input and timer. Returns 0 on success. */
int PlatformInit(void);

/* Shut everything down. */
void PlatformShutdown(void);

/* Pump SDL events and update the global Mouse struct + keyboard state. */
void PlatformPumpEvents(void);

/* Present the current 320x200 Screen buffer + Palette to the window.
 * Call after any full or partial redraw that should become visible. */
void PlatformPresent(void);

/* Return milliseconds since start (compatible with original TimeCounter scale). */
short PlatformGetTicker(void);

/* Sleep a few ms so we don't spin the CPU. */
void PlatformDelay(unsigned int ms);

/* Non-zero after SDL_QUIT / explicit quit request. */
int PlatformShouldQuit(void);

/* Request process exit (window close, Q, Alt+F4, etc.). */
void PlatformRequestQuit(void);

/* Keyboard (DOS-style) */
int PlatformKbhit(void);
int PlatformShiftDown(void);
int PlatformGetch(void);


/* Optional: set the logical palette (VGA 0-63 values expected). */
void PlatformSetPalette(const unsigned char *pal768);

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
