#ifndef SIGNAL_H
#define SIGNAL_H

#include "sys/types.h"
#include "stdint.h"
typedef uint32_t sigset_t;

typedef int sig_atomic_t;

int sigfillset(sigset_t *set);
int sigemptyset(sigset_t *set);

#define SIG_BLOCK 0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2
int sigprocmask(int how, const sigset_t *set, sigset_t *oldset);

int kill(pid_t pid, int sig);//TODO

#endif