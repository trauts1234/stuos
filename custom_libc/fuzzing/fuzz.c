#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <nonstandard.h>

#include "fork_test.h"
#include "string_test.h"
#include "get_id_test.h"
#include "stdlib_test.h"
#include "file_test.h"
#include "pipe_test.h"

#include "tools.h"
#include "unistd.h"

void sleep_uptime_test(uint64_t loops) {
    uint64_t prev = 0;
    for(uint64_t i=0; i<loops; i++) {
        printf("on loop %llu\n", i);
        uint64_t delay = rand8();
        uint64_t start = get_uptime_ms();
        if(start < prev) {abort();}
        sleep_ms(delay);
        uint64_t end = get_uptime_ms();
        if(start > end) {abort();}
        int64_t actual = end - start;

        if(actual < 0) {
            printf("had a negative delay???\n");
            abort();
        }
        if(absolute_difference(actual, delay) > 50) {
            printf("delay: %lld, actual: %lld", delay, actual);
            abort();
        }
    }
}

void malloc_free_test(uint64_t loops) {
    const size_t max_alloc_count = 1024;
    const size_t max_alloc_bytes = 10'000'000;
    uint8_t *allocations[max_alloc_count];
    size_t allocation_sizes[max_alloc_count];

    size_t alloc_count = 0;
    size_t alloc_bytes = 0;

    for(uint64_t i=0;i<loops; i++) {
        printf("on loop %llu, total allocated:%lu\n", i, alloc_bytes);
        bool should_alloc = (rand64() & 0x7) == 0 && alloc_count < max_alloc_count;
        bool should_free = (rand64() & 0xf) == 0 && alloc_count > 0;
        bool should_free_zero = (rand64() & 0xff) == 0;

        size_t new_amount = skewed64() & 0xFFFFF;
        if(should_alloc && alloc_bytes + new_amount <= max_alloc_bytes) {
            uint8_t *new_alloc = malloc(new_amount);
            if(new_alloc == 0 && new_amount != 0) {
                printf("OOM maybe?\n");
                abort();
            }

            allocations[alloc_count] = new_alloc;
            allocation_sizes[alloc_count] = new_amount;

            memset(new_alloc, 0xAB, new_amount);

            alloc_count++;
            alloc_bytes += new_amount;
        }

        if(should_free) {
            size_t to_free_idx = skewed32() % alloc_count;
            uint8_t *to_free = allocations[to_free_idx];

            free(to_free);

            //swap-remove
            alloc_bytes -= allocation_sizes[to_free_idx];
            alloc_count--;
            allocations[to_free_idx] = allocations[alloc_count];
            allocation_sizes[to_free_idx] = allocation_sizes[alloc_count];
        }

        if(should_free_zero) {
            free(0);//special case
        }

        for(uint64_t testing_alloc=0; testing_alloc < alloc_count; testing_alloc++) {
            // memset(allocations[testing_alloc], 0xBD, allocation_sizes[testing_alloc]);
            for(uint64_t other_alloc=0; other_alloc < alloc_count; other_alloc++) {
                if(other_alloc == testing_alloc) continue;//don't check this one
                void* first_start = allocations[testing_alloc];
                void* first_end = first_start + allocation_sizes[testing_alloc];
                void* second_start = allocations[other_alloc];
                void* second_end = second_start + allocation_sizes[other_alloc];
                if(first_start < second_end && second_start < first_end) {
                    printf("overlapping:\nfirst: %p to %p\nsecond: %p to %p", first_start, first_end, second_start, second_end);
                    abort();
                }
            }
        }
    }
}


int main(int argc, char *argv[]) {
    if(argc != 2) {
        printf("got %d args: ", argc);
        for(int i=0; i<argc; i++) printf("%s, ", argv[i]);
        abort();
    }
    if(strcmp(argv[1], "helloworld")) {
        printf("got wrong arg: %s", argv[1]);
        abort();
    }
    
    printf("type something");
    char buf[3];
    size_t read = fread(buf, 1, 3, stdin);
    if(read != 1 || !isalnum(buf[0])) {
        printf("bad input");
        abort();
    }

    printf("running sleep test\n");
    sleep_uptime_test(10);
    printf("running malloc test\n");
    malloc_free_test(1000);
    printf("running file test\n");
    file_test();
    printf("running id test\n");
    get_id_test();
    printf("running fork test\n");
    fork_test();
    printf("running string test\n");
    string_test();
    printf("running stdlib test\n");
    stdlib_test();
    printf("running execve test\n");
    if(fork() == 0) {
        execve("/tarfs/other.out", (char*const []) {"/tarfs/other.out", NULL}, NULL);
    }
    printf("running pipe test\n");
    pipe_test();
    fprintf(stdout, "success!\n");
}