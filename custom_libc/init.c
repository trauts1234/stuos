#include "required.h"
#include "uapi/errno.h"
#include "uapi/syscalls.h"
#include "uapi/page_size.h"
#include <stdlib.h>

uint64_t _malloc_expand_heap(void* heap_end, uint64_t bytes){
    if((uint64_t)heap_end & PAGE_MASK) {abort();}//must be page aligned
    uint64_t rounded_up_pages = (bytes + PAGE_SIZE - 1) / PAGE_SIZE;

    uint64_t i=0;
    for(;i<rounded_up_pages; i++) {
        struct RequestPageData alloc_data = {.page_virt_addr = heap_end + (PAGE_SIZE * i)};
        do_syscall(&alloc_data, REQUEST_PAGE_SYSCALL);
        switch(alloc_data.err) {
            case 0:
            continue;
            case ENOMEM:
            return PAGE_SIZE * i;
            default:
            printf("UNEXPECTED ALLOCATION ERROR");
            abort();
        }
    }

    return PAGE_SIZE * i;
}

void stuos_libc_init() {
    struct GetHeapStartData heap_start;
    do_syscall(&heap_start, GET_HEAP_START_SYSCALL);
    init_memory_allocator(heap_start.output);
}