#ifndef IO_H
#define IO_H
#include "uapi/stdint.h"

uint8_t in8(uint16_t _port);
void out8(uint16_t _port, uint8_t _data);

void out16(uint16_t port, uint16_t val);
uint16_t in16(uint16_t port);

void out32(uint16_t port, uint32_t val);
uint32_t in32(uint16_t port);
/// Wait for IO? this may be rubbish
static void io_wait() {}

void spin_wait();

#endif