#ifndef PS2_H
#define PS2_H

struct KeyEvent {
    enum {
        KE_NULL,//nothing happened - take this as a NULL value
        KE_ASCII,//ascii letter passed - go and read `character`
        KE_PAUSE,
        KE_PRINTSCR,
        KE_SHIFT,
        KE_CAPS,
    } event_type;

    //only valid if `event_type` == KE_ASCII
    char character;

    bool is_break;
};

void initialise_ps2();
//run in interrupt 33
void handle_incoming_byte();

#endif