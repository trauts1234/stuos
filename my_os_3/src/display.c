#include "uapi/stdint.h"
#include <stddef.h>
#include <stdbool.h>
#include "display.h"
#include "limine.h"
#include "font8x8_basic.h"

static volatile struct limine_framebuffer *raw_framebuffer;
/// only used if `framebuffer_mode` is TEXT_MODE
/// cursor x / cursor y: offsets in number of characters from the top left of the screen
static struct {unsigned int cursor_x; unsigned int cursor_y;} text_mode_settings;

// 8x8 bitmap chars
static const uint64_t char_memorysize = 8;
// x2 size
static const uint64_t char_scalefactor = 2;
// => 16x16 pixels
static const uint64_t char_screensize = char_memorysize * char_scalefactor;

void display_init(volatile struct limine_framebuffer* framebuffer_ptr) {
    raw_framebuffer = framebuffer_ptr;

    text_mode_settings.cursor_x = 0;
    text_mode_settings.cursor_y = 0;
}

void display_clear_screen() {
    text_mode_settings.cursor_x = 0;
    text_mode_settings.cursor_y = 0;
    for(uint64_t x=0; x<raw_framebuffer->width; x++) {
        for(uint64_t y = 0; y<raw_framebuffer->height; y++) {
            display_write_pixel(x, y, (struct Colour) {.r=0, .g=0, .b=0, .a=0});
        }
    }
}

void display_write_pixel(uint64_t x, uint64_t y, struct Colour colour) {
    volatile struct Colour *screen_ptr = (struct Colour*)raw_framebuffer->address;
    uint64_t index = x + raw_framebuffer->width * y;
    screen_ptr[index] = colour;
}

/// @brief moves the cursor to the next character
/// @param require_newline whether to reset to a newline
static void skip_to_next_char(bool require_newline) {
    const uint64_t max_cursor_x = raw_framebuffer->width/char_screensize;
    const uint64_t max_cursor_y = raw_framebuffer->height/char_screensize;

    if(text_mode_settings.cursor_x+1 < max_cursor_x && !require_newline) {
        //enough room to go across 1
        text_mode_settings.cursor_x++;
    } else if(text_mode_settings.cursor_y+1 < max_cursor_y) {
        //go to next row
        text_mode_settings.cursor_x = 0;
        text_mode_settings.cursor_y++;
    } else {
        //hit bottom right. just reset here and overwrite past text
        text_mode_settings.cursor_x = 0;
        text_mode_settings.cursor_y = 0;
    }
}

int display_write_char(char c) {
    const struct Colour white_px = {.r=255, .g=255, .b=255, .a=0};
    const struct Colour black_px = {.r=0, .g=0, .b=0, .a=0};

    if(c <= 0) {
        return -1;
    }

    char* bitmap_start = font8x8_basic[(size_t)c];
    
    const uint64_t char_topleft_x = text_mode_settings.cursor_x * char_screensize;
    const uint64_t char_topleft_y = text_mode_settings.cursor_y * char_screensize;
    //loop through each pixel
    for(unsigned int y_offset=0; y_offset < char_screensize; y_offset++) {
        //the bitmap for the current row I am drawing
        char current_row = bitmap_start[y_offset/char_scalefactor];
        for(unsigned int x_offset=0; x_offset < char_screensize; x_offset++) {
            //whether the current pixel I am drawing is white
            bool is_white = (current_row >> (x_offset/char_scalefactor)) & 1;
            uint64_t current_pixel_x = char_topleft_x + x_offset;
            uint64_t current_pixel_y = char_topleft_y + y_offset;

            display_write_pixel(current_pixel_x, current_pixel_y, is_white ? white_px : black_px);
        }
    }

    //move cursor to next area
    skip_to_next_char(c == '\n');

    return 0;
}