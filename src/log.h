/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef MOAS_LOG_H
#define MOAS_LOG_H

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Diagnostic output of the port. One call is one line; a trailing '\n' in
 * the message is optional. Where it goes depends on the platform:
 *   desktop / wasm: stderr
 *   Android:        logcat (SDL_Log)
 *   DOS:            MOAS.LOG in the current directory — the console is the
 *                   VGA screen there, so printing would draw over the game.
 */
void MoasLog(const char *fmt, ...);
void MoasLogV(const char *fmt, va_list ap);

#ifdef __cplusplus
}
#endif

#endif
