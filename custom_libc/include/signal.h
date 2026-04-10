#ifndef SIGNAL_H
#define SIGNAL_H

#include "sys/types.h"
#include <stdint.h>
typedef uint32_t sigset_t;

typedef int sig_atomic_t;

int sigfillset(sigset_t *set);
int sigemptyset(sigset_t *set);

int kill(pid_t pid, int sig);//TODO

#endif