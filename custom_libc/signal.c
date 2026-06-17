#include "signal.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

int sigfillset(sigset_t *set) {
    *set = UINT32_MAX;
    return 0;
}

int sigemptyset(sigset_t *set) {
    *set = 0;
    return 0;
}

int sigprocmask(int how, const sigset_t *set, sigset_t *oldset) {
    //TODO
    printf("TODO sigprocmask");
    abort();
}