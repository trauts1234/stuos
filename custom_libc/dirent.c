#include "dirent.h"
#include "fcntl.h"
#include "uapi/fcntl.h"
#include "uapi/syscalls.h"
#include "stdlib.h"

DIR *opendir(const char *name) {
    int fd = open(name, O_DIRECTORY);

    DIR* output = malloc(sizeof(DIR));
    *output = (DIR) {
        .file_descriptor_number = fd
    };
    return output;
}

int closedir(DIR *dirp) {
    struct CloseFDData data = {
        .file_descriptor_number = dirp->file_descriptor_number,
    };

    do_syscall(&data, CLOSE_FD_SYSCALL);
    free(dirp);

    return 0;
}