#include "uapi/stdint.h"

/**
 * params (extra_to_allocate) -> extra that was allocated
 *
 * input doesn't have to be aligned, and the output will either be 0 or >= the input
 */
typedef uint64_t (*ExpandHeap)(void*, uint64_t);

typedef struct MemoryAllocator {
  /**
   * Also the start of the heap
   */
  struct LinkedListItem *head;
  struct LinkedListItem *tail;
  /**
   * end of heap, exclusive
   */
  void *heap_end;
  ExpandHeap allocate_more_memory;
} MemoryAllocator;

int32_t init_memory_allocator(ExpandHeap allocate_more_memory,
                              void *heap_start,
                              struct MemoryAllocator *output);

void *allocate(struct MemoryAllocator *self, uint64_t size);

void deallocate(struct MemoryAllocator *self, void *ptr);
