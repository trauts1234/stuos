#ifndef SIGNAL_H
#define SIGNAL_H

#include "uapi/signal.h"

int sigfillset(sigset_t *set);
int sigemptyset(sigset_t *set);

int sigprocmask(int how, const sigset_t *set, sigset_t *oldset);

int kill(pid_t pid, int sig);//TODO

#endif