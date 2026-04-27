#include "stdio.h"
#include "uapi/syscalls.h"
#include "stdlib.h"
#include <errno.h>
#include <string.h>
#include "fcntl.h"

static FILE _stdin = {.file_descriptor_number = 0};
static FILE _stdout = {.file_descriptor_number = 1};
static FILE _stderr = {.file_descriptor_number = 2};

FILE* stdin = &_stdin;
FILE* stdout = &_stdout;
FILE* stderr = &_stderr;

FILE *fopen(const char *pathname, const char *mode) {
    int flags = 0;

    for(const char* curr = mode; *curr; curr++) {
        switch (*curr) {

        case 'w':
        flags |= O_CREAT;
        flags |= O_TRUNC;
        break;

        case 'a':
        flags |= O_CREAT;
        flags |= O_APPEND;
        break;

        case 'r':
        break;

        case 'b'://ignore binary setting
        case '+'://opened with write permissions anyways
        break;

        default:
        abort();
        }
    }
    

    //for now, ignore the rest of mode? r+, etc.

    int fd = open(pathname, flags);

    FILE* output = malloc(sizeof(FILE));
    *output = (FILE) {
        .file_descriptor_number = fd
    };
    return output;
}

int fclose(FILE *stream) {
    struct CloseFDData data = {
        .file_descriptor_number = stream->file_descriptor_number,
        .error_code = 0
    };

    do_syscall(&data, CLOSE_FD_SYSCALL);
    free(stream);

    return data.error_code;
}

int fseek(FILE *stream, long offset, int whence) {
    struct LseekFDData data = {
        .file_descriptor_number = stream->file_descriptor_number,
        .offset = offset,
        .whence = whence
    };

    do_syscall(&data, LSEEK_FD_SYSCALL);

    return data.actual_offset;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t num_bytes = size * nmemb;
    struct ReadFDData data = {
        .file_descriptor_number = stream->file_descriptor_number,
        .buffer = ptr,
        .num_bytes = num_bytes,
        .num_bytes_actually_read = 0
    };
    
    do_syscall(&data, READ_FD_SYSCALL);

    if(data.num_bytes_actually_read % size != 0) {
        abort();//TODO guarantee that all are written, as this error can totally happen
    }

    return data.num_bytes_actually_read / size;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t num_bytes = size * nmemb;
    struct WriteFDData data = {
        .file_descriptor_number = stream->file_descriptor_number,
        .buffer = ptr,
        .num_bytes = num_bytes,
        .num_bytes_actually_written = 0
    };

    do_syscall(&data, WRITE_FD_SYSCALL);

    if(data.num_bytes_actually_written % size != 0) {
        abort();//TODO guarantee that an exact number of items are written, as this error can totally happen
    }

    return data.num_bytes_actually_written / size;
}

int fputs(const char *s, FILE *stream) {
    size_t bytes_to_write = strlen(s);
    size_t num_written = fwrite(s, bytes_to_write, 1, stream);
    if(num_written != 1) {
        abort();//TODO error handling here
    }

    return 0;//no-one actually specifies what this should return
}
int fgetc(FILE *stream) {
    uint8_t buffer[1];
    struct ReadFDData data = {
        .file_descriptor_number = stream->file_descriptor_number,
        .buffer = buffer,
        .num_bytes = 1,
        .num_bytes_actually_read = 0
    };
    do_syscall(&data, READ_FD_SYSCALL);

    if(data.num_bytes_actually_read == 0) {
        return EOF;
    }
    
    return *buffer;
}
char *fgets(char *s, int size, FILE *stream) {
    if(size == 0) {
        *s = 0;
        return s;
    }
    uint8_t* start = (uint8_t*)s;
    uint8_t* curr = start;
    while(1) {
        //read n-1 characters
        if (curr-start == size - 1) break;

        struct ReadFDData data = {
            .file_descriptor_number = stream->file_descriptor_number,
            .buffer = curr,
            .num_bytes = 1,
            .num_bytes_actually_read = 0
        };
        do_syscall(&data, READ_FD_SYSCALL);

        if(*curr == '\n' || data.num_bytes_actually_read == 0) {
            break;
        }
        curr++;
    }
    
    curr[1] = 0;//null-terminate string
    return s;
}

int fflush (FILE *) {return 0;}//no buffering, so no need to flush

int fileno(FILE *stream) {return stream->file_descriptor_number;}

void perror(const char *s) {
    if(s && *s) {
        fprintf(stderr, "%s: ", s);
    }

    fprintf(stderr, "%s\n", strerror(errno));
}

int fputc(int c, FILE *stream) {
    uint8_t arr[1] = {c};
    struct WriteFDData data = {.file_descriptor_number=stream->file_descriptor_number, .buffer=arr, .num_bytes=1};
    do_syscall(&data, WRITE_FD_SYSCALL);
    return c;
}
int puts(const char *s) {
    struct WriteFDData data = {.file_descriptor_number=stdout->file_descriptor_number, .buffer=(const uint8_t*)s, .num_bytes=1};
    do_syscall(&data, WRITE_FD_SYSCALL);
    return 1;
}
