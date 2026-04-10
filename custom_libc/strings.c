#include "ctype.h"


int strcasecmp(const char *s1, const char *s2) {
    unsigned char c1, c2;

    // Walk through both strings simultaneously
    while (*s1 && *s2) {
        c1 = (unsigned char)*s1;
        c2 = (unsigned char)*s2;

        // Convert to lower case for comparison
        c1 = (unsigned char)tolower(c1);
        c2 = (unsigned char)tolower(c2);

        if (c1 != c2)
            return (int)c1 - (int)c2;

        ++s1;
        ++s2;
    }

    c1 = (unsigned char)*s1;
    c2 = (unsigned char)*s2;

    return (int)c1 - (int)c2;
}