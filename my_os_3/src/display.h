#ifndef DISPLAY_H
#define DISPLAY_H

#include "uapi/stdint.h"
#include "limine.h"

struct Colour {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

/// Sets up the display
/// @warning This must be run before writing to the display
void display_init(volatile struct limine_framebuffer* framebuffer_ptr);

/// Draws a pixel at the specified coordinates
void display_write_pixel(uint64_t x, uint64_t y, struct Colour colour);
struct Colour display_read_pixel(uint64_t x, uint64_t y);

/// Sets the screen to solid black, and resets the cursor to top-left
void display_clear_screen();

uint64_t display_get_width();
uint64_t display_get_height();

#endif