#include "locale.h"
#include "uapi/stddef.h"
#include <assert.h>
#include <string.h>

//I assume I always use C.UTF-8 and will always stay in that mode
//since isw*() functions are unicode and wc*() functions are UTF-8
//remember langinfo.c!!!
char *setlocale(int category, const char *locale) {
    assert(category == LC_CTYPE);//I only support this one currently

    if(strcmp(locale, "C.UTF-8") == 0) {
        return "C.UTF-8";
    }
    return NULL;
}