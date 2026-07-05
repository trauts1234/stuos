#ifndef DIRENT_H
#define DIRENT_H

#include "uapi/dirent.h"

typedef struct {
    int file_descriptor_number;
    struct dirent current;
} DIR;

DIR *opendir(const char *name);
int closedir(DIR *dirp);

struct dirent *readdir(DIR *dirp);

struct dirent64 {
    ino_t d_ino;
    unsigned char d_type;
    
    char d_name[256];
};
struct dirent64 *readdir64(DIR *dirp);

#endif