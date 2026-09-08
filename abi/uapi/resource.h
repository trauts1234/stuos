#ifndef UAPI_RESOURCE_H
#define UAPI_RESOURCE_H
#include "stdint.h"

typedef uint64_t rlim_t;

#define RLIM_INFINITY ((__rlim_t) -1)

struct rlimit {
    rlim_t rlim_cur;  /* Soft limit */
    rlim_t rlim_max;  /* Hard limit (ceiling for rlim_cur) */
};

#endif