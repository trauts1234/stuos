#ifndef TERMIOS_H
#define TERMIOS_H

#include "uapi/termios.h"

int tcgetattr(int fd, struct termios *termios_p);

#endif