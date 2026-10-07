#ifndef SCHEDULING_H
#define SCHEDULING_H

#include "pipes_and_files.h"
#include "signal.h"
#include <uapi/resource.h>
#include <uapi/signal.h>
#include <uapi/stdint.h>
#include <uapi/limits.h>
#include <uapi/types.h>

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
    struct ProcessorState initial_state;
};

struct LimitData {
    rlim_t current_value;
    struct rlimit limit;
};

/// Puts a new process in the queue
/// (file_descriptors is copied)
/// The current running process is taken as the parent
/// Returns the allocated process ID
pid_t add_new_process(struct LoadedProgram program);

// pass NULL if you don't want to save the processor state, i.e after killing the current process or when no process has been started yet
void run_next_task(const struct ProcessorState* const interrupted_processor_state) __attribute__((noreturn));

void set_current_as_zombie(uint8_t exit_code);

const struct FileOperations *get_file_descriptor(int fd_number);
void set_file_descriptor(int fd_number, struct FileOperations operations);
const char *get_cwd();
void replace_current_process(struct LoadedProgram program);


#endif