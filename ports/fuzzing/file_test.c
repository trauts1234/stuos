#include "assert.h"
#include "unistd.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tools.h"

void file_test() {
    char* rw_files[] = {"tocreate", "verylongfilenamethatneedsseveralfatentries.thingy"};

    //pre-fill tocreate
    FILE *f = fopen("tocreate", "w");
    fwrite("hello world", sizeof(char), 11, f);
    fclose(f);

    uint16_t counting[4096];
    for(int i=0; i<4096; i++) {counting[i] = i;}
    uint16_t counting_output[4096];

    for(uint64_t i = 0; i<sizeof(rw_files)/sizeof(char*); i++) {
        char* curr_f = rw_files[i];
        printf("testing R/W for %s\n", curr_f);
        FILE* fd = fopen(curr_f, "w");

        uint8_t buf[2];
        uint64_t count = fread(buf, 1, 2, fd);
        if(count != 0) {
            printf("read %lu bytes from an empty file\n", count);
            abort();
        }

        for(uint64_t j = 0; j < 150; j++) {
            uint64_t offset = rand64() % 4096;
            uint64_t num_bytes = rand64() % 4096;

            fseek(fd, offset, SEEK_SET);
            uint64_t written = fwrite(counting, 2, num_bytes, fd);
            uint64_t read = fread(counting_output, 2, num_bytes, fd);
            if(written != num_bytes || read != num_bytes) {
                printf("wrote or read a weird number of items?");
                abort();
            }
            for(uint64_t i=0; i<num_bytes;i++) {
                if(counting_output[i] != counting[i]) {
                    printf("read different bytes than were written, when reading %lu bytes at offset %lu+%lu\n", num_bytes, offset, i*2);
                    printf("read %d, expected %d\n", counting_output[i], counting[i]);
                abort();
                }
            }
            memset(counting_output, 69, sizeof(counting_output));//mangle the array
        }

        fclose(fd);
    }

    char buf[100];
    chdir("/dev");
    getcwd(buf, 100);
    assert(strcmp(buf, "/dev") == 0);
    chdir("/");
}