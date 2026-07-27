#ifndef APIC_H
#define APIC_H

#include "uapi/stdint.h"

void apic_init(void *rsdp_response_phys);

/// Gets how many milliseconds the OS has been running, by counting the number of timer interrupts that have gone off
uint64_t get_uptime_ms();

#endif