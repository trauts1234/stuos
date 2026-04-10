#ifndef IO_H
#define IO_H

/// Calls the inb instruction
unsigned char in_byte (unsigned short _port);
/// Calls the outb instruction
void out_byte (unsigned short _port, unsigned char _data);
/// Wait for IO? this may be rubbish
void io_wait();

#endif