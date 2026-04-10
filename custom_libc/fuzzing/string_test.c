#include "stddef.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void string_test() {
    char* str = malloc(1000);
    strcpy(str, "hello world,!");

    char* hello = strtok(str, " a");
    char* world = strtok(NULL, ",");
    char* exclamation = strtok(NULL, "");

    if(strcmp(hello, "hello") || strcmp(world, "world") || strcmp(exclamation, "!")) {
        printf("error in stringtok tests: %s | %s | %s", hello, world, exclamation);
        abort();
    }

    
}