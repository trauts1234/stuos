
#include "stddef.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>

static int digit_value(char c) {
    c = tolower(c);
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'z') return c - 'a' + 10;
    return -1;
}

struct StrToResult {
    bool overflow;//if overflowed unsigned long long, in this case disregard magnitude
    bool sign;
    unsigned long long magnitude;
};

//base <= 36 please
static struct StrToResult strto(const char *nptr, char **endptr, int base) {
    struct StrToResult result = {};
    const char* current = nptr;
    //eat up whitespace if any
    while(isspace(*current)) {current++;}

    //eat a + or -
    switch (*current) {
        case '+':
        current++;break;
        case '-':
        result.sign = true;
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

    const char* current_checkpoint = current;

    while(*current)
    {
        int d = digit_value(*current++);
        if (d < 0 || d >= base) break;

        if(result.overflow) continue;
        
        if(result.magnitude > (ULONG_MAX - d) / base) {
            result.overflow = true;//this means that accumulator*base + d > LONG_MAX, and overflowing
        } else {
            result.magnitude = result.magnitude*base + d;
        }
    }

    //if no digits consumed
    if(current == current_checkpoint) {
        if (endptr) *endptr = (char*)nptr;
        return result;
    }

    if(endptr) *endptr = (char*)current;

    return result;
}

long int strtol(const char *nptr, char **endptr, int base) {
    if(base > 36) {
        errno = EINVAL;
        return 0;
    }
    struct StrToResult r = strto(nptr, endptr, base);

    if(r.sign) {
        if(r.magnitude > (unsigned long long)LONG_MAX + 1ull || r.overflow) {//+1 since there are more negative numbers
            errno = ERANGE;
            return LONG_MIN;
        }
        return -r.magnitude;
    } else {
        if(r.magnitude > (unsigned long long)LONG_MAX || r.overflow) {
            errno = ERANGE;
            return LONG_MAX;
        }
        return r.magnitude;
    }
}

long long int strtoll(const char *nptr, char **endptr, int base) {
    //since long and long long are the same
    return strtol(nptr, endptr, base);
}

unsigned long int strtoul(const char *nptr, char **endptr, int base) {
    if(base > 36) {
        errno = EINVAL;
        return 0;
    }
    struct StrToResult r = strto(nptr, endptr, base);

    if(r.sign) {
        if(r.overflow) {
            errno = ERANGE;
            return ULONG_MAX;
        }
        return -r.magnitude;
    } else {
        if(r.overflow) {
            errno = ERANGE;
            return ULONG_MAX;
        }
        return r.magnitude;
    }
}
unsigned long long int strtoull(const char *nptr, char **endptr,int base) {
    return strtoul(nptr, endptr, base);//since unsigned long long equals unsigned long
}