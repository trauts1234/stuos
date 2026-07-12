#include "required.h"
#include "linked_list_item.h"

#define MALLOC_ALIGNMENT 16

struct MemoryAllocator {
    struct LinkedListItem *head, *tail;
    void* heap_end;
};

static struct MemoryAllocator allocator;

static bool expand_heap(uint64_t requested_bytes) {
    uint64_t actual_needed_bytes = requested_bytes + sizeof(struct LinkedListItem);

    //try and allocate the memory
    uint64_t allocated = _malloc_expand_heap(allocator.heap_end, actual_needed_bytes);
    void *old_heap_end = allocator.heap_end;
    allocator.heap_end += allocated;
    if(allocated < actual_needed_bytes) return false; //failed

    lli_insert_after(allocator.tail, old_heap_end, &allocator.tail);
    return true;
}

static struct LinkedListItem *find_or_make_free(uint64_t size) {
    for(struct LinkedListItem *cursor = allocator.head; cursor; cursor = cursor->next) {
        //coalesce free regions
        while(lli_try_merge_after(cursor, &allocator.tail)) {}

        if(lli_region_size(cursor, allocator.heap_end) >= size && lli_is_free(cursor)) {
            return cursor;//found a big enough free region
        }
    }
    //broke out of loop as didn't find region

    struct LinkedListItem *old_last_item_ptr = allocator.tail;
    uint64_t last_size = lli_is_free(old_last_item_ptr) ? lli_region_size(old_last_item_ptr, allocator.heap_end) : 0;//hope that there is some free at the end
    
    uint64_t remaining_to_request = size - last_size;
    bool result = expand_heap(remaining_to_request);
    if(!result) {
        abort();
    }
    if(!allocator.tail->is_free || lli_region_size(allocator.tail, allocator.heap_end) < size) abort();

    return allocator.tail;
}

void init_memory_allocator(void* heap_start) {
    uint64_t allocated_initial_memory = _malloc_expand_heap(heap_start, sizeof(struct LinkedListItem));
    void* heap_end = heap_start + allocated_initial_memory;
    if(allocated_initial_memory < sizeof(struct LinkedListItem)) abort();

    struct LinkedListItem *first_ll_item_ptr = heap_start;
    lli_new(0, first_ll_item_ptr);

    allocator = (struct MemoryAllocator) {
        .heap_end = heap_end,
        .head = first_ll_item_ptr,
        .tail = first_ll_item_ptr
    };
}

void* malloc(size_t size) {
    if(size == 0) return 0;
    //sometimes give more RAM than needed, to maintain alignment
    size = ((size+MALLOC_ALIGNMENT-1)/MALLOC_ALIGNMENT) * MALLOC_ALIGNMENT;
    struct LinkedListItem *reserved_metadata_ptr = find_or_make_free(size);
    //mark area as used
    lli_mark_as(reserved_metadata_ptr, false);
    //get size and data pointer
    uint64_t reserved_size = lli_region_size(reserved_metadata_ptr, allocator.heap_end);
    void* reserved_data = lli_region_start(reserved_metadata_ptr);

    if(reserved_size >= size + sizeof(struct LinkedListItem)) {
        //enough memory for the allocated region, plus a new free region after
        void* next_region_pointer = reserved_data + size;
        lli_insert_after(reserved_metadata_ptr, next_region_pointer, &allocator.tail);
    } else {
        //only just enough memory for the allocation
    }

    return reserved_data;
}

void free(void* ptr) {
    if(ptr == 0) return;
    struct LinkedListItem *ptr_to_ll = ((struct LinkedListItem*)ptr)-1;
    lli_mark_as(ptr_to_ll, true);
}