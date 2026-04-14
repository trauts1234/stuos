#ifndef UAPI_SYSCALLS_H
#define UAPI_SYSCALLS_H
//pointers to these passed in RDI to syscalls

#include "stdint.h"
#include "fcntl.h"

extern void do_syscall(void* data, uint64_t syscall_number);

static const uint64_t HALT_SYSCALL = 0;

static const uint64_t GETCHARNONBLOCKING_SYSCALL = 2;
struct GetCharNonblockingData {
    int pressed;
    char output;
};

static const uint64_t WRITEPIXEL_SYSCALL = 5;
struct WritePixelData {
    uint64_t x;
    uint64_t y;
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

static const uint64_t CLEARSCREEN_SYSCALL=6;

static const uint64_t GET_UPTIME_MS_SYSCALL=7;
struct GetUptimeMsData {
    uint64_t ms;
};

static const uint64_t REQUEST_PAGE_SYSCALL = 11;
struct RequsetPageData {
    void* page_virt_addr;
};

static const uint64_t GET_HEAP_START_SYSCALL = 12;
struct GetHeapStartData {
    void* output;
};

static const uint64_t WRITE_FD_SYSCALL = 13;
struct WriteFDData {
    int file_descriptor_number;
    const uint8_t* buffer;
    uint64_t num_bytes;

    uint64_t num_bytes_actually_written;
};

static const uint64_t OPEN_FILE_SYSCALL = 14;
struct OpenFileData {
    const char* path;
    int output_file_descriptor_number;
    int open_flags;
};

static const uint64_t READ_FD_SYSCALL = 15;
struct ReadFDData {
    int file_descriptor_number;
    uint8_t* buffer;
    //Try to read at most this many bytes
    uint64_t num_bytes;

    uint64_t num_bytes_actually_read;
};

static const uint64_t LSEEK_FD_SYSCALL = 16;
struct LseekFDData {
    int file_descriptor_number;
    int64_t offset;
    int whence;
    //actual offset from the start of the file
    int64_t actual_offset;
};

static const uint64_t CLOSE_FD_SYSCALL = 17;
struct CloseFDData {
    int file_descriptor_number;
    //TODO I am usually just returning this, instead of using errno
    int error_code;
};

static const uint64_t FORK_SYSCALL = 18;
struct ForkData {
    // - parent: process id of child, or -1 on failure
    // - child: 0
    int pid;
};

static const uint64_t GET_PGRP_SYSCALL = 19;
struct GetPgrpData {
    int result;
};

static const uint64_t GET_PID_SYSCALL = 20;
struct GetPidData {
    int result;
};

static const uint64_t DUP2_SYSCALL = 21;
struct Dup2Data {
    int oldfd;
    int newfd;
};

static const uint64_t GET_CWD_SYSCALL = 22;
struct GetCwdData {
    char* buf;
    uint64_t size;
};

static const uint64_t CHDIR_SYSCALL = 23;
struct ChdirData {
    const char* path;
};

static const uint64_t EXECVE_SYSCALL = 24;
struct ExecveData {
    const char* filename;
    char *const *argv;
};

// bitfield for options
enum {WNOHANG=1, WUNTRACED=2, WCONTINUED=4};

static const uint64_t WAIT_SYSCALL = 25;
struct WaitData {
    //as seen in waitpid()
    int pid;
    int* status;

    int options;

    //return value from waitpid()
    //my implementation puts the exit status here
    int output_pid;
};

static const uint64_t ISATTY_SYSCALL = 26;
struct IsattyData {
    int fd;
    int result;
};

static const uint64_t PIPE_SYSCALL = 27;
struct PipeData {
    int fd_a;
    int fd_b;
};

#endif