#include "sys/stat.h"
#include "uapi/syscalls.h"

//TODO:
//when a user calls open(file, mask) or similar, do mask = mask & ~_file_mode_creation_mask
static mode_t _file_mode_creation_mask = 0777;
mode_t umask(mode_t mask) {
    mode_t prev = _file_mode_creation_mask;
    _file_mode_creation_mask = mask & 0777;
    return prev;
}

int stat(const char *path, struct stat *buf) {
    struct StatData data = {
        .path = path
    };
    do_syscall(&data, STAT_SYSCALL);
    *buf = data.result;
    return 0;
}
int lstat(const char *path, struct stat *buf) {
    //if symlink, stat the link, not the file being linked to
    return stat(path, buf);
}