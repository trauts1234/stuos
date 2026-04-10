#ifndef TOOLS_H
#define TOOLS_H

#include <stdint.h>

static uint64_t state = 2506958121 * 2506958121;

static uint8_t rand8()
{
   state = state * 1664525 + 1013904223;
   return state >> 24;
}

static uint32_t rand32() {
    return rand8() | (rand8() << 8) | (rand8() << 16) | (rand8() << 24);
}

static uint64_t rand64() {
    return rand32() | ((uint64_t)rand32() << 32);
}

static uint8_t skewed8() {
    switch (rand8() & 0xf) {
        case 0: return 0;
        case 1: return 1;
        case 2: return UINT8_MAX;
        case 3: return UINT8_MAX - 1;
        case 4: return (~state) & 0xff;
        case 5:
        case 6:
        case 7: return state & 0xff;
        default: return rand8();
    }
}

static uint32_t skewed32() {
    switch (rand8() & 0xf) {
        case 0: return 0;
        case 1: return 1;
        case 2: return UINT32_MAX;
        case 3: return UINT32_MAX - 1;
        case 4: return (~state) & 0xff'ff'ff'ff;
        case 5:
        case 6:
        case 7: return state & 0xff'ff'ff'ff;
        default: return rand32();
    }
}

static uint64_t skewed64() {
    switch (rand8() & 0xf) {
        case 0: return 0;
        case 1: return 1;
        case 2: return UINT64_MAX;
        case 3: return UINT64_MAX - 1;
        case 4: return ~state;
        case 5:
        case 6:
        case 7: return state;
        default: return rand64();
    }
}

static uint64_t absolute_difference(uint64_t a, uint64_t b) {
  return (a > b) ? (a - b) : (b - a);
}

#endif