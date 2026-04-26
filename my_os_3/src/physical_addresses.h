#ifndef PHYSICAL_ADDRESSES_H
#define PHYSICAL_ADDRESSES_H
#include "uapi/stdint.h"

//represents a region of physical memory
struct ValidPhysicalPages {uint64_t base_phys; uint64_t num_pages;};

// returns: the base physical address that has been reserved
uint64_t reserve_next_physical_region(uint64_t num_pages);

//reserves a physical memory region, with the specified base address
void reserve_physical_region(uint64_t base_address_override, uint64_t num_pages);

#endif