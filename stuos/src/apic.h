#ifndef APIC_H
#define APIC_H

#include "uapi/stdint.h"

#define LAPIC_PHYS_ADDR 0xFEE00000ull

void apic_init();

// maps irq to vector
void map_ioapic_interrupt(uint8_t irq, uint8_t vector);

//write an interrupt number here to call an interrupt
uint64_t get_lapic_magic_address();

/// Gets how many milliseconds the OS has been running, by counting the number of timer interrupts that have gone off
uint64_t get_uptime_ms();

#endif