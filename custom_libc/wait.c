

#include "sys/types.h"
#include "uapi/syscalls.h"
#include "uapi/types.h"
#include "sys/wait.h"

pid_t wait(int *status) {
    return waitpid(-1, status, 0);
}

pid_t waitpid(pid_t pid, int *status, int options) {
    pid_t output_pid = 67;
    struct WaitData data = {
        .pid = pid,
        .status = status,
        .options = options,
        .output_pid = &output_pid
    };
    do_syscall(&data, WAIT_SYSCALL);

    return output_pid;
}