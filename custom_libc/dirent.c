#include "dirent.h"
#include "fcntl.h"
#include "uapi/fcntl.h"
#include "uapi/syscalls.h"
#include "stdlib.h"
#include "stdint.h"

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

struct dirent *readdir(DIR *dirp) {
    struct ReadFDData data = {
        .file_descriptor_number = dirp->file_descriptor_number,
        .buffer = (uint8_t*)&dirp->current,
        .num_bytes = sizeof(struct dirent),
        .num_bytes_actually_read = 0
    };
    
    do_syscall(&data, READ_FD_SYSCALL);

    if(data.num_bytes_actually_read != sizeof(struct dirent)) {
        //TODO what if I read part of a dirent randomly
        return NULL;
    } else {
        return &dirp->current;
    }
}
struct dirent64 *readdir64(DIR *dirp) {
    return (struct dirent64*)readdir(dirp);
}