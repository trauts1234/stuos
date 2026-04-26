#include "physical_addresses.h"
#include "debugging.h"

//physical addresses at and above this are unused
static uint64_t reserved_waterline = 0;

uint64_t reserve_next_physical_region(uint64_t num_pages) {
    const uint64_t result = reserved_waterline;
    reserve_physical_region(reserved_waterline, num_pages);
    return result;
}

void reserve_physical_region(uint64_t base_address_override, uint64_t num_pages) {
    if(base_address_override < reserved_waterline) HCF//tried to allocate overlapping range

    reserved_waterline += num_pages;
}