#include "debugging.h"
#include "uapi/stdint.h"
#include "display.h"
#include "limine.h"

static volatile struct limine_framebuffer *raw_framebuffer;

void display_init(volatile struct limine_framebuffer* framebuffer_ptr) {
    raw_framebuffer = framebuffer_ptr;
}

void display_write_pixel(uint64_t x, uint64_t y, struct Colour colour) {
    if(x >= display_get_width()) HCF
    if(y >= display_get_height()) HCF
    volatile struct Colour *screen_ptr = (struct Colour*)raw_framebuffer->address;
    uint64_t index = x + raw_framebuffer->width * y;
    screen_ptr[index] = colour;
}

struct Colour display_read_pixel(uint64_t x, uint64_t y) {
    volatile struct Colour *screen_ptr = (struct Colour*)raw_framebuffer->address;
    uint64_t index = x + raw_framebuffer->width * y;
    return screen_ptr[index];
}

uint64_t display_get_width() {
    return raw_framebuffer->width;
}
uint64_t display_get_height() {
    return raw_framebuffer->height;
}