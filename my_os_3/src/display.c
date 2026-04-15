#include "debugging.h"
#include "uapi/stdint.h"
#include <stddef.h>
#include <stdbool.h>
#include "display.h"
#include "limine.h"
#include "font8x8_basic.h"

static volatile struct limine_framebuffer *raw_framebuffer;
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
    for (uint64_t x = 0; x < raw_framebuffer->width; x++) {
        for (uint64_t y = 0; y < raw_framebuffer->height; y++) {
            display_write_pixel(x, y, (struct Colour){.r=0, .g=0, .b=0, .a=0});
        }
    }
}

void display_write_pixel(uint64_t x, uint64_t y, struct Colour colour) {
    volatile struct Colour *screen_ptr = (struct Colour*)raw_framebuffer->address;
    uint64_t index = x + raw_framebuffer->width * y;
    screen_ptr[index] = colour;
}

/// @brief Clears a single character-row to black (row_index is in character units, not pixels)
static void display_clear_char_row(uint64_t row_index) {
    const struct Colour black_px = {.r=0, .g=0, .b=0, .a=0};
    const uint64_t pixel_y_start = row_index * char_screensize;
    for (uint64_t y = pixel_y_start; y < pixel_y_start + char_screensize; y++) {
        for (uint64_t x = 0; x < raw_framebuffer->width; x++) {
            display_write_pixel(x, y, black_px);
        }
    }
}

/// @brief Scrolls the entire framebuffer up by one character row, then clears the last row
static void display_scroll_up(void) {
    volatile struct Colour *screen_ptr = (struct Colour*)raw_framebuffer->address;

    // Blit every pixel row upward by char_screensize pixel rows
    for (uint64_t y = 0; y + char_screensize < raw_framebuffer->height; y++) {
        for (uint64_t x = 0; x < raw_framebuffer->width; x++) {
            screen_ptr[x + raw_framebuffer->width * y] =
                screen_ptr[x + raw_framebuffer->width * (y + char_screensize)];
        }
    }

    // Clear the newly revealed bottom character row
    const uint64_t max_cursor_y = raw_framebuffer->height / char_screensize;
    display_clear_char_row(max_cursor_y - 1);
}

/// @brief Advances the cursor to the next character position.
/// @param require_newline whether to force a line-break regardless of horizontal space
static void skip_to_next_char(bool require_newline) {
    const uint64_t max_cursor_x = raw_framebuffer->width  / char_screensize;
    const uint64_t max_cursor_y = raw_framebuffer->height / char_screensize;

    if (text_mode_settings.cursor_x + 1 < max_cursor_x && !require_newline) {
        // Enough room to advance one column
        text_mode_settings.cursor_x++;
    } else if (text_mode_settings.cursor_y + 1 < max_cursor_y) {
        // Move to the next row and clear it before anything is written there
        text_mode_settings.cursor_x = 0;
        text_mode_settings.cursor_y++;
        display_clear_char_row(text_mode_settings.cursor_y);
    } else {
        // Bottom of screen reached: scroll up instead of wrapping to top
        text_mode_settings.cursor_x = 0;
        display_scroll_up();
    }
}

void display_write_char(char c) {
    const struct Colour white_px = {.r=255, .g=255, .b=255, .a=0};
    const struct Colour black_px = {.r=0,   .g=0,   .b=0,   .a=0};

    if(c <= 0) {
        HCF;
    }

    if (c == '\n') {
        skip_to_next_char(true);
        return;
    }

    char* bitmap_start = font8x8_basic[(size_t)c];

    const uint64_t char_topleft_x = text_mode_settings.cursor_x * char_screensize;
    const uint64_t char_topleft_y = text_mode_settings.cursor_y * char_screensize;

    for (unsigned int y_offset = 0; y_offset < char_screensize; y_offset++) {
        char current_row = bitmap_start[y_offset / char_scalefactor];
        for (unsigned int x_offset = 0; x_offset < char_screensize; x_offset++) {
            bool is_white = (current_row >> (x_offset / char_scalefactor)) & 1;
            display_write_pixel(
                char_topleft_x + x_offset,
                char_topleft_y + y_offset,
                is_white ? white_px : black_px
            );
        }
    }

    skip_to_next_char(false);
}