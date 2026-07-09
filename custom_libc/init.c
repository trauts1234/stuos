#include "rust_bindings.h"
#include "uapi/syscalls.h"
#include <stdlib.h>

struct MemoryAllocator memory_allocator;

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

void stuos_libc_init() {
    struct GetHeapStartData heap_start;
    do_syscall(&heap_start, GET_HEAP_START_SYSCALL);
    init_memory_allocator(allocate_more_heap, heap_start.output, &memory_allocator);
}