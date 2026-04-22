#ifndef DIRENT_H
#define DIRENT_H
#include "stdint.h"
//todo "ls" style syscalls?

typedef uint64_t ino_t;
typedef int64_t off_t;

struct dirent {
    ino_t d_ino;
    off_t d_off;

    unsigned short int d_reclen;
    // unsigned char d_type; //TODO unify internal VNODE_FILE, VNODE_DIR with this enum
    char d_name[256];
};

#endif