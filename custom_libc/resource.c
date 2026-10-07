#include "uapi/syscalls.h"
#include <sys/resource.h>

int getrlimit(int resource, struct rlimit *rlim){
    struct GetRLimitData data = {
        .resource = resource,
    };
    do_syscall(&data, GETRLIMIT_SYSCALL);
    *rlim = data.limit;
    return 0;
}
int setrlimit(int resource, const struct rlimit *rlim) {
    struct SetRLimitData data = {
        .resource = resource,
        .limit = *rlim
    };
    do_syscall(&data, SETRLIMIT_SYSCALL);
    return 0;
}