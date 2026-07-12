#include "stdlib.h"
#include "uapi/stddef.h"
#include "uapi/syscalls.h"
#include "stdio.h"

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

int atoi(const char *nptr) {
    return strtol(nptr, NULL, 10);
}
double atof(const char *nptr) {
    return strtod(nptr, NULL);
}