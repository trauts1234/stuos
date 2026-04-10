#ifndef DEBUGGING_H
#define DEBUGGING_H

#include "uapi/stdint.h"

/// Sets up debugging structures - call this before calling *any* debugging.h functions
/// @returns 0 on success, 1 on error
int debugging_init();
/// Prints a null-terminated string to the serial port, for debugging purposes
void debug_print(const char *a);
/// Prints an integer in base 10 to the serial port
void debug_int(uint64_t num);
/// Prints an integer in hex to the serial port
void debug_hex(uint64_t num);
/// Writes one byte to the serial port, as ascii
void debug_writechar(char a);

__attribute__((noreturn)) void __debugging_hcf(int line, char* file);

#define DEBUG_HERE {debug_print("I reached line "); \
    debug_int(__LINE__); \
    debug_print(" in "); \
    debug_print(__FILE__); \
    debug_print("\n");}

#define HCF {__debugging_hcf(__LINE__, __FILE__);}

#endif