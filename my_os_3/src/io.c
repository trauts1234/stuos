#include "io.h"

unsigned char in_byte (unsigned short _port) {
  unsigned char rv;
  __asm__ __volatile__ ("inb %1, %0" : "=a" (rv) : "dN" (_port));
  return rv;
}

void out_byte (unsigned short _port, unsigned char _data) {
  __asm__ __volatile__ ("outb %1, %0" : : "dN" (_port), "a" (_data));
}

void io_wait() {
    __asm__ __volatile__ ("outb %%al, $0x80" : : "a"(0));
}