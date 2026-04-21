#ifndef TTY_H
#define TTY_H

#include "ps2.h"
#include "uapi/stdint.h"

//Write to the TTY
void tty_write_char(char c);

//read from the TTY's buffer
// reads up to num bytes, or up to and including a newline
//returns 0 if there isn't anything to read
uint64_t tty_read(char* out, uint64_t num);

//provide keyboard input to the TTY
//this is called by the PS/2 driver
void tty_provide_stdin(struct KeyEvent ev);

#endif