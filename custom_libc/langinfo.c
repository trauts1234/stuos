#include "langinfo.h"
#include <assert.h>

char *nl_langinfo(nl_item item) {
    assert(item == CODESET);
    //as explained in locale.c
    return "UTF-8";
}