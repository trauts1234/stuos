#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include "uapi/stdint.h"

/// Where RSP should be set when using the interrupt stack, so the highest address
extern uint8_t *const interrupt_stack_top;

/// Sets up a timer, calling interrupts occasionally. This must be run before get_uptime_ms(), as the timer must be set up first
void setup_pic_pit_idt();

/// Gets how many milliseconds the OS has been running, by counting the number of timer interrupts that have gone off
uint64_t get_uptime_ms();

#endif