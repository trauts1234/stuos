#ifndef UAPI_DIRENT_H
#define UAPI_DIRENT_H

#include "stdint.h"
#include "types.h"

struct dirent {
    ino_t d_ino;
    off_t d_off;

    unsigned short int d_reclen;
    char d_name[256];
};

#endif