#ifndef __WMT_LOADER_H_
#define __WMT_LOADER_H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <cutils/properties.h>
#include <cutils/misc.h>
#include <sys/ioctl.h>
#include <private/android_filesystem_config.h>
#include <utils/Log.h>

extern int init_module(void *, unsigned long, const char *);
extern int delete_module(const char *, unsigned int);
extern int load_fm_module(int chip_id);
extern int load_wifi_module(int chip_id);
extern int load_ant_module(int chip_id);

#endif
