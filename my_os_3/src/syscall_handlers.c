#include <stddef.h>
#include <stdbool.h>
#include "elf.h"
#include "fs.h"
#include "interrupts.h"
#include "memory.h"
#include "display.h"
#include "debugging.h"
#include "interrupts.h"
#include "uapi/syscalls.h"
#include "pipes_and_files.h"
#include "scheduling.h"
#include "kern_libc.h"

#define DEBUG_SYSCALLS false

static uint8_t syscall_stack[4096 * 4] __attribute__ ((__aligned__(16)));
// used in assembly
uint8_t *const syscall_stack_top = syscall_stack + sizeof(syscall_stack);

void syscall_crash(void*, struct ProcessorState* processor_state) {
    if(DEBUG_SYSCALLS) kprintf("%s: \n", __func__);
    register_as_waiting((struct WaitingData) {
        .status = I_AM_ZOMBIE,
        .zombie = {
            
        }
    });
    run_next_task(NULL);
}

void syscall_write_pixel(struct WritePixelData* data) {
    if(DEBUG_SYSCALLS) kprintf("%s: x=%d,y=%d = rgb %d %d %d\n", __func__, data->x, data->y, data->r, data->g, data->b);
    struct Colour col = {.r=data->r, .g=data->g, .b=data->b, .a=0};
    display_write_pixel(data->x, data->y, col);
}

void syscall_readchar_nonblocking(struct GetCharNonblockingData* data) {
    if(DEBUG_SYSCALLS) kprintf("%s: \n", __func__);
    HCF
    // data->output = read_char_nonblocking(&data->pressed);
}

void syscall_get_uptime_ms(struct GetUptimeMsData* data) {
    if(DEBUG_SYSCALLS) kprintf("%s: \n", __func__);
    data->ms = get_uptime_ms();
}

void syscall_request_page(struct RequsetPageData* data) {
    if(DEBUG_SYSCALLS) kprintf("%s: requested %p\n", __func__, data->page_virt_addr);
    if ((uint64_t)data->page_virt_addr >> 63) {
        //higher half
        return;
    }
    allocate_ram_page(data->page_virt_addr);
}

void syscall_get_heap_start(struct GetHeapStartData* data) {
    if(DEBUG_SYSCALLS) kprintf("%s: \n", __func__);
    data->output = get_current_heap_start();
}

void syscall_write_fd(struct WriteFDData* data, struct ProcessorState* processor_state) {
    if(DEBUG_SYSCALLS) kprintf("%s: write %lu bytes to fd %d\n", __func__, data->num_bytes, data->file_descriptor_number);
    struct FileOperations* file_operations = get_file_descriptors()[data->file_descriptor_number];
    if(file_operations == NULL) {HCF}
    data->num_bytes_actually_written = file_operations->write(file_operations->special_data, data->buffer, data->num_bytes);
}

static int find_free_fd() {
    //find a free file descriptor
    int fd_num = 0;
    while(get_file_descriptors()[fd_num] != 0) {
        fd_num++;
        if(fd_num == MAX_FD_COUNT) {HCF}//out of file descriptors
    }
    return fd_num;
}

void syscall_open_file(struct OpenFileData* data) {
    if(DEBUG_SYSCALLS) kprintf("%s: path: %s\n", __func__, data->path);
    struct FileOperations** fd_list = get_file_descriptors();
    struct FileOperations* file = fop_generate_file(get_current_cwd(), data->path, data->open_flags);
    int fd_num = find_free_fd();
    fd_list[fd_num] = file;
    data->output_file_descriptor_number = fd_num;
}

void syscall_read_fd(struct ReadFDData* data, struct ProcessorState* processor_state) {
    if(DEBUG_SYSCALLS) kprintf("%s: read up to %lu bytes from fd %d\n", __func__, data->num_bytes, data->file_descriptor_number);
    register_as_waiting((struct WaitingData) {
        .status = WAITING_FOR_READ,
        .read = {
            .fd_number = data->file_descriptor_number,
            .output_buf = data->buffer,
            .num_bytes = data->num_bytes,
            .output_num_bytes_ptr = &data->num_bytes_actually_read,
        }
    });
    
    run_next_task(processor_state);
}

void syscall_lseek_fd(struct LseekFDData* data) {
    if(DEBUG_SYSCALLS) kprintf("%s: offset %lu in fd %d. whence: %d\n", __func__, data->offset, data->file_descriptor_number, data->whence);
    struct FileOperations* file_operations = get_file_descriptors()[data->file_descriptor_number];
    if(file_operations == NULL) {HCF}

    data->actual_offset = file_operations->offset(file_operations->special_data, data->offset, data->whence);
}

void syscall_close_fd(struct CloseFDData* data) {
    if(DEBUG_SYSCALLS) kprintf("%s: fd %d\n", __func__, data->file_descriptor_number);
    struct FileOperations** file_operations = get_file_descriptors() + data->file_descriptor_number;

    (*file_operations)->close((*file_operations)->special_data);
    *file_operations = NULL;

    data->error_code = 0;
}

