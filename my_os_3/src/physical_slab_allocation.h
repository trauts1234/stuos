#ifndef PHYSICAL_SLAB_ALLOCATION_H
#define PHYSICAL_SLAB_ALLOCATION_H

#include <stddef.h>
#include "uapi/stdint.h"
static const size_t PAGE_SIZE = 4096;
static const uint64_t PAGE_MASK = PAGE_SIZE-1;

/// Sets up pages by reserving memory to store whether physical pages are used
void init_slab_allocator(uint64_t base_phys, void* base_virt_hhdm, uint64_t mem_size_bytes);

/// @returns The total amount of ram that could be allocated, assuming none was allocated to start with
uint64_t ram_size_bytes();

/// Allocates a physical page and returns its physical address
uint64_t malloc4k_phys();
/// Marks a page free, using its physical address. Make sure to modify page tables too!
void free4k_phys(uint64_t phys_addr);

#endif