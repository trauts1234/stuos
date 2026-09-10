#ifndef DISPLAY_H
#define DISPLAY_H

#include <uapi/stdint.h>

struct Colour {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

/// Sets up the display
/// @warning This must be run before writing to the display
void display_init();

/// Draws a pixel at the specified coordinates
void display_write_pixel(uint64_t x, uint64_t y, struct Colour colour);


uint64_t display_get_width();
uint64_t display_get_height();

#endif