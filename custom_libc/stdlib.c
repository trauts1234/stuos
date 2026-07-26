#include "stdlib.h"
#include "uapi/stddef.h"
#include "uapi/syscalls.h"
#include "stdio.h"
#include <string.h>

void exit(int status) {
    struct HaltSyscallData data = {
        .exit_code = status
    };
    do_syscall(&data, HALT_SYSCALL);
    while(1) {printf("ERROR: exit() returned somehow!\n");}
}
void abort() {
    //TODO send SIGABRT
    exit(0);
}

void *calloc(size_t nmemb, size_t size) {
    void *region = malloc(nmemb * size);
    if(region == 0) return 0;
    memset(region, 0, nmemb * size);
    return region;
}

int atoi(const char *nptr) {
    return strtol(nptr, NULL, 10);
}
double atof(const char *nptr) {
    return strtod(nptr, NULL);
}