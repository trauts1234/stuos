#include "sys/types.h"
#include "uapi/syscalls.h"
#include <stdarg.h>

int open(const char *pathname, int flags, ...) {
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }
    //ignores mode
    struct OpenFileData data = {
        .path = pathname,
        .output_file_descriptor_number = 0,
        .open_flags = flags,
    };

    do_syscall(&data, OPEN_FILE_SYSCALL);
    
    return data.output_file_descriptor_number;
}

int open64(const char *pathname, int flags, ...) {
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }
    
    return open(pathname, flags, mode);
}