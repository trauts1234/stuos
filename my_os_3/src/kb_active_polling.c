#include "uapi/stdint.h"

#include "io.h"
#include "kb_active_polling.h"

#define KBC_STATUS 0x64
#define KBC_EA 0x60

char kbd_US [128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
  '\t', /* <-- Tab */
  'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, /* <-- control key */
  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',  0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',   0,
  '*',
    0,  /* Alt */
  ' ',  /* Space bar */
    0,  /* Caps lock */
    0,  /* 59 - F1 key ... > */
    0,   0,   0,   0,   0,   0,   0,   0,
    0,  /* < ... F10 */
    0,  /* 69 - Num lock*/
    0,  /* Scroll Lock */
    0,  /* Home key */
    0,  /* Up Arrow */
    0,  /* Page Up */
  '-',
    0,  /* Left Arrow */
    0,
    0,  /* Right Arrow */
  '+',
    0,  /* 79 - End key*/
    0,  /* Down Arrow */
    0,  /* Page Down */
    0,  /* Insert Key */
    0,  /* Delete Key */
    0,   0,   0,
    0,  /* F11 Key */
    0,  /* F12 Key */
    0,  /* All other keys are undefined */
};

static void send_msg_to_kb(uint8_t message) {
    while(in_byte(KBC_STATUS) & 2) {}//wait for cmd buffer being free
    out_byte(KBC_EA, message);
}

struct KeyEvent {
    uint16_t keycode;  // 0xXX or 0xE0XX
    uint8_t  pressed;  // 1 = key down, 0 = key up
};

struct KeyEvent read_keyboard_event(void) {
    static uint8_t have_f0 = 0;  // release prefix (break code)
    static uint8_t have_e0 = 0;  // extended code prefix

    struct KeyEvent ev = {0, 0};

    if (in_byte(KBC_STATUS) & 1) {
        uint8_t sc = in_byte(KBC_EA);

        if (sc == 0xE0) {
            have_e0 = 1;
            return ev; // wait for next byte
        }
        if (sc == 0xF0) {
            have_f0 = 1;
            return ev; // wait for next byte
        }

        // normal scancode
        uint16_t code = sc;
        if (have_e0) {
            code |= 0xE000; // mark extended
            have_e0 = 0;
        }

        ev.keycode = code;
        ev.pressed = have_f0 ? 0 : 1; // 0=release, 1=press
        have_f0 = 0;
    }

    return ev; // ev.keycode==0 means no event
}

void initialise_keyboard() {
    //hopefully the interrupts code will disable ISR 1, since that fires every time a keypress happens
    
    while(in_byte(KBC_STATUS) & 1) {
        in_byte(KBC_EA);
    }

    send_msg_to_kb(0xF4);
    while(in_byte(KBC_STATUS) & 1) {
        in_byte(KBC_EA);
    }

    send_msg_to_kb(0xEE);
}

char read_char_nonblocking(int* pressed) {
    struct KeyEvent ev = read_keyboard_event();
    *pressed = ev.pressed;
    if (ev.keycode == 0 || ev.keycode >= 128) return 0;//otherwise, concurrent keypresses break things
    return kbd_US[ev.keycode];
}