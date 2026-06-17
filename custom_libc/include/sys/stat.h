#ifndef STAT_H
#define STAT_H

#include "types.h"
#include "uapi/stat.h"

mode_t umask(mode_t mask);
int stat(const char *path, struct stat *buf);

#endif