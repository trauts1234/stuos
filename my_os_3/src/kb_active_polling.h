#ifndef KB_ACTIVE_POLLING_H
#define KB_ACTIVE_POLLING_H
// this seems to rely on PS/2 Keyboard emulation?
#include "uapi/stdint.h"

/// Sets up keyboard reading
/// @warning Must be called before any other functions in kb_active_polling.h as the keyboard needs setting up
void initialise_keyboard();

/// Tries to read a char from the keyboard
/// @param pressed An integer to be set to 1 for "key pressed", or 0 for "key released" - only valid if function returns != 0
/// @returns '\0' if no character was pressed, otherwise the character
char read_char_nonblocking(int* pressed);

#endif