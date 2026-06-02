#ifndef SETJMP_H
#define SETJMP_H

#include "uapi/stdint.h"

// #include <setjmp.h>
typedef uint64_t jmp_buf[10];

int setjmp(jmp_buf env);

void longjmp(jmp_buf env, int val);

#endif