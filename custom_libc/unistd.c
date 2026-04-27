#include "nonstandard.h"
#include "stddef.h"
#include "uapi/syscalls.h"
#include "sys/types.h"
#include <stdint.h>
#include <stdlib.h>
#include "stdio.h"
#include "unistd.h"

char **environ = {NULL};

int fork() {
    struct ForkData data = {
        .pid = -1
    };

    do_syscall(&data, FORK_SYSCALL);
    
    return data.pid;
}

int vfork() {
    return fork();
}

//cache the gid, as I am root and I don't care about permissions
static gid_t _current_gid = 0;
gid_t getgid(void) {
    return _current_gid;
}
gid_t getegid(void) {
    return _current_gid;
}
int setgid(gid_t gid) {
    _current_gid = gid;
    return 0;
}

static uid_t _current_uid = 0;
uid_t getuid(void) {
    return _current_uid;
}
uid_t geteuid(void) {
    return _current_uid;
}
int setuid(uid_t uid) {
    _current_uid = uid;
    return 0;
}

pid_t getpgrp(void) {
    struct GetPgrpData data;
    do_syscall(&data, GET_PGRP_SYSCALL);

    return data.result;
}

pid_t getpid(void) {
    struct GetPidData data;
    do_syscall(&data, GET_PID_SYSCALL);

    return data.result;
}

int dup2(int oldfd, int newfd) {
    struct Dup2Data data = {
        .oldfd = oldfd,
        .newfd = newfd
    };
    do_syscall(&data, DUP2_SYSCALL);

    return newfd;
}

ssize_t read(int fd, void *buf, size_t count) {
    struct ReadFDData data = {
        .file_descriptor_number = fd,
        .buffer = buf,
        .num_bytes = count,
        .num_bytes_actually_read = 0
    };
    
    do_syscall(&data, READ_FD_SYSCALL);

    return data.num_bytes_actually_read;
}
ssize_t write(int fd, const void *buf, size_t count) {
    struct WriteFDData data = {.file_descriptor_number=fd, .buffer=buf, .num_bytes=count};
    do_syscall(&data, WRITE_FD_SYSCALL);
    return data.num_bytes_actually_written;
}
int close(int fd) {
    struct CloseFDData data = {
        .file_descriptor_number = fd,
        .error_code = 0
    };

    do_syscall(&data, CLOSE_FD_SYSCALL);

    return data.error_code;
}

char *getcwd(char *buf, size_t size) {
    struct GetCwdData data =  {
        .buf = buf,
        .size = size
    };
    do_syscall(&data, GET_CWD_SYSCALL);

    return buf;
}

int chdir(const char *path) {
    struct ChdirData data = {
        .path = path,
    };
    do_syscall(&data, CHDIR_SYSCALL);
    return 0;
}

int execve(const char *filename, char *const argv[], char *const envp[]) {
    if (envp != NULL && *envp != NULL) {abort();}//currently can't handle this!
    struct ExecveData data = {
        .filename = filename,
        .argv = argv,
    };
    do_syscall(&data, EXECVE_SYSCALL);
    return -1;
}
int execvp(const char *file, char *const argv[]) {
    //TODO check $PATH for file, and handle ./thing
    //todo use environ to pass to execve
    return execve(file, argv, environ);
}

int isatty(int fd) {
    struct IsattyData data = {
        .fd = fd,
        .result = -2
    };
    do_syscall(&data, ISATTY_SYSCALL);

    return data.result;
}

int pipe(int pipefd[2]) {
    struct PipeData data = {};
    do_syscall(&data, PIPE_SYSCALL);

    pipefd[0] = data.fd_a;
    pipefd[1] = data.fd_b;
    
    return 0;
}

static void mssleep(uint64_t ms) {
    uint64_t start = get_uptime_ms();
    while (get_uptime_ms() - start <= ms){
        __asm("nop");
    }
}

int usleep(useconds_t usec) {
    //TODO error out if signal interrupts me
    mssleep(usec / 1000);
    return 0;
}
unsigned int sleep(unsigned int seconds) {
    //TODO error out if signal interrupts me
    mssleep((uint64_t)seconds * 1000ul);
    return 0;
}