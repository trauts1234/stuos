#ifndef SCHEDULING_H
#define SCHEDULING_H

#include "uapi/stdint.h"

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
            //TODO return code etc.
        } zombie;
    };
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

/// pid can be set to 0 to read current process
int get_current_pid();
/// pid can be set to 0 to read current process
int get_pgrp(int pid);
void* get_current_heap_start();
//This is read-write if you are sensible about it
struct FileOperations** get_file_descriptors();
void register_as_waiting(struct WaitingData data);

const char* get_current_cwd();
void set_current_cwd(const char* new_ptr);

// pass NULL if you don't want to save the processor state, i.e after killing the current process or when no process has been started yet
void run_next_task(const struct ProcessorState* const interrupted_processor_state) __attribute__((noreturn));

#endif