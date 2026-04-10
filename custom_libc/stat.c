#include "sys/stat.h"

static mode_t _file_mode_creation_mask = 0777;
mode_t umask(mode_t mask) {
    mode_t prev = _file_mode_creation_mask;
    _file_mode_creation_mask = mask & 0777;
    return prev;
}