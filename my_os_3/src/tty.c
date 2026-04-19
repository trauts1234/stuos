#include "debugging.h"
#include "kb_active_polling.h"
#include "uapi/stdint.h"
#include "display.h"
#include "font8x8_basic.h"

// 8x8 bitmap chars
static const uint64_t char_memorysize = 8;
// x2 size
static const uint64_t char_scalefactor = 2;
// => 16x16 pixels
static const uint64_t char_screensize = char_memorysize * char_scalefactor;

static unsigned int cursor_x = 0, cursor_y = 0;

#define INPUT_BUF_LENGTH 1000
static char input_buffer[INPUT_BUF_LENGTH] = {0};
static char* next = input_buffer;

void tty_poll_keyboard() {
    char character;
    int pressed;

    while (1) {
        character = read_char_nonblocking(&pressed);
        if(character == 0) break;
        if(!pressed) continue;
        
        if(character == '\b') {
            if(next == input_buffer) HCF//underflow
            next--;
        } else {
            if(next == input_buffer + INPUT_BUF_LENGTH) HCF//overflow
            *next++ = character;
        }
        
    }
}

static char* find_newline_in_buffer() {
    for(char* i=input_buffer; i < next; i++) {
        if(*i == '\n') {
            return i;
        }
    }
    return 0;
}

uint64_t tty_read(char* out, uint64_t num) {
    char* buffer_newline = find_newline_in_buffer();
    if(!buffer_newline) return 0;//nothing to read

    uint64_t num_read=0;
    //read until output buffer is full or I have copied a newline
    while (num_read<num && input_buffer + num_read <= buffer_newline) {
        *out++ = input_buffer[num_read++];
    }

    //shuffle the remaining data down
    uint64_t remaining = (uint64_t)(next - input_buffer) - num_read;
    for(uint64_t j=0; j < remaining; j++) {
        input_buffer[j] = input_buffer[j+num_read];
    }
    //adjust write pointer
    next -= num_read;
    
    return num_read;
}

/// @brief Clears a single character-row to black (row_index is in character units, not pixels)
static void tty_clear_char_row(uint64_t row_index) {
    const struct Colour black_px = {.r=0, .g=0, .b=0, .a=0};
    const uint64_t pixel_y_start = row_index * char_screensize;
    for (uint64_t y = pixel_y_start; y < pixel_y_start + char_screensize; y++) {
        for (uint64_t x = 0; x < display_get_width(); x++) {
            display_write_pixel(x, y, black_px);
        }
    }
}

/// @brief Scrolls the entire framebuffer up by one character row, then clears the last row
static void tty_scroll_up(void) {

    // Blit every pixel row upward by char_screensize pixel rows
    for (uint64_t y = 0; y + char_screensize < display_get_height(); y++) {
        for (uint64_t x = 0; x < display_get_width(); x++) {
            display_write_pixel(x, y, display_read_pixel(x, y + char_screensize));
        }
    }

    // Clear the newly revealed bottom character row
    const uint64_t max_cursor_y = display_get_height() / char_screensize;
    tty_clear_char_row(max_cursor_y - 1);
}

/// @brief Advances the cursor to the next character position.
/// @param require_newline whether to force a line-break regardless of horizontal space
static void skip_to_next_char(bool require_newline) {
    const uint64_t max_cursor_x = display_get_width()  / char_screensize;
    const uint64_t max_cursor_y = display_get_height() / char_screensize;

    if (cursor_x + 1 < max_cursor_x && !require_newline) {
        // Enough room to advance one column
        cursor_x++;
    } else if (cursor_y + 1 < max_cursor_y) {
        // Move to the next row and clear it before anything is written there
        cursor_x = 0;
        cursor_y++;
        tty_clear_char_row(cursor_y);
    } else {
        // Bottom of screen reached: scroll up instead of wrapping to top
        cursor_x = 0;
        tty_scroll_up();
    }
}

void tty_write_char(char c) {
    const struct Colour white_px = {.r=255, .g=255, .b=255, .a=0};
    const struct Colour black_px = {.r=0,   .g=0,   .b=0,   .a=0};

    if(c <= 0) {
        HCF;
    }

    if (c == '\n') {
        skip_to_next_char(true);
        return;
    }

    char* bitmap_start = font8x8_basic[(uint64_t)c];

    const uint64_t char_topleft_x = cursor_x * char_screensize;
    const uint64_t char_topleft_y = cursor_y * char_screensize;

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