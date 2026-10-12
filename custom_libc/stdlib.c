#include "stdlib.h"
#include "uapi/stddef.h"
#include "uapi/syscalls.h"
#include "stdio.h"
#include "unistd.h"
#include <assert.h>
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

int abs(int j) {
    return (j<0) ? -j : j;
}
long int labs(long int j){
    return (j<0l) ? -j : j;
}
long long int llabs(long long int j){
    return (j<0ll) ? -j : j;
}

char *getenv(const char *name) {
    size_t len;
    char **env;

    assert(name != NULL && strchr(name, '=') == NULL);

    len = strlen(name);

    for (env = environ; env != NULL && *env != NULL; env++) {
        if (strncmp(*env, name, len) == 0 && (*env)[len] == '=') {
            return *env + len + 1;
        }
    }

    return NULL;
}