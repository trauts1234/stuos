#ifndef STAT_H
#define STAT_H

#include "types.h"
#include "uapi/stat.h"

mode_t umask(mode_t mask);
int stat64(const char *path, struct stat64 *buf);

#endif