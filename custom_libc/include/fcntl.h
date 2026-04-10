#ifndef FCNTL_H
#define FCNTL_H

#include "uapi/fcntl.h"

int open(const char *pathname, int flags, ...);
int open64(const char *pathname, int flags, ...);

#endif