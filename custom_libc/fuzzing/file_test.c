#include "assert.h"
#include "unistd.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tools.h"

void file_test() {
    char* rw_files[] = {"/ramfs/data.txt", "/dev/disk"};
    const uint64_t num_rw_files = 1;
    //these files should start with their data being equal to the filename
    char* r_files[] = {"/tarfs/data.txt"};
    const uint64_t num_r_files = 1;

    printf("testing R\n");
    for(uint64_t i = 0; i<num_r_files; i++) {
        char* curr_f = r_files[i];
        uint64_t filename_len = strlen(curr_f);
        FILE* fd = fopen(curr_f, "r");

        for(uint64_t j = 0; j<300; j++) {
            uint64_t off = rand64() % filename_len;
            uint64_t size = rand64() % filename_len;
            uint64_t expected_size = size > filename_len - off ? filename_len - off : size;


            //jump to a random location
            fseek(fd, off, SEEK_SET);
            //read into a zero-terminated buffer
            char* buf = malloc(size + 1);
            memset(buf, 0, size + 1);
            uint64_t actual_size = fread(buf, sizeof(char), size, fd);

            if(actual_size != expected_size) {
                printf("expected %lld bytes, read %lld", expected_size, actual_size);
                abort();
            }

            if(memcmp(buf, curr_f + off, expected_size)) {
                printf("read incorrect data from file! expected %s, got %s", curr_f + off, buf);
                abort();
            }

            free(buf);

        }

        fclose(fd);
    }

    uint16_t counting[4096];
    for(int i=0; i<4096; i++) {counting[i] = i;}
    uint16_t counting_output[4096];

    printf("testing R/W\n");
    for(uint64_t i = 0; i<num_rw_files; i++) {
        char* curr_f = rw_files[i];
        FILE* fd = fopen(curr_f, "w");

        uint8_t buf[2];
        uint64_t count = fread(buf, 1, 2, fd);
        if(count != 0) {
            printf("read %lld bytes from an empty file", count);
            abort();
        }

        for(uint64_t j = 0; j < 300; j++) {
            uint64_t offset = rand64() % 4096;
            uint64_t num_bytes = rand64() % 4096;

            fseek(fd, offset, SEEK_SET);
            uint64_t written = fwrite(counting, 2, num_bytes, fd);
            uint64_t read = fread(counting_output, 2, num_bytes, fd);
            if(written != num_bytes || read != num_bytes) {
                printf("wrote or read a weird number of items?");
                abort();
            }
            if(memcmp(counting, counting_output, num_bytes)) {
                printf("read different bytes than were written");
                abort();
            }
            memset(counting_output, 69, sizeof(counting_output));//mangle the array
        }

        fclose(fd);
    }

    char buf[100];
    chdir("/tarfs");
    getcwd(buf, 100);
    assert(strcmp(buf, "/tarfs") == 0);

    FILE* with_cwd = fopen("data.txt", "");
    if(with_cwd == NULL) printf("file is null");
    fread(buf, sizeof(char), 100, with_cwd);
    if(strcmp(buf, "/tarfs/data.txt")) {
        printf("read incorrect data: %s", buf);
        abort();
    }
    fclose(with_cwd);
}