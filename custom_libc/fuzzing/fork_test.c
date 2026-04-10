#include "nonstandard.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>

void fork_test() {
    int result = fork();
    if(result == 0) {
        printf("hello from child\n");
        sleep_ms(100);
        abort();
    } else {
        printf("Hello from parent. Child's pid is %d\n", result);
        waitpid(result, NULL, 0);
    }

    int result2 = fork();
    if(result2 == 0) {
        printf("hello from child\n");
        abort();
    } else {
        printf("Hello from parent. Child's pid is %d\n", result2);
        sleep_ms(100);
        waitpid(0, NULL, 0);
    }
}