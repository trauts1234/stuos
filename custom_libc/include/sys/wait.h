#ifndef WAIT_H
#define WAIT_H

#include "types.h"

pid_t wait(int *status);

pid_t waitpid(pid_t pid, int *status, int options);

#endif