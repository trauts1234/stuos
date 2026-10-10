
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(int argc, char **argv, char**envp) {
    printf("hello from other");
    assert(strcmp(envp[0], "myenv:100") == 0);
    assert(envp[1] == NULL);
    return 0;
}