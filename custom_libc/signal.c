#include "signal.h"
#include <stdint.h>

int sigfillset(sigset_t *set) {
    *set = UINT32_MAX;
    return 0;
}

int sigemptyset(sigset_t *set) {
    *set = 0;
    return 0;
}