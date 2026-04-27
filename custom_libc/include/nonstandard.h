#ifndef _NONSTANDARD_H
#define _NONSTANDARD_H

#include "stdint.h"

int getchar_nonblocking(int* pressed);
void write_pixel(uint64_t x, uint64_t y, uint8_t r, uint8_t g, uint8_t b);
uint64_t get_uptime_ms();

#endif