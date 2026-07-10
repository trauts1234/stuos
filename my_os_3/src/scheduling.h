#ifndef SCHEDULING_H
#define SCHEDULING_H

#include "signal.h"
#include <uapi/signal.h>
#include <uapi/stdint.h>

#define MAX_FD_COUNT 10

struct ProcessorState {
    /* General purpose registers saved by assembly stub */
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rsi, rdi, rbp, rdx, rcx, rbx, rax;

    /* CPU-pushed state from actual interrupt*/
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

struct LoadedProgram {
    void* heap_start;
    uint64_t page_table_root;
    struct FileOperations* file_descriptors[MAX_FD_COUNT];
    struct ProcessorState initial_state;
};

struct WaitingData {
    enum {NOT_WAITING, WAITING_FOR_READ, WAITING_FOR_CHILD, I_AM_ZOMBIE} status;

    union {
        struct WaitingRead {
            int fd_number;
            uint8_t* output_buf;
            uint64_t num_bytes;
            uint64_t* output_num_bytes_ptr;
        } read;
        
        struct WaitingChild {
            // <-1 : wait for child whose pgid is abs(number)
            // -1 : wait for any child
            // 0 : wait for child whose pgid is my pid
            // >0 : wait for child whose pid is number
            int number;
            int* status;
            int options;
            
            int* output_pid;
        } child;

        struct WaitingZombie {
            uint8_t exit_code;
        } zombie;
    };
};

struct ProcessData {
    /// PAGE_SIZE aligned, represents where the ELF's heap starts - This is only to tell the ELF if they request this information via syscall
    void* heap_start;
    /// Indexed by file descriptor number
    struct FileOperations* file_descriptors[MAX_FD_COUNT];
    /// Process ID
    int pid;
    /// Process group ID
    int pgrp;
    /// Parent Process ID
    int ppid;
    /// certain operations are prevented after exec* commands
    // bool have_previously_exec;
    /// CR3 value
    uint64_t page_table_root;

    // userspace pointers to signal handler (process wide)
    // 0 is a valid default
    sighandler_t signal_handlers[NUM_SIGNALS];
    // Whether there is a pending signal (thread specific? not sure...)
    bool signal_pending[NUM_SIGNALS];
    //Bit set if the signal should be ignored (1 is masked, 0 is unmasked) (thread specific)
    // 0 is a valid default
    sigset_t signal_mask;

    /// Where relative paths originate from
    /// Must be heap allocated
    const char *cwd;

    /// Assuming the thread is paused, this is the state
    struct ProcessorState paused_state;

    struct WaitingData waiting_data;

    struct ProcessData* next_process_to_run;
};

/// Puts a new process in the queue
/// (file_descriptors is copied)
/// The current running process is taken as the parent
/// Returns the allocated process ID
int add_new_process(struct LoadedProgram program);

/// Applies the functionality of execve
/// The LoadedProgram's file descriptors are ignored
/// Correct things are copied from the current process
/// WARNING: remember that the address space is changed here too
void replace_current_process(struct LoadedProgram program);

struct ProcessData *get_process(int pid);
//finds the pids that are under the specified pgrp
uint64_t get_pids(int output_pids[100], int pgrp);
void register_as_waiting(struct WaitingData data);

//TODO remove?
const char* get_current_cwd();
void set_current_cwd(const char* new_ptr);

// pass NULL if you don't want to save the processor state, i.e after killing the current process or when no process has been started yet
void run_next_task(const struct ProcessorState* const interrupted_processor_state) __attribute__((noreturn));

#endif