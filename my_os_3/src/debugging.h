#ifndef DEBUGGING_H
#define DEBUGGING_H

#include <uapi/stdint.h>

/// Sets up debugging structures - call this before calling *any* debugging.h functions
/// @returns 0 on success, 1 on error
int debugging_init();
/// Writes one byte to the serial port, as ascii
void putchar(char a);

void abort();

__attribute__((noreturn)) void __debugging_hcf(int line, char* file);

#define DEBUG_HERE {printf("I reached line %d in %s\n", __LINE__, __FILE__);}

#define HCF {__debugging_hcf(__LINE__, __FILE__);}

#endif