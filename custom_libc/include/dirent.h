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

#endif