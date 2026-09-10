#include "debugging.h"
#include <uapi/stdint.h>
#include "display.h"
#include "limine.h"
#include "limine_settings.h"

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = LIMINE_API_REVISION
};

static volatile void* screen = 0;
static uint64_t bytes_per_pixel;
static uint64_t pitch;
static uint64_t width, height;

void display_init() {
    assert(framebuffer_request.response);
    assert(framebuffer_request.response->framebuffer_count >= 1);
    struct limine_framebuffer *buf = framebuffer_request.response->framebuffers[0];

    bytes_per_pixel = buf->bpp / 8;
    assert(bytes_per_pixel == sizeof(struct Colour));//could cause a double fault?
    pitch = buf->pitch;
    width = buf->width;
    height = buf->height;
    screen = buf->address;
}


void display_write_pixel(uint64_t x, uint64_t y, struct Colour colour) {
    assert(screen);
    assert(x < display_get_width());
    assert(y < display_get_height());
    uint64_t offset = x*bytes_per_pixel + pitch * y;
    volatile struct Colour *screen_ptr = (struct Colour*)(screen+offset);
    *screen_ptr = colour;
}

uint64_t display_get_width() {
    assert(screen);
    return width;
}
uint64_t display_get_height() {
    assert(screen);
    return height;
}