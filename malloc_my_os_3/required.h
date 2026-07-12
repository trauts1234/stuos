#ifndef REQUIRED_H
#define REQUIRED_H

#include "uapi/stdint.h"
#include "uapi/stddef.h"

//call this 
void init_memory_allocator(void* heap_start);

//the following functions must be provided:
int printf(const char* format, ...);
void abort();
uint64_t _malloc_expand_heap(void*, uint64_t);

#endif