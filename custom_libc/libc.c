#include "stdio.h"
#include "nonstandard.h"
#include "uapi/syscalls.h"
#include <stdint.h>

//This is a minimal libc, with some extra functions to control OS-specific features like the display

void debug_print(char* data) {
    printf("%s", data);
}
void debug_int(uint64_t data) {
    printf("%llu", data);
}
void debug_hex(uint64_t data) {
    printf("%llx", data);
}

int getchar_nonblocking(int* pressed) {
    struct GetCharNonblockingData data = {.output=0};
    do_syscall(&data, GETCHARNONBLOCKING_SYSCALL);
    *pressed = data.pressed;
    return data.output;
}

void write_pixel(uint64_t x, uint64_t y, uint8_t r, uint8_t g, uint8_t b) {
    struct WritePixelData data = {.x=x, .y=y, .r=r, .g=g, .b=b};
    do_syscall(&data, WRITEPIXEL_SYSCALL);
}

void clear_screen() {
    do_syscall(0, CLEARSCREEN_SYSCALL);
}

uint64_t get_uptime_ms() {
    struct GetUptimeMsData data = {.ms=0};
    do_syscall(&data, GET_UPTIME_MS_SYSCALL);
    return data.ms;
}