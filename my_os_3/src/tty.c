#include "debugging.h"
#include "uapi/stdint.h"
#include "display.h"
#include "kern_libc.h"
#include "font8x8_basic.h"
#include "ps2.h"

// 8x8 bitmap chars
static const uint64_t char_memorysize = 8;
// x2 size
static const uint64_t char_scalefactor = 2;
// => 16x16 pixels
static const uint64_t char_screensize = char_memorysize * char_scalefactor;

//width and height in chars, not pixels
static uint64_t chars_width, chars_height;

//index_buffer(cursor_x, cursor_y) points to the next empty character
//if cursor_x == 0, clear the whole row
static unsigned int cursor_x = 0, cursor_y = 0;

//indexed [x + width*y]
//the "top of the screen" is at [0 + chars_width * ((cursor_y+1) % chars_height)] i.e the row below cursor_y
static char *buffer = 0;
//indexed the same, true if backspace should remove this character
static bool *buffer_is_backspaceable;

static enum {
    ONLY_PREV_CHAR_AFFECTED,//cursor_x must be > 0, and only cursor_x-1 will be re-rendered
    ALL_DIRTY//everything will be re-rendered
} current_buffer_dirty_status = ALL_DIRTY;//say whether to only re-render cursor_y or the whole screen

const struct Colour text_col = {.r=255, .g=255, .b=255, .a=0};
const struct Colour background_col = {.r=0,   .g=0,   .b=0,   .a=0};

#define INPUT_BUF_LENGTH 1000
static char input_buffer[INPUT_BUF_LENGTH] = {0};
static char* next = input_buffer;

static char* find_newline_in_input_buffer() {
    for(char* i=input_buffer; i < next; i++) {
        if(*i == '\n') {
            return i;
        }
    }
    return 0;
}

//index into buffer (make sure to wrap beforehand)
static uint64_t index_buffer(uint64_t x, uint64_t y) {
    if(y >= chars_height) HCF
    if(x >= chars_width) HCF//out of range
    return x + chars_width * y;
}


//plot char c at (x,y) pixels from the top left
static void blit_char(char c, uint64_t start_pixel_x, uint64_t start_pixel_y) {
    char* bitmap_start = font8x8_basic[(uint64_t)c];

    for (unsigned int y_offset = 0; y_offset < char_screensize; y_offset++) {
        char current_row = bitmap_start[y_offset / char_scalefactor];
        for (unsigned int x_offset = 0; x_offset < char_screensize; x_offset++) {
            bool is_foreground_pixel = (current_row >> (x_offset / char_scalefactor)) & 1;
            display_write_pixel(
                start_pixel_x + x_offset,
                start_pixel_y + y_offset,
                is_foreground_pixel ? text_col : background_col
            );
        }
    }
}

static void blit_row(uint64_t char_y) {
    bool row_has_seen_newline = false;
    for(uint64_t char_x=0; char_x < chars_width; char_x++) {
        char curr = buffer[index_buffer(char_x, char_y)];
        if (curr == '\n') row_has_seen_newline = true;

        if(row_has_seen_newline) curr = ' ';//print blank if a newline has been entered, to clear the rest of the line

        blit_char(curr, char_x * char_screensize, char_y * char_screensize);
    }
}

//write text buffer to display
static void blit() {
    switch (current_buffer_dirty_status) {

    case ONLY_PREV_CHAR_AFFECTED:
        blit_char(buffer[index_buffer(cursor_x-1, cursor_y)], (cursor_x-1) * char_screensize, cursor_y * char_screensize);
        return;

    // case ONLY_ROW_AFFECTED:
    //     blit_row(cursor_y);
    //     return;

    case ALL_DIRTY:
        //loop from y= the row after the cursor, all the way back round to the cursor
        uint64_t char_y=cursor_y;
        do {
            char_y++;
            char_y %= chars_height;//wrap

            blit_row(char_y);
            
        } while (char_y % chars_height != cursor_y);
    }
}

void initialise_tty() {
    chars_width = display_get_width() / char_screensize;
    chars_height = display_get_height() / char_screensize;

    const uint64_t buf_len = chars_width * chars_height;
    buffer = kmalloc(buf_len);
    buffer_is_backspaceable = kmalloc(buf_len);
    memset(buffer, ' ', buf_len);
    memset(buffer_is_backspaceable, false, buf_len);

    current_buffer_dirty_status = ALL_DIRTY;
    blit();
}

static void write_from_multiple_sources(char c, bool is_from_keyboard) {
    if(c == '\b') {
        //backspace
        //TODO scan backwards to find a backspaceable char, then remove it and shift everything up
        DEBUG_HERE

        current_buffer_dirty_status = ALL_DIRTY;//this is messy
        blit();
        return;
    }

    if(cursor_x == 0) {
        //new line started, clear the previous line
        memset(buffer + index_buffer(cursor_x, cursor_y), ' ', chars_width);
    }

    //write character (\n prints blank so this is OK)
    const uint64_t idx = index_buffer(cursor_x, cursor_y);
    buffer[idx] = c;
    buffer_is_backspaceable[idx] = is_from_keyboard;

    if(c == '\n') {//TODO what about \r
        cursor_y++;
        cursor_x = 0;
        current_buffer_dirty_status = ALL_DIRTY;//new line could shift whole display
    } else {
        cursor_x++;
        current_buffer_dirty_status = ONLY_PREV_CHAR_AFFECTED;//simple char write
    }

    if(cursor_x == chars_width) {
        //overflow, wrap to next line
        cursor_x = 0;
        cursor_y++;
        current_buffer_dirty_status = ALL_DIRTY;//since y is affected
    }

    cursor_y %= chars_height;

    blit();//redraw
}

//Write to the TTY
void tty_write_char(char c) {
    write_from_multiple_sources(c, false);
}

void tty_provide_stdin(struct KeyEvent ev) {
    if(ev.event_type != KE_ASCII) return;
    if(ev.is_break) return;
    
    write_from_multiple_sources(ev.character, true);

    if(ev.character == '\b') {
        if(next == input_buffer) return;//underflow
        next--;
    } else {
        if(next == input_buffer + INPUT_BUF_LENGTH) HCF//overflow
        *next++ = ev.character;
    }
}

uint64_t tty_read(char* out, uint64_t num) {
    char* buffer_newline = find_newline_in_input_buffer();
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