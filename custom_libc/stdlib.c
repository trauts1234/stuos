#include "stdlib.h"
#include "stdint.h"
#include "nonstandard.h"

#include "uapi/syscalls.h"
#include "rust_bindings.h"

static struct MemoryAllocator memory_allocator;
static bool memory_allocator_is_init = false;

static uint64_t allocate_more_heap(void* heap_end, uint64_t bytes){
    //hope these constants are the same in the kernel...
    static const uint64_t PAGE_SIZE = 4096;
    static const uint64_t PAGE_MASK = PAGE_SIZE-1;

    if((uint64_t)heap_end & PAGE_MASK) {abort();}//must be page aligned
    uint64_t rounded_up_pages = (bytes + PAGE_SIZE - 1) / PAGE_SIZE;

    for(uint64_t i = 0; i<rounded_up_pages; i++) {
        struct RequsetPageData alloc_data = {.page_virt_addr = heap_end + (PAGE_SIZE * i)};
        do_syscall(&alloc_data, REQUEST_PAGE_SYSCALL);
    }

    return PAGE_SIZE * rounded_up_pages;
}

void* malloc(unsigned long bytes){
    if(!memory_allocator_is_init) {
        memory_allocator_is_init = true;

        struct GetHeapStartData heap_start;
        do_syscall(&heap_start, GET_HEAP_START_SYSCALL);
        init_memory_allocator(allocate_more_heap, heap_start.output, &memory_allocator);
    }
    
    return allocate(&memory_allocator, bytes);
}
void free(void* ptr) {
    if(!memory_allocator_is_init) {abort();}
    deallocate(&memory_allocator, ptr);
}

void exit(int status) {
    //TODO exit code
    do_syscall(0, HALT_SYSCALL);
}
void abort() {
    do_syscall(0, HALT_SYSCALL);
}