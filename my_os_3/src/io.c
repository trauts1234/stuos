#include "io.h"

uint8_t in_byte (uint16_t _port) {
  uint8_t rv;
  __asm__ __volatile__ ("inb %1, %0" : "=a" (rv) : "dN" (_port));
  return rv;
}

void out_byte (uint16_t _port, uint8_t _data) {
  __asm__ __volatile__ ("outb %1, %0" : : "dN" (_port), "a" (_data));
}

void out_long(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %w1" : : "a"(val), "Nd"(port));
}

uint32_t in_long(uint16_t port) {
    uint32_t val;
    __asm__ volatile ("inl %w1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

void io_wait() {
    __asm__ __volatile__ ("outb %%al, $0x80" : : "a"(0));
}