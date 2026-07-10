#ifndef WAIT_H
#define WAIT_H

#include "uapi/wait.h"
#include "types.h"

pid_t wait(int *status);

pid_t waitpid(pid_t pid, int *status, int options);

#define WIFEXITED(status) ((status & WIFEXITED_MASK) != 0)
#define WEXITSTATUS(status) (status & WEXITSTATUS_MASK)
//TODO there's more info that should be stored in status

#endif