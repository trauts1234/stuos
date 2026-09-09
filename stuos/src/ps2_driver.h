#ifndef PS2_DRIVER_H
#define PS2_DRIVER_H

#include <uapi/stdbool.h>
#include <uapi/stdint.h>

void initialise_ps2();
//run on interrupt
void handle_incoming_byte(int);

#endif