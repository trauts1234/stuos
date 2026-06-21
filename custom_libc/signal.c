#include "signal.h"
#include "uapi/syscalls.h"
#include "stdint.h"
#include "unistd.h"

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

void signal_do_nothing(int _) {
}

sighandler_t signal(int signum, sighandler_t handler) {
    struct SetSignalHandlerData data = {
        .signal_number = signum,
        .handler = handler
    };
    do_syscall(&data, SETSIGNALHANDLER_SYSCALL);
    return data.old_handler;
}

int raise(int sig) {
    //only works on single threaded programs.
    //try pthread_kill(pthread_self(), sig);
    return kill(getpid(), sig);
}

int kill(pid_t pid, int sig) {
    struct KillData data = {
        .pid=pid,
        .sig=sig
    };
    do_syscall(&data, KILL_SYSCALL);
    return 0;
}