
#include "stddef.h"
#include <ctype.h>
#include <limits.h>

static int digit_value(char c) {
    c = tolower(c);
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'z') return c - 'a' + 10;
    return -1;
}

long int strtol(const char *nptr, char **endptr, int base) {
    const char* current = nptr;
    //eat up whitespace if any
    while(isspace(*current)) {current++;}

    //eat a + or -
    bool is_negative = false;
    switch (*current) {
        case '+':
        current++;break;
        case '-':
        is_negative = true;
        current++;break;
        default:
        break;
    }

    //if base isn't set, use a default
    if(base == 0)
    {
        if (current[0] == '0' && tolower(current[1]) == 'x') {
            base = 16;
            current += 2;
        } else if (*current == '0') {
            base = 8;
        } else {
            base = 10;
        }
    }

    long int accumulator = 0;
    bool overflowing = false;

    const char* current_checkpoint = current;

    while(1)
    {
        int d = digit_value(*current++);
        if (d < 0 || d >= base) break;

        if(overflowing) continue;

        if(is_negative) {
            if(accumulator < (LONG_MIN + d) / base) {
                overflowing = true;//this means that accumulator*base - d < LONG_MIN, and overflowing
            } else {
                accumulator = accumulator*base - d;
            }
        } else {
            if(accumulator > (LONG_MIN - d) / base) {
                overflowing = true;//this means that accumulator*base + d > LONG_MAX, and overflowing
            } else {
                accumulator = accumulator*base + d;
            }
        }
    }

    //if no digits consumed
    if(current == current_checkpoint) {
        if (endptr) *endptr = (char*)nptr;
        return 0;
    }

    if(endptr) *endptr = (char*)current;

    if(overflowing) {
        return is_negative ? LONG_MIN : LONG_MAX;
    }

    return accumulator;
}

long long int strtoll(const char *nptr, char **endptr, int base) {
    return strtol(nptr, endptr, base);
}