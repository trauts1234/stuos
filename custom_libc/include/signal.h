#ifndef SIGNAL_H
#define SIGNAL_H

#include "uapi/signal.h"

void signal_do_nothing(int _);
#define SIG_IGN signal_do_nothing
#define SIG_DFL 0

int sigfillset(sigset_t *set);
int sigemptyset(sigset_t *set);

int sigprocmask(int how, const sigset_t *set, sigset_t *oldset);
sighandler_t signal(int signum, sighandler_t handler);
int raise(int sig);

int kill(pid_t pid, int sig);

#endif