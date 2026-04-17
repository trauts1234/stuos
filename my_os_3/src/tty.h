#ifndef TTY_H
#define TTY_H

#include "uapi/stdint.h"

//Write to the TTY
void tty_write_char(char c);
//read fromt the TTY's buffer
//returns 0 if there isn't anything to read
uint64_t tty_read(char* out, uint64_t num);//TODO

//request the TTY to read from the keyboard
void tty_poll_keyboard();

#endif