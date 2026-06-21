#ifndef UAPI_SIGNAL_H
#define UAPI_SIGNAL_H

#include "types.h"
#include "stdint.h"

#define SIG_BLOCK 0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2

#define SIGINT 0
#define SIGKILL 1

typedef uint32_t sigset_t;
typedef int sig_atomic_t;
typedef void (*sighandler_t)(int);

#endif