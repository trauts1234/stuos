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
    do_syscall(0, HALT_SYSCALL);
    while(1) {printf("ERROR: exit() returned somehow!\n");}
}
void abort() {
    do_syscall(0, HALT_SYSCALL);
    while(1) {printf("ERROR: abort() returned somehow!\n");}
}

int atoi(const char *nptr) {
    return strtol(nptr, NULL, 10);
}
double atof(const char *nptr) {
    return strtod(nptr, NULL);
}