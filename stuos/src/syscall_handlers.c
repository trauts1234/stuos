#include <uapi/errno.h>
#include <uapi/signal.h>
#include <uapi/stddef.h>
#include <uapi/stdbool.h>
#include "elf.h"
#include "fs.h"
#include "memory.h"
#include "debugging.h"
#include <uapi/syscalls.h>
#include <uapi/types.h>
#include "pipes_and_files.h"
#include "scheduling.h"
#include "tty.h"
#include "apic.h"
#include "kern_libc.h"
#include "uapi/page_size.h"

#define DEBUG_SYSCALLS false

static uint8_t syscall_stack[4096 * 4] __attribute__ ((__aligned__(16)));
// used in assembly
uint8_t *const syscall_stack_top = syscall_stack + sizeof(syscall_stack);

void syscall_halt(struct HaltSyscallData *data) {
    if(DEBUG_SYSCALLS) printf("%s: exit code %d\n", __func__, data->exit_code);
    set_current_as_zombie(data->exit_code);//TODO this should be a KILL-esque signal????
    run_next_task(NULL);
}

void syscall_getcwd(struct GetCwdData* data) {
    if(DEBUG_SYSCALLS) printf("%s: \n", __func__);
    const char* cwd = get_cwd();
    if(strlen(cwd) + 1 > data->size) {HCF}

    strcpy(data->buf, cwd);
}

void syscall_execve(const struct ExecveData* data) {
    if(DEBUG_SYSCALLS) {
        printf("%s: %s with args ", __func__, data->filename);
        char*const * curr = data->argv;
        while(curr && *curr) {
            printf("%s,", *curr);
            curr++;
        }
        printf("\n");
    }
    const struct VNode to_execute = vfs_get(get_cwd(), data->filename, 0);

    uint64_t argc=0;
    for(;data->argv[argc]; argc++);

    char** kernel_space_argv = malloc(sizeof(char*) * (argc + 1));//+1 to store NULL
    kernel_space_argv[argc] = NULL;

    for(uint64_t i=0;i<argc;i++) {
        uint64_t len = strlen(data->argv[i]);
        char* kernel_copy = malloc(len + 1);
        strcpy(kernel_copy, data->argv[i]);
        kernel_space_argv[i] = kernel_copy;
    }

    const struct LoadedProgram loaded = instantiate_ELF(to_execute, kernel_space_argv);

    for(uint64_t i=0;i<argc;i++) {
        free(kernel_space_argv[i]);
    }
    free(kernel_space_argv);
    //TODO:
    // - do not preserve signals (ensure to handle signal stacks too if a signal called the execve!)
    // - do not copy the heap
    // - and more...
    replace_current_process(loaded);
    run_next_task(NULL);
}

void syscall_stat(struct StatData* data) {
    if(DEBUG_SYSCALLS) printf("%s: %s\n", __func__, data->path);
    struct VNode file = vfs_get(get_cwd(), data->path, 0);
    data->result = file.stat_file(file.id);
}

// void syscall_sigprocmask(struct SigProcMaskData* data) {
    // if(DEBUG_SYSCALLS) printf("%s: \n", __func__);
    // sigset_t *curr = &get_process(0)->signal_mask;
    // data->oldset = *curr;
    // if(data->set) {
    //     switch (data->how) {
    //         case SIG_BLOCK:
    //             *curr |= *data->set;break;
    //         case SIG_UNBLOCK:
    //             *curr &= ~*data->set;break;
    //         case SIG_SETMASK:
    //             *curr = *data->set;break;
    //         default:
    //             HCF
    //     }
    // }
// }

// void syscall_setsignalhandler(struct SetSignalHandlerData *data) {
    // if(DEBUG_SYSCALLS) printf("%s: \n", __func__);
    // sighandler_t* sig = get_process(0)->signal_handlers + data->signal_number;
    // data->old_handler = *sig;
    // *sig = data->handler;
// }

// void syscall_kill(struct KillData *data, struct ProcessorState* state) {
    // if(DEBUG_SYSCALLS) printf("%s: \n", __func__);
    // if (data->pid > 0) {
    //     struct ProcessData *proc = get_process(data->pid);
    //     proc->signal_pending[data->sig] = true;
    // } else if (data->pid == -1) {
    //     HCF //send to nearly all processes? nah.
    // } else {
    //     // 0 -> -0 (current processes)
    //     pid_t process_group = data->pid == 0 ? get_process(0)->pgrp : -data->pid;
    //     pid_t elegible_processes[100];
    //     uint64_t num_elegible_processes = get_pids(elegible_processes, process_group);
    //     for(uint64_t i=0; i<num_elegible_processes; i++) {
    //         struct ProcessData *proc = get_process(elegible_processes[i]);
    //         proc->signal_pending[data->sig] = true;
    //     }
    // }

    // run_next_task(state);
// }

// void syscall_tcgetattr(struct TcGetAttrData *data) {
    // if(DEBUG_SYSCALLS) printf("%s: \n", __func__);
    // //DRY from isatty, TODO out of range file descriptors
    // struct FileOperations* fop = get_process(0)->file_descriptors[data->fd];

    // data->err = 0;
    // if(!fop->is_a_tty) {
    //     data->err = ENOTTY;
    //     return;
    // }

    // data->output = tty_settings;
// }

void syscall_yield(void*, struct ProcessorState *processor_state) {
    if(DEBUG_SYSCALLS) printf("%s: \n", __func__);
    run_next_task(processor_state);
}