#include "debugging.h"
#include "kern_libc.h"
#include "io.h"

static const int PORT = 0x3f8;    

int debugging_init() {
    out8(PORT + 1, 0x00);    // Disable all interrupts
    out8(PORT + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    out8(PORT + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
    out8(PORT + 1, 0x00);    //                  (hi byte)
    out8(PORT + 3, 0x03);    // 8 bits, no parity, one stop bit
    out8(PORT + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
    out8(PORT + 4, 0x0B);    // IRQs enabled, RTS/DSR set
    out8(PORT + 4, 0x1E);    // Set in loopback mode, test the serial chip
    out8(PORT + 0, 0xAE);    // Test serial chip (send byte 0xAE and check if serial returns same byte)

    // Check if serial is faulty (i.e: not same byte as sent)
    if(in8(PORT + 0) != 0xAE) {
        return 1;
    }

    // If serial is not faulty set it in normal operation mode
    // (not-loopback with IRQs enabled and OUT#1 and OUT#2 bits enabled)
    out8(PORT + 4, 0x0F);
    return 0;
}

static int is_transmit_empty() {
   return in8(PORT + 5) & 0x20;
}

void putchar(char a) {
   while (!is_transmit_empty());

   out8(PORT,a);
}

void abort() {
    HCF
}

__attribute__((noreturn)) extern void loop_hlt();

__attribute__((noreturn)) void __debugging_hcf(int line, char* file) {
    printf("halt and catch fire!\nline: %d\nfile: %s\n", line, file);
    loop_hlt();
}