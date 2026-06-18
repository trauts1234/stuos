#include "signal.h"
#include "uapi/syscalls.h"
#include "stdint.h"

int sigfillset(sigset_t *set) {
    *set = UINT32_MAX;
    return 0;
}

int sigemptyset(sigset_t *set) {
    *set = 0;
    return 0;
}

int sigprocmask(int how, const sigset_t *set, sigset_t *oldset) {
    struct SigProcMaskData data = {
        .how=how,
        .set = set,
    };
    do_syscall(&data, SIGPROCMASK_SYSCALL);
    if(oldset) *oldset = data.oldset;
    return 0;
}