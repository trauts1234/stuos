#include "nonstandard.h"
#include "uapi/syscalls.h"
#include <stdint.h>

void clear_screen() {
    do_syscall(0, CLEARSCREEN_SYSCALL);
}

uint64_t get_uptime_ms() {
    struct GetUptimeMsData data = {.ms=0};
    do_syscall(&data, GET_UPTIME_MS_SYSCALL);
    return data.ms;
}