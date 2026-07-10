#include "stdio.h"
#include "nonstandard.h"
#include "uapi/syscalls.h"
#include <stdint.h>

//used by the malloc thing, when errors occur
void debug_print(char* data) {
    printf("%s", data);
}
void debug_int(uint64_t data) {
    printf("%llu", data);
}
void debug_hex(uint64_t data) {
    printf("%llx", data);
}

void clear_screen() {
    do_syscall(0, CLEARSCREEN_SYSCALL);
}

uint64_t get_uptime_ms() {
    struct GetUptimeMsData data = {.ms=0};
    do_syscall(&data, GET_UPTIME_MS_SYSCALL);
    return data.ms;
}