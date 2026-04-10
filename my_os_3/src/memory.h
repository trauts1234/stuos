#include "uapi/stdint.h"
#include <stddef.h>
#include "limine.h"

#ifndef MEMORY_H
#define MEMORY_H

/// Sets up physical RAM, by allocating a region to track which pages are free
void memory_init(volatile struct limine_memmap_response *memmap_response, uint64_t hhdm_offset);
/// Sets up archaic paginging things, and allows interrupts to work properly
void setup_gdt_tss();
/// Copies the original kernel page tables, and returns the physical address of the tree's root (set CR3 to point at them)
uint64_t generate_clean_virtual_addressing();
/// Removes the page table' userland pages under the current virtual addressing. sets CR3 back to a default (the original kernel page tables)
void remove_virtual_addressing();
/// Keeps the kernel page table pointers, but clones the data and page tree, returning physical address of new page table tree
uint64_t clone_memory(uint64_t original_root_phys);
/// Sets CR3 to point at the root of a page table tree
extern void set_pml4_phys(uint64_t physical_addr);
/// Gets CR3, returning a physical address of the root of the page table tree
extern uint64_t get_pml4_phys();
/// @brief allocates and zeroes a new page, and you can pick what the address is
/// @param virtual_addr virtual address of the start of an unmapped page
void allocate_page(void* virtual_addr);
/// @brief Releases a virtual page by walking the paging tree and marking as free - This doesn't free the underlying memory, just means that virtual addresses don't point to it
void deallocate_page(void* virtual_addr);
void debug_allocated_pages();

#endif