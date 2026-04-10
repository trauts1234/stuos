#include "scheduling.h"
#include "debugging.h"
#include "kern_libc.h"
#include "pipes_and_files.h"
#include "memory.h"

#define MAX_THREADS_COUNT 1

/// @brief Starts a userland program
__attribute__((noreturn))
extern void start_userland(struct ProcessorState* processor_state);

int allocate_pid() {
    static int next = 1;
    if(next < 0) HCF
    return next++;
}

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

    /// Where relative paths originate from
    /// Must be heap allocated
    const char *cwd;

    //for multithreading, add array here: struct ThreadData workers[MAX_THREADS_COUNT];
    /// Assuming the thread is paused, this is the state
    struct ProcessorState paused_state;

    struct WaitingData waiting_data;

    struct ProcessData* next_process_to_run;
};

//current item in the looping process linked list of doom
struct ProcessData* current_process_in_ll = NULL;

// previous can be null
// pid can be 0, for current process
// returns NULL and doesn't change previous if pid not found
static struct ProcessData* get_process(int pid, struct ProcessData** previous) {
    if(pid == 0) pid = current_process_in_ll->pid;

    struct ProcessData* prev_ptr = current_process_in_ll;
    struct ProcessData* ptr = prev_ptr->next_process_to_run;
    while(ptr->pid != pid) {
        prev_ptr = ptr;
        ptr = ptr->next_process_to_run;
        if(prev_ptr == current_process_in_ll) {
            return NULL;
        }
    }

    if(previous) *previous = prev_ptr;
    return ptr;
}

int add_new_process(struct LoadedProgram program) {

    char* cwd = kmalloc(2);
    strcpy(cwd, "/");

    int pgrp = 1;
    int ppid = 0;
    if(current_process_in_ll) {
        //inherit
        pgrp = current_process_in_ll->pgrp;
        ppid = current_process_in_ll->pid;
    }

    struct ProcessData* new = kmalloc(sizeof(struct ProcessData));
    *new = (struct ProcessData) {
        .heap_start = program.heap_start,
        //file_descriptors is set later
        .pid = allocate_pid(),
        .pgrp = pgrp,
        .ppid = ppid,
        .page_table_root = program.page_table_root,
        .cwd = cwd,
        .paused_state = program.initial_state,
        .waiting_data = {
            .status = NOT_WAITING,
        }
        //next_process_to_run is set later
    };

    memcpy(&new->file_descriptors, program.file_descriptors, sizeof(struct FileOperations*) * MAX_FD_COUNT);

    if(current_process_in_ll == NULL) {
        new->next_process_to_run = new;// just one process, so point at myself
        current_process_in_ll = new;
    } else {
        //insert new just after the current process
        new->next_process_to_run = current_process_in_ll->next_process_to_run;
        current_process_in_ll->next_process_to_run = new;
    }
    return new->pid;
}

void replace_current_process(struct LoadedProgram program) {
    if(!current_process_in_ll) HCF//there must be a parent

    struct ProcessData new = (struct ProcessData) {
        .heap_start = program.heap_start,
        //file_descriptors is set later
        .pid = current_process_in_ll->pid,
        .pgrp = current_process_in_ll->pgrp,
        .page_table_root = program.page_table_root,
        .cwd = current_process_in_ll->cwd,
        .paused_state = program.initial_state,
        .waiting_data = {
            .status = NOT_WAITING, //is this right?
        },
        .next_process_to_run = current_process_in_ll->next_process_to_run,
    };
    memcpy(&new.file_descriptors, current_process_in_ll->file_descriptors, sizeof(struct FileOperations*) * MAX_FD_COUNT);

    //replace the current process
    *current_process_in_ll = new;

    //replace address space
    remove_virtual_addressing();
    set_pml4_phys(new.page_table_root);
}

int get_current_pid() {
    struct ProcessData* data = get_process(0, NULL);
    return data->pid;
}
int get_pgrp(int pid) {
    struct ProcessData* data = get_process(pid, NULL);
    return data->pgrp;
}
void* get_current_heap_start() {
    return current_process_in_ll->heap_start;
}
struct FileOperations** get_file_descriptors() {
    return current_process_in_ll->file_descriptors;
}
const char* get_current_cwd() {
    return current_process_in_ll->cwd;
}
//copies new_ptr's data
void set_current_cwd(const char* new_ptr) {
    uint64_t len = strlen(new_ptr);
    char* dest = kmalloc(len+1);
    memcpy(dest, new_ptr, len+1);

    current_process_in_ll->cwd = dest;
}

void register_as_waiting(struct WaitingData data) {
    if(current_process_in_ll->waiting_data.status != NOT_WAITING) {HCF}
    current_process_in_ll->waiting_data = data;
}

static bool is_suitable_zombie(struct WaitingChild waiting_data, struct ProcessData *process) {
    if (process->ppid != current_process_in_ll->pid || process->waiting_data.status != I_AM_ZOMBIE) {
        return false;
    }
    if(waiting_data.number < -1) {
        return process->pgrp == -waiting_data.number;
    } else if (waiting_data.number == -1) {
        return true;
    } else if (waiting_data.number == 0) {
        return process->pgrp == current_process_in_ll->pgrp;
    } else {
        return process->pid == waiting_data.number;
    }
}

void reap(int pid) {
    //find the process
    struct ProcessData *prev_ptr = NULL;
    struct ProcessData *to_reap = get_process(pid, &prev_ptr);

    if(prev_ptr == to_reap) {
        //last process has died
        debug_print("Ran out of processes to run, as they have all died\n");
        HCF
    }

    if(to_reap->waiting_data.status != I_AM_ZOMBIE) HCF//can only reap zombie processes

    //reparent children
    for(struct ProcessData* curr = to_reap->next_process_to_run; curr != to_reap; curr = curr->next_process_to_run) {
        if(curr->ppid == to_reap->pid) {
            curr->ppid = 1;//init process now owns this process
        }
    }

    //destroy cwd
    kfree((void*)to_reap->cwd);

    //destroy file descriptors:
    // TODO should this happen when they go zombie, not now
    struct FileOperations** file_descriptors = to_reap->file_descriptors;
    for(uint64_t fd_idx=0; fd_idx < MAX_FD_COUNT; fd_idx++) {
        struct FileOperations* fd = file_descriptors[fd_idx];
        if(fd == NULL) continue;
        fd->close(fd->special_data);
    }

    prev_ptr->next_process_to_run = to_reap->next_process_to_run;
    kfree(to_reap);

    uint64_t original_cr3 = get_pml4_phys();
    if(original_cr3 == to_reap->page_table_root) HCF//how is a zombie process running right now?
    //delete the page table root
    set_pml4_phys(to_reap->page_table_root);
    remove_virtual_addressing();

    set_pml4_phys(original_cr3);
}

/// Called by an assembly interrupt handler
/// If you have just killed the current process, pass NULL here
void run_next_task(const struct ProcessorState* const interrupted_processor_state) {
    if(current_process_in_ll == NULL) HCF //must be running something

    //save the processor state
    if(interrupted_processor_state) current_process_in_ll->paused_state = *interrupted_processor_state;

    while(1) {
        //go to the next process
        current_process_in_ll = current_process_in_ll->next_process_to_run;
        set_pml4_phys(current_process_in_ll->page_table_root);

        switch (current_process_in_ll->waiting_data.status) {

        case NOT_WAITING:
            // program needs running normally
            start_userland(&current_process_in_ll->paused_state);

        case WAITING_FOR_READ:
            // poll for read
            struct WaitingRead read_data = current_process_in_ll->waiting_data.read;
            struct FileOperations* fop = current_process_in_ll->file_descriptors[read_data.fd_number];
            if(fop == NULL) {HCF}
            struct FopReadResult result = fop->read_nonblocking(fop->special_data, read_data.output_buf, read_data.num_bytes);
            if(result.read_something) {
                //something happened!
                *current_process_in_ll->waiting_data.read.output_num_bytes_ptr = result.bytes_read;
                current_process_in_ll->waiting_data.status = NOT_WAITING;
            }
            break;

        case WAITING_FOR_CHILD:
            struct WaitingChild request = current_process_in_ll->waiting_data.child;
            if(request.options != 0) HCF//TODO implement the flags that can be passed

            for (struct ProcessData* curr = current_process_in_ll->next_process_to_run; curr != current_process_in_ll; curr = curr->next_process_to_run) {
                if(is_suitable_zombie(request, curr)) {
                    //zombie child found!
                    if (request.status) {
                        *request.status = 0;//TODO put status code here
                    }
                    *request.output_pid = curr->pid;
                    current_process_in_ll->waiting_data.status = NOT_WAITING;

                    //reap the child
                    reap(curr->pid);

                    break;
                }
            }
            break;
        case I_AM_ZOMBIE:
            break;//nothing to do here

        }
    }
}