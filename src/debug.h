/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef MOAS_DEBUG_H
#define MOAS_DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif

/* Bit flags — combine freely */
#define MOAS_DBG_SCRIPT   (1u << 0)  /* Execute opcodes */
#define MOAS_DBG_INPUT    (1u << 1)  /* mouse/keyboard edges */
#define MOAS_DBG_WAIT     (1u << 2)  /* GetClickOrEnter / Wait / selects */
#define MOAS_DBG_WINDOW   (1u << 3)  /* Open/Close window, cursor state */
#define MOAS_DBG_LOAD     (1u << 4)  /* data loads */
#define MOAS_DBG_ALL      (0xffffu)

extern unsigned moas_debug;
extern int moas_skip_intro;   /* skip Execute(3,0) */
extern int moas_start_proc;   /* -1 = default 0 after intro */
extern int moas_start_addr;

void MoasParseArgs(int argc, char **argv);
void MoasDbg(unsigned flag, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif
