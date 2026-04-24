#ifndef IO_H
#define IO_H
#include "uapi/stdint.h"

/// Calls the inb instruction
uint8_t in_byte (uint16_t _port);
/// Calls the outb instruction
void out_byte (uint16_t _port, uint8_t _data);
//calls the outl instruction
void out_long(uint16_t port, uint32_t val);
//calls the inl instruction
uint32_t in_long(uint16_t port);
/// Wait for IO? this may be rubbish
void io_wait();

#endif