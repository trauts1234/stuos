#include "stdlib.h"
#include "uapi/stddef.h"
#include "uapi/syscalls.h"
#include "rust_bindings.h"
#include "stdio.h"

extern struct MemoryAllocator memory_allocator;

void* malloc(unsigned long bytes){
    return allocate(&memory_allocator, bytes);
}
void free(void* ptr) {
    deallocate(&memory_allocator, ptr);
}

void exit(int status) {
    //TODO exit code
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