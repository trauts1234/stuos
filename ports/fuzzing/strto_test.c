#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
void strto_test() {
    char* endptr;
    const char* curr;
    //edge cases
    errno = 0;
    curr = "";
    
    assert(strtoul(curr, &endptr, 10) == 0);
    assert(endptr == curr);
    assert(errno == 0);

    assert(strtoul(curr, &endptr, 10) == 0);
    assert(endptr == curr);
    assert(errno == 0);
    
    assert(strtoul(curr, &endptr, 37) == 0);
    assert(endptr == curr);
    assert(errno == EINVAL);

    //base 10
    errno = 0;
    curr = "-2";
    assert(strtoul(curr, &endptr, 10) == (unsigned long)-2);
    assert(endptr == curr + 2);
    assert(errno == 0);
    assert(strtol(curr, &endptr, 10) == -2);
    assert(endptr == curr + 2);
    assert(errno == 0);

    curr = "2";
    assert(strtoul(curr, &endptr, 10) == 2);
    assert(endptr == curr + 1);
    assert(errno == 0);
    assert(strtol(curr, &endptr, 10) == 2);
    assert(endptr == curr + 1);
    assert(errno == 0);

    curr = "200000000000000000000000000000000000000000000000000000000000000000000000";
    assert(strtoul(curr, &endptr, 10) == ULONG_MAX);
    assert(endptr == curr + 72);
    assert(errno == ERANGE);
    errno = 0;
    assert(strtol(curr, &endptr, 10) == LONG_MAX);
    assert(endptr == curr + 72);
    assert(errno == ERANGE);

    curr = "-9223372036854775808";
    errno = 0;
    assert(strtol(curr, &endptr, 10) == LONG_MIN);
    assert(errno == 0);
    curr = "+9223372036854775807";
    assert(strtol(curr, &endptr, 10) == LONG_MAX);
    assert(errno == 0);

    //base 36
    errno = 0;
    curr = "-Z1";
    assert(strtoul(curr, &endptr, 36) == (unsigned long)-(35*36+1));
    assert(endptr == curr + 3);
    assert(errno == 0);
    assert(strtol(curr, &endptr, 36) == -(35*36+1));
    assert(endptr == curr + 3);
    assert(errno == 0);

    curr = "fF";
    assert(strtoul(curr, &endptr, 36) == 15*36+15);
    assert(endptr == curr + 2);
    assert(errno == 0);
    assert(strtol(curr, &endptr, 36) == 15*36+15);
    assert(endptr == curr + 2);
    assert(errno == 0);

    curr = "2000000000000000000000000000G0000000000000000000000000000000000000000000";
    assert(strtoul(curr, &endptr, 36) == ULONG_MAX);
    assert(endptr == curr + 72);
    assert(errno == ERANGE);
    errno = 0;
    assert(strtol(curr, &endptr, 36) == LONG_MAX);
    assert(endptr == curr + 72);
    assert(errno == ERANGE);

    //base 0
    errno = 0;
    curr = "-2";
    assert(strtoul(curr, &endptr, 0) == (unsigned long)-2);
    assert(endptr == curr + 2);
    assert(errno == 0);
    assert(strtol(curr, &endptr, 0) == -2);
    assert(endptr == curr + 2);
    assert(errno == 0);

    curr = "2";
    assert(strtoul(curr, &endptr, 0) == 2);
    assert(endptr == curr + 1);
    assert(errno == 0);
    assert(strtol(curr, &endptr, 0) == 2);
    assert(endptr == curr + 1);
    assert(errno == 0);

    curr = "200000000000000000000000000000000000000000000000000000000000000000000000";
    assert(strtoul(curr, &endptr, 0) == ULONG_MAX);
    assert(endptr == curr + 72);
    assert(errno == ERANGE);
    errno = 0;
    assert(strtol(curr, &endptr, 0) == LONG_MAX);
    assert(endptr == curr + 72);
    assert(errno == ERANGE);

    errno = 0;
    curr = "-200000000000000000000000000000000000000000000000000000000000000000000000";
    assert(strtoul(curr, &endptr, 0) == ULONG_MAX);
    assert(endptr == curr + 73);
    assert(errno == ERANGE);
    errno = 0;
    assert(strtol(curr, &endptr, 0) == LONG_MIN);
    assert(endptr == curr + 73);
    assert(errno == ERANGE);

    curr = "0xFFFFFFFFFFFFFFFF";
    errno = 0;
    assert(strtoul(curr, &endptr, 0) == ULONG_MAX);
    assert(errno == 0);

    curr = "-0x2";
    errno = 0;
    assert(strtoul(curr, &endptr, 0) == (unsigned long)-2);
    assert(endptr == curr + 4);
    assert(errno == 0);
    assert(strtol(curr, &endptr, 0) == -2);
    assert(endptr == curr + 4);
    assert(errno == 0);

    curr = "0xE";
    assert(strtoul(curr, &endptr, 0) == 14);
    assert(endptr == curr + 3);
    assert(errno == 0);
    assert(strtol(curr, &endptr, 0) == 14);
    assert(endptr == curr + 3);
    assert(errno == 0);

    curr = "  0x2000D00000000000000000000000000000000000000000000000000000000000000000";
    assert(strtoul(curr, &endptr, 0) == ULONG_MAX);
    assert(endptr == curr + 74);
    assert(errno == ERANGE);
    errno = 0;
    assert(strtol(curr, &endptr, 0) == LONG_MAX);
    assert(endptr == curr + 74);
    assert(errno == ERANGE);

    curr = "-02";
    assert(strtoul(curr, &endptr, 0) == (unsigned long)-2);
    assert(endptr == curr + 3);
    assert(errno == ERANGE);
    errno = 0;
    assert(strtol(curr, &endptr, 0) == -2);
    assert(endptr == curr + 3);
    assert(errno == 0);

    curr = "+02";
    assert(strtoul(curr, &endptr, 0) == 2);
    assert(endptr == curr + 3);
    assert(errno == 0);
    assert(strtol(curr, &endptr, 0) == 2);
    assert(endptr == curr + 3);
    assert(errno == 0);

    curr = "\t020000000000000000000000000000000000000000000000000000000000000000000000";
    assert(strtoul(curr, &endptr, 0) == ULONG_MAX);
    assert(endptr == curr + 73);
    assert(errno == ERANGE);
    errno = 0;
    assert(strtol(curr, &endptr, 0) == LONG_MAX);
    assert(endptr == curr + 73);
    assert(errno == ERANGE);
}