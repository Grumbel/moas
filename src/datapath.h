/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef MOAS_DATAPATH_H
#define MOAS_DATAPATH_H

#ifdef __cplusplus
extern "C" {
#endif

void MoasInitDataPath(const char *argv0);
const char *MoasDataDir(void);
int MoasOpenData(const char *name);
void MoasReportMissingData(const char *name);

#ifdef __cplusplus
}
#endif

#endif
