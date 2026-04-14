#ifndef UNISTD_H
#define UNISTD_H

#include "sys/types.h"
#include "stddef.h"

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

extern char **environ;

pid_t fork();
pid_t vfork();

gid_t getgid(void);
gid_t getegid(void);
int setgid(gid_t gid);

uid_t getuid(void);
uid_t geteuid(void);
int setuid(uid_t uid);

pid_t getpgrp(void); 

pid_t getpid(void);

int dup2(int oldfd, int newfd);

ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
int close(int fd);

char *getcwd(char *buf, size_t size);
int chdir(const char *path);

int execve(const char *filename, char *const argv[], char *const envp[]);
int execvp(const char *file, char *const argv[]);

int isatty(int fd);

int pipe(int pipefd[2]);

#endif