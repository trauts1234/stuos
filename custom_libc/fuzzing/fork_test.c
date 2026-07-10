#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <sys/wait.h>

void fork_test() {
    int result = fork();
    if(result == 0) {
        printf("hello from child\n");
        usleep(100 * 1000);
        exit(2);
    } else {
        printf("Hello from parent. Child's pid is %d\n", result);
        int status = 0;
        waitpid(result, &status, 0);
        assert(WIFEXITED(status));
        assert(WEXITSTATUS(status) == 2);
    }

    int result2 = fork();
    if(result2 == 0) {
        printf("hello from child\n");
        abort();
    } else {
        printf("Hello from parent. Child's pid is %d\n", result2);
        usleep(100 * 1000);
        waitpid(0, NULL, 0);
    }
}