#include "termios.h"
#include "uapi/syscalls.h"
#include "errno.h"

int tcgetattr(int fd, struct termios *termios_p) {
    struct TcGetAttrData data = {
        .fd = fd,
    };
    do_syscall(&data, TCGETATTR_SYSCALL);
    if(data.err != 0) {
        errno = data.err;
        return -1;
    }

    *termios_p = data.output;
    return 0;
}