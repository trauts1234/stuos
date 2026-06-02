#include "setjmp_test.h"
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>

static volatile int count = 0;
static volatile int ret_value = -1;

void setjmp_test() {
    jmp_buf buf;
    ret_value = setjmp(buf);
    printf("after set jump\n");
    count++;

    if(ret_value) {
        if(ret_value != 69) abort();
        if(count != 2) abort();
        printf("success\n");
        return;
    } else {
        printf("long jumping\n");
        longjmp(buf, 69);
    }
    abort();
}