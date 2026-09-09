#ifndef UAPI_SYSCALLS_H
#define UAPI_SYSCALLS_H

#include "resource.h"
#include "signal.h"
#include "stat.h"
#include "stdint.h"
#include "fcntl.h"
#include "termios.h"
#include "types.h"

extern void do_syscall(void* data, uint64_t syscall_number);

#define HALT_SYSCALL 0
struct HaltSyscallData {
    uint8_t exit_code;
};

#define CLEARSCREEN_SYSCALL 6

#define GET_UPTIME_MS_SYSCALL 7
struct GetUptimeMsData {
    uint64_t ms;
};

#define REQUEST_PAGE_SYSCALL 11
struct RequestPageData {
    void* page_virt_addr;
    //0, ENOMEM
    int err;
};

#define GET_HEAP_START_SYSCALL 12
struct GetHeapStartData {
    void* output;
};

#define WRITE_FD_SYSCALL 13
struct WriteFDData {
    int file_descriptor_number;
    const uint8_t* buffer;
    uint64_t num_bytes;

    uint64_t num_bytes_actually_written;
};

#define OPEN_FILE_SYSCALL 14
struct OpenFileData {
    const char* path;
    int output_file_descriptor_number;
    int open_flags;
};

#define READ_FD_SYSCALL 15
struct ReadFDData {
    int file_descriptor_number;
    uint8_t* buffer;
    //Try to read at most this many bytes
    uint64_t num_bytes;

    uint64_t num_bytes_actually_read;
};

#define LSEEK_FD_SYSCALL 16
struct LseekFDData {
    int file_descriptor_number;
    int64_t offset;
    int whence;
    //actual offset from the start of the file
    int64_t actual_offset;
};

#define CLOSE_FD_SYSCALL 17
struct CloseFDData {
    int file_descriptor_number;
};

#define FORK_SYSCALL 18
struct ForkData {
    // - parent: process id of child, or -1 on failure
    // - child: 0
    int pid;
};

#define GET_PGRP_SYSCALL 19
struct GetPgrpData {
    int result;
};

#define GET_PID_SYSCALL 20
struct GetPidData {
    int result;
};

#define DUPFD_SYSCALL 21
struct DupFdData {
    int fildes;
    //find a fd >= to this
    int min_new_fd;
    int result_fd;
};

#define GET_CWD_SYSCALL 22
struct GetCwdData {
    char* buf;
    uint64_t size;
};

#define CHDIR_SYSCALL 23
struct ChdirData {
    const char* path;
};

#define EXECVE_SYSCALL 24
struct ExecveData {
    const char* filename;
    char *const *argv;
};

// bitfield for options
enum {WNOHANG=1, WUNTRACED=2, WCONTINUED=4};

#define WAIT_SYSCALL 25
struct WaitData {
    //as seen in waitpid()
    int pid;
    int* status;

    int options;

    //return value from waitpid()
    //my implementation puts the exit status here
    int output_pid;
};

#define ISATTY_SYSCALL 26
struct IsattyData {
    int fd;
    int result;
};

#define PIPE_SYSCALL 27
struct PipeData {
    int fd_a;
    int fd_b;
};

#define STAT_SYSCALL 28
struct StatData {
    struct stat result;
    const char *path;
};

#define SIGPROCMASK_SYSCALL 29
struct SigProcMaskData {
    int how;
    const sigset_t* set;
    sigset_t oldset;
};

#define SETSIGNALHANDLER_SYSCALL 30
struct SetSignalHandlerData {
    int signal_number;
    // 0 to request the default handler, otherwise a pointer to a handler
    sighandler_t handler;
    sighandler_t old_handler;
};

#define KILL_SYSCALL 31
struct KillData {
    pid_t pid;
    int sig;
};

//TODO test
#define TCGETATTR_SYSCALL 32
struct TcGetAttrData {
    int fd;
    //only written to if errno == 0
    struct termios output;
    // 0, EBADF, ENOTTY
    int err;
};

#define SETRLIMIT_SYSCALL 33
struct SetRLimitData {
    int resource;
    struct rlimit limit;
    //0, EINVAL
    int err;
};

#define GETRLIMIT_SYSCALL 34
struct GetRLimitData {
    int resource;
    struct rlimit limit;
    //0, EINVAL
    int err;
};

#endif