
#include <stdio.h>
#include <stdlib.h>

void __assert(bool condition, int line, char* file) {
    if(!condition) {
        fprintf(stderr, "ERROR - assertion failed on line %d in file %s", line, file);
        abort();
    }
}