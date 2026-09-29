#ifndef PROCESSES_H
#define PROCESSES_H

#include <uapi/stdint.h>
#include <uapi/limits.h>
#include <uapi/resource.h>

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
    const void* heap_start;
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
int add_new_process(struct LoadedProgram program);

#endif