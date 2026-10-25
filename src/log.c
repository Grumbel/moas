/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "log.h"

#include <stdio.h>
#include <string.h>
#ifdef __ANDROID__
#include <SDL.h>
#endif

#ifdef DOS
#define MOAS_LOG_FILE "MOAS.LOG"

static FILE *log_file;
static int log_failed;

/* Truncated on the first message of a run, so it holds only this session. */
static FILE *log_open(void)
{
  if (!log_file && !log_failed) {
    log_file = fopen(MOAS_LOG_FILE, "w");
    if (!log_file) log_failed = 1;
  }
  return log_file;
}
#endif

void MoasLogV(const char *fmt, va_list ap)
{
  char buf[1024];
  size_t n;

  vsnprintf(buf, sizeof(buf), fmt, ap);
  n = strlen(buf);
  if (n && buf[n - 1] == '\n') buf[--n] = 0;

#if defined(DOS)
  {
    FILE *f = log_open();
    if (f) {
      fprintf(f, "%s\n", buf);
      fflush(f); /* keep the line if we crash or hang right after */
    }
  }
#elif defined(__ANDROID__)
  SDL_Log("%s", buf);
#else
  fprintf(stderr, "%s\n", buf);
#endif
}

void MoasLog(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  MoasLogV(fmt, ap);
  va_end(ap);
}
