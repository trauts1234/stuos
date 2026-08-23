#ifndef APIC_H
#define APIC_H

#include "uapi/stdint.h"

#define LAPIC_PHYS_ADDR 0xFEE00000ull

void apic_init(void *rsdp_response_phys);

/// Gets how many milliseconds the OS has been running, by counting the number of timer interrupts that have gone off
uint64_t get_uptime_ms();

#endif