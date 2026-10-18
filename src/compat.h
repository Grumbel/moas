/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef MOAS_COMPAT_H
#define MOAS_COMPAT_H

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#ifndef DOS

/* DOS I/O port stubs */
static inline unsigned char inp(unsigned short port) { (void)port; return 0; }
static inline void outp(unsigned short port, unsigned char val) { (void)port; (void)val; }

/* Interrupt control stubs */
static inline void _disable(void) {}
static inline void _enable(void) {}

/* conio replacements — backed by SDL key queue in platform_sdl.c */
int PlatformKbhit(void);
int PlatformGetch(void);
static inline int kbhit(void) { return PlatformKbhit(); }
static inline int getch(void) { return PlatformGetch(); }

/* min/max macros (common in old DOS code) */
#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif

#endif /* !DOS */

#endif /* MOAS_COMPAT_H */

#ifndef DOS
#include <stdio.h>
#include <sys/stat.h>

/* Borland / MSC helpers */
static inline char *itoa(int value, char *str, int base) {
  if (base == 10) sprintf(str, "%d", value);
  else if (base == 16) sprintf(str, "%x", value);
  else sprintf(str, "%d", value);
  return str;
}
static inline char *ltoa(long value, char *str, int base) {
  if (base == 10) sprintf(str, "%ld", value);
  else sprintf(str, "%ld", value);
  return str;
}
static inline long filelength(int fd) {
  struct stat st;
  if (fstat(fd, &st) == 0) return (long)st.st_size;
  return -1L;
}
static inline void _setcolor(short c) { (void)c; }
static inline unsigned long _memmax(void) { return 0; }
#endif

#ifndef S_IWRITE
#define S_IWRITE 0200
#endif

/* Case-insensitive open for DOS-era filenames on case-sensitive filesystems.
 * Tries the given name first, then common case variants of the basename. */
static inline int open_ci(const char *path, int flags, ...)
{
  int fd = open(path, flags);
  if (fd >= 0) return fd;

  /* Try uppercase and lowercase of the whole path (simple but enough for GAME.*/
  char buf[256];
  size_t n = strlen(path);
  if (n >= sizeof(buf)) return -1;
  for (size_t i = 0; i < n; i++) buf[i] = (char)toupper((unsigned char)path[i]);
  buf[n] = 0;
  fd = open(buf, flags);
  if (fd >= 0) return fd;
  for (size_t i = 0; i < n; i++) buf[i] = (char)tolower((unsigned char)path[i]);
  buf[n] = 0;
  fd = open(buf, flags);
  return fd;
}
