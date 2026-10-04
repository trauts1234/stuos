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

void syscall_get_uptime_ms(struct GetUptimeMsData* data) {
    if(DEBUG_SYSCALLS) printf("%s: \n", __func__);
    data->ms = get_uptime_ms();
}

void syscall_request_page(struct RequestPageData* data) {
    if(DEBUG_SYSCALLS) printf("%s: requested %p\n", __func__, data->page_virt_addr);
    if ((uint64_t)data->page_virt_addr >> 63) {
        //higher half
        HCF
    }
    assert(((uint64_t)data->page_virt_addr & PAGE_MASK) == 0);

    struct LimitData lim = get_rlimit(RLIMIT_DATA);
    if(lim.current_value + PAGE_SIZE > lim.limit.rlim_cur) {
        data->err = ENOMEM;
        return;
    }
    
    allocate_ram_page(data->page_virt_addr, false);
}

void syscall_get_heap_start(struct GetHeapStartData* data) {
    if(DEBUG_SYSCALLS) printf("%s: \n", __func__);
    data->output = get_current_heap_start();
}

void syscall_write_fd(struct WriteFDData* data) {
    if(DEBUG_SYSCALLS) printf("%s: write %llu bytes to fd %d\n", __func__, data->num_bytes, data->file_descriptor_number);
    const struct FileOperations* file_operations = get_file_descriptor(data->file_descriptor_number);
    if(file_operations == NULL) {HCF}
    data->num_bytes_actually_written = file_operations->write(file_operations->special_data, data->buffer, data->num_bytes);
}

void syscall_open_file(struct OpenFileData* data);

void syscall_read_fd(struct ReadFDData* data, struct ProcessorState* processor_state);

void syscall_lseek_fd(struct LseekFDData* data);

void syscall_close_fd(struct CloseFDData* data);

void syscall_fork(struct ForkData* data, struct ProcessorState* parent_state);

void syscall_get_pgrp(struct GetPgrpData* data);

void syscall_get_pid(struct GetPidData* data);

void syscall_dupfd(struct DupFdData* data);

void syscall_getcwd(struct GetCwdData* data) {
    if(DEBUG_SYSCALLS) printf("%s: \n", __func__);
    const char* cwd = get_cwd();
    if(strlen(cwd) + 1 > data->size) {HCF}

    strcpy(data->buf, cwd);
}

void syscall_chdir(struct ChdirData* data);

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

void syscall_wait(struct WaitData* data, struct ProcessorState* state);

void syscall_isatty(struct IsattyData* data);

void syscall_pipe(struct PipeData* data);

void syscall_stat(struct StatData* data) {
    if(DEBUG_SYSCALLS) printf("%s: %s\n", __func__, data->path);
    struct VNode file = vfs_get(get_cwd(), data->path, 0);
    data->result = file.stat_file(file.id);
}

void syscall_sigprocmask(struct SigProcMaskData* data) {
    HCF
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
}

void syscall_setsignalhandler(struct SetSignalHandlerData *data) {
    HCF
    // if(DEBUG_SYSCALLS) printf("%s: \n", __func__);
    // sighandler_t* sig = get_process(0)->signal_handlers + data->signal_number;
    // data->old_handler = *sig;
    // *sig = data->handler;
}

void syscall_kill(struct KillData *data, struct ProcessorState* state) {
    HCF
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
}

void syscall_tcgetattr(struct TcGetAttrData *data) {
    HCF
    // if(DEBUG_SYSCALLS) printf("%s: \n", __func__);
    // //DRY from isatty, TODO out of range file descriptors
    // struct FileOperations* fop = get_process(0)->file_descriptors[data->fd];

    // data->err = 0;
    // if(!fop->is_a_tty) {
    //     data->err = ENOTTY;
    //     return;
    // }

    // data->output = tty_settings;
}

void syscall_setrlimit(struct SetRLimitData *data) {
    if(DEBUG_SYSCALLS) printf("%s: limit %d = soft: %llu, hard: %llu\n", __func__, data->resource, data->limit.rlim_cur, data->limit.rlim_max);

    set_rlimit(data->resource, data->limit);
}
void syscall_getrlimit(struct GetRLimitData *data) {
    if(DEBUG_SYSCALLS) printf("%s: limit %d\n", __func__, data->resource);

    data->limit = get_rlimit(data->resource).limit;
}

void syscall_yield(void*, struct ProcessorState *processor_state) {
    if(DEBUG_SYSCALLS) printf("%s: \n", __func__);
    run_next_task(processor_state);
}

void *syscall_table[] = {
    syscall_halt,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    syscall_get_uptime_ms,
    NULL,
    NULL,
    NULL,
    syscall_request_page,
    syscall_get_heap_start,
    syscall_write_fd,
    syscall_open_file,
    syscall_read_fd,
    syscall_lseek_fd,
    syscall_close_fd,
    syscall_fork,
    syscall_get_pgrp,
    syscall_get_pid,
    syscall_dupfd,
    syscall_getcwd,
    syscall_chdir,
    syscall_execve,
    syscall_wait,
    syscall_isatty,
    syscall_pipe,
    syscall_stat,
    syscall_sigprocmask,
    [TCGETATTR_SYSCALL] = syscall_tcgetattr,
    [SETRLIMIT_SYSCALL] = syscall_setrlimit,
    [GETRLIMIT_SYSCALL] = syscall_getrlimit,
    [YIELD_SYSCALL] = syscall_yield,
};