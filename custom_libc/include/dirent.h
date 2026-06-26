#ifndef DIRENT_H
#define DIRENT_H

#include "uapi/dirent.h"
//todo "ls" style syscalls?

typedef struct {
    int file_descriptor_number;
} DIR;

#endif