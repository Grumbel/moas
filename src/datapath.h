/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef MOAS_DATAPATH_H
#define MOAS_DATAPATH_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void MoasInitDataPath(const char *argv0);
const char *MoasDataDir(void);

/* Writable state directory (savegames). Created on first use.
 * Linux: $XDG_STATE_HOME/moas or ~/.local/state/moas
 * Windows: %LOCALAPPDATA%\\moas\\state
 * Android: <internal>/moas-state
 * Emscripten: /moas-state (pair with IDBFS)
 * DOS: "." (CWD; DOSBox runner persists *.SAV to host state dir)
 */
const char *MoasStateDir(void);

/* Config directory (settings.ini, see settings.h).
 * Linux: $XDG_CONFIG_HOME/moas or ~/.config/moas
 * Windows: %APPDATA%\\moas
 * Android: <internal>/moas-config
 * Emscripten: /moas-config
 * DOS: "."
 */
const char *MoasConfigDir(void);

/* Build absolute path: state_dir/name into buf. Returns 0 on success, -1 on error. */
int MoasStatePath(char *buf, size_t buflen, const char *name);

/* Open a file under the state directory. mode: O_RDONLY or write flags.
 * On failure returns -1 and sets errno. */
int MoasOpenState(const char *name, int flags, int mode);

int MoasOpenData(const char *name);
void MoasReportMissingData(const char *name);

/* Print data / state / config directories once at startup. */
void MoasLogDirectories(void);

#ifdef __cplusplus
}
#endif

#endif