void syscall_fork(struct ForkData* data, struct ProcessorState* parent_state) {
    if(DEBUG_SYSCALLS) kprintf("%s: \n", __func__);
    uint64_t parent_page_table = get_pml4_phys();
    uint64_t child_page_table = clone_virtual_addressing(parent_page_table);

    struct LoadedProgram child = {
        .heap_start = get_current_heap_start(),
        .page_table_root = child_page_table,
        //file_descriptors is done after
        .initial_state = *parent_state
    };
    
    //add 1 to ref count as the child will have cloned the file descriptors
    for(int i=0; i<MAX_FD_COUNT; i++) {
        struct FileOperations* fd = get_file_descriptors()[i];
        if (fd != NULL) fd->reference_count++;
        child.file_descriptors[i] = fd;
    }

    int child_pid = add_new_process(child);

    //now here's a mad bit - data is a userspace pointer, so changing CR3 will cause the same pointer to point to a different page
    set_pml4_phys(child_page_table);
    data->pid = 0;//give child 0
    set_pml4_phys(parent_page_table);
    data->pid = child_pid;// pass the child's PID to the parent
}

void syscall_get_pgrp(struct GetPgrpData* data) {
    if(DEBUG_SYSCALLS) kprintf("%s: \n", __func__);
    data->result = get_pgrp(0);
}

void syscall_get_pid(struct GetPidData* data) {
    if(DEBUG_SYSCALLS) kprintf("%s: \n", __func__);
    data->result = get_current_pid();
}

void syscall_dup2(struct Dup2Data* data) {
    if(DEBUG_SYSCALLS) kprintf("%s: fd %d => fd %d\n", __func__, data->oldfd, data->newfd);
    struct FileOperations **fd = get_file_descriptors();

    if(data->newfd >= MAX_FD_COUNT || data->newfd < 0) {HCF}
    //if it's stupid and it works...
    #define old fd[data->oldfd]
    #define new fd[data->newfd]
    if(old == NULL) {HCF}

    if(new == old) {
        return;//do nothing as the file descriptor is already cloned
    }
    //close new if already open
    if(new) {
        new->close(new->special_data);
    }
    new = old;
    old->reference_count++;
    #undef old
    #undef new
}

void syscall_getcwd(struct GetCwdData* data) {
    if(DEBUG_SYSCALLS) kprintf("%s: \n", __func__);
    const char* cwd = get_current_cwd();
    if(strlen(cwd) + 1 > data->size) {HCF}

    strcpy(data->buf, cwd);
}

void syscall_chdir(struct ChdirData* data) {
    if(DEBUG_SYSCALLS) kprintf("%s: %s\n", __func__, data->path);
    set_current_cwd(data->path);
}

void syscall_execve(const struct ExecveData* data) {
    if(DEBUG_SYSCALLS) kprintf("%s: %s\n", __func__, data->filename);
    const struct VNode to_execute = vfs_get(get_current_cwd(), data->filename, 0);

    uint64_t argc=0;
    for(;data->argv[argc]; argc++);

    char** kernel_space_argv = kmalloc(sizeof(char*) * (argc + 1));//+1 to store NULL
    kernel_space_argv[argc] = NULL;

    for(uint64_t i=0;i<argc;i++) {
        uint64_t len = strlen(data->argv[i]);
        char* kernel_copy = kmalloc(len + 1);
        strcpy(kernel_copy, data->argv[i]);
        kernel_space_argv[i] = kernel_copy;
    }

    const struct LoadedProgram loaded = instantiate_ELF(to_execute, kernel_space_argv);

    for(uint64_t i=0;i<argc;i++) {
        kfree(kernel_space_argv[i]);
    }
    kfree(kernel_space_argv);
    //TODO:
    // - do not preserve signals (ensure to handle signal stacks too if a signal called the execve!)
    // - do not copy the heap
    // - and more...
    replace_current_process(loaded);
    run_next_task(NULL);
}

void syscall_wait(struct WaitData* data, struct ProcessorState* state) {
    if(DEBUG_SYSCALLS) kprintf("%s: for pid %d\n", __func__);
    register_as_waiting((struct WaitingData) {
        .status = WAITING_FOR_CHILD,
        .child = {
            .number = data->pid,
            .status = data->status,
            .options = data->options,
            .output_pid = &data->output_pid,
        }
    });

    run_next_task(state);
}

void syscall_isatty(struct IsattyData* data) {
    if(DEBUG_SYSCALLS) kprintf("%s: \n", __func__);
    struct FileOperations* fop = get_file_descriptors()[data->fd];
    data->result = fop->is_a_tty;
}

void syscall_pipe(struct PipeData* data) {
    if(DEBUG_SYSCALLS) kprintf("%s: \n", __func__);
    struct FileOperations *fds[2] = {NULL, NULL};
    fop_generate_pipe(fds);

    int fd_a = find_free_fd();
    get_file_descriptors()[fd_a] = fds[0];
    int fd_b = find_free_fd();
    get_file_descriptors()[fd_b] = fds[1];

    data->fd_a = fd_a;
    data->fd_b = fd_b;
}

void *syscall_table[] = {
    syscall_crash,
    NULL,
    syscall_readchar_nonblocking,
    NULL,
    NULL,
    syscall_write_pixel,
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
    syscall_dup2,
    syscall_getcwd,
    syscall_chdir,
    syscall_execve,
    syscall_wait,
    syscall_isatty,
    syscall_pipe,
};