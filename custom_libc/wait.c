

#include "sys/types.h"
#include "uapi/syscalls.h"
#include "sys/wait.h"

pid_t wait(int *status) {
    return waitpid(-1, status, 0);
}

pid_t waitpid(pid_t pid, int *status, int options) {
    struct WaitData data = {
        .pid = pid,
        .status = status,
        .options = options,
    };
    do_syscall(&data, WAIT_SYSCALL);

    return data.output_pid;
}