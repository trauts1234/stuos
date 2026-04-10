#include "physical_slab_allocation.h"
#include "debugging.h"
#include "display.h"
#include <stddef.h>
#include "uapi/stdint.h"
#include "kern_libc.h"

struct ValidPhysicalPages {uint64_t base_phys; uint64_t num_pages;};

/// The physical addresses that can be allocated, and aren't used for keeping track of which pages are malloc'ed
static struct ValidPhysicalPages valid_physical_memory;
/// HHDM address of a list of booleans marking whether a page is free (at index n, the n'th page of valid_physical_memory)
static bool* slab_flags_base;

uint64_t ram_size_bytes() {
    return valid_physical_memory.num_pages * PAGE_SIZE;
}

void debug_allocated_pages() {
    display_write_char('\n');
    for(size_t page_num = 0;page_num < valid_physical_memory.num_pages; page_num++) {
        if(slab_flags_base[page_num]) {
            display_write_char('#');
        } else {
            display_write_char(' ');
        }
    }
}

uint64_t malloc4k_phys() {
    size_t page_num;
    for(page_num = 0;page_num < valid_physical_memory.num_pages; page_num++) {
        if(slab_flags_base[page_num] == 0) {
            //free page
            slab_flags_base[page_num] = 1;//allocate it
            return valid_physical_memory.base_phys + page_num*PAGE_SIZE;
        }
    }
    HCF
    return 0;//out of memory
}

void free4k_phys(uint64_t phys_addr) {
    if(phys_addr % PAGE_SIZE != 0) {
        HCF//not the start of a page - but it should be
    }

    size_t page_offset = (phys_addr - valid_physical_memory.base_phys) / PAGE_SIZE;
    slab_flags_base[page_offset] = 0;
}

void init_slab_allocator(uint64_t base_phys, void* base_virt_hhdm, uint64_t mem_size_bytes) {
    uint64_t total_page_count = mem_size_bytes / PAGE_SIZE;
    uint64_t pages_required_for_slab_flag_data = (total_page_count+PAGE_SIZE-1) / PAGE_SIZE;//1 byte of data for each 4k page (round up)

    slab_flags_base = (bool*)base_virt_hhdm;
    valid_physical_memory = (struct ValidPhysicalPages){
        .base_phys = base_phys + pages_required_for_slab_flag_data * PAGE_SIZE,//start valid memory after all the memory used for slab flags
        .num_pages = total_page_count - pages_required_for_slab_flag_data
    };

    memset(slab_flags_base, 0, valid_physical_memory.num_pages);//set all pages as unused
}