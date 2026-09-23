#include "uapi/syscalls.h"
int sched_yield() {
    do_syscall(0, YIELD_SYSCALL);
    return 0;
}