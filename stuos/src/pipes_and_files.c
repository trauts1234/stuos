#include "pipes_and_files.h"
#include "fs.h"
#include "kern_libc.h"
#include "debugging.h"
#include "tty.h"
#include <uapi/fcntl.h>

struct OpenVnodeSpecialData {
    struct VNode file;
    uint64_t offset;
};

struct PipeSpecialData {
    struct PipeSpecialData* other_pipe_data;

    //This data is for my *reading*; to write data, look at the other's pipe data
    void* buffer;
    uint64_t buffer_length;
    //index into buffer to read next byte
    uint64_t reader_next_byte;

};

/// When the data just needs to be freed to close the special data, run this
static void just_free_close(void* special_data) {
    free(special_data);
}

static size_t pipe_write(void* special_data, const void* output_buf, size_t num) {
    if(special_data == NULL) {HCF}
    struct PipeSpecialData* data = ((struct PipeSpecialData*)special_data)->other_pipe_data;
    if(data == NULL) {HCF}//TODO return error, as this can happen

    //reallocate the buffer every time
    uint64_t new_buffer_length = num + data->buffer_length - data->reader_next_byte;
    void* new_buffer = malloc(new_buffer_length);
    if(data->buffer_length) memcpy(new_buffer, data->buffer, data->buffer_length);
    memcpy(new_buffer + data->buffer_length, output_buf, num);

    //replace buffer
    free(data->buffer);
    data->buffer = new_buffer;
    data->buffer_length = new_buffer_length;
    data->reader_next_byte = 0;

    return num;
}

static struct FopReadResult pipe_read(void* special_data, void* output_buf, size_t num) {
    if(special_data == NULL) {HCF}
    struct PipeSpecialData* data = special_data;

    if(data->other_pipe_data == NULL || num == 0) {
        //if write end is closed, I can't read
        return (struct FopReadResult) {
            .read_something = true,
            .bytes_read = 0
        };
    }

    uint64_t max_bytes_to_read = data->buffer_length - data->reader_next_byte;
    if(num < max_bytes_to_read) {
        max_bytes_to_read = num;
    }

    if(max_bytes_to_read == 0) 
        return (struct FopReadResult) {
            .read_something = false,
            .bytes_read = 0
        };

    memcpy(output_buf, data->buffer, max_bytes_to_read);
    data->reader_next_byte += max_bytes_to_read;

    return (struct FopReadResult) {
        .read_something = true,
        .bytes_read = max_bytes_to_read
    };

}

static void pipe_close(void* special_data) {
    if(special_data == NULL) {HCF}
    struct PipeSpecialData* data = special_data;

    //ensure that the other end of the pipe can't access me
    if(data->other_pipe_data) data->other_pipe_data->other_pipe_data = NULL;

    free(data->buffer);
    free(data);
}

/// Prints to stdout, and can be put as a file operation
size_t stdout_write(void* special_data, const void* output_buf, size_t num) {
    if(special_data != NULL) {HCF}
    const char *output = output_buf;
    for(uint64_t i=0; i<num; i++) {
        tty_write_char(output[i]);
    }
    return num;
}
struct FopReadResult stdin_read(void* special_data, void* output_buf, size_t num) {
    if(special_data != NULL) {HCF}

    uint64_t bytes_read = tty_read((char*)output_buf, num);
    if(!bytes_read){
        return (struct FopReadResult) {
            .read_something = false,
            .bytes_read = 0,
        };
    }

    return (struct FopReadResult) {
        .read_something = true,
        .bytes_read = bytes_read,
    };
}
static size_t file_write(void* special_data, const void* input_buf, size_t num) {
    struct OpenVnodeSpecialData* data = special_data;
    return data->file.write_file(data->file.id, data->offset, input_buf, num);
}

static struct FopReadResult file_read(void* special_data, void* output_buf, size_t num) {
    struct OpenVnodeSpecialData* data = special_data;
    uint64_t bytes_read = data->file.read_file(data->file.id, data->offset, output_buf, num);
    data->offset += bytes_read;//skip forward

    return (struct FopReadResult) {
        .read_something = true,
        .bytes_read = bytes_read
    };
}

static uint64_t file_lseek(void* special_data, int64_t off, int whence) {
    struct OpenVnodeSpecialData* data = special_data;

    switch (whence) {
    case 0:
    data->offset = off;
    break;

    case 1:
    HCF

    case 2:
    HCF

    default:
    HCF
    }

    return data->offset;
}

struct FileOperations* fop_generate_file(const char* cwd, const char* path, int open_flags) {
    struct OpenVnodeSpecialData* file = malloc(sizeof(struct OpenVnodeSpecialData));
    *file = (struct OpenVnodeSpecialData) {
        .file = vfs_get(cwd, path, open_flags),
        .offset = 0
    };

    if(open_flags & O_APPEND) HCF// need to set offset to point at EOF

    struct FileOperations* heap_allocation = malloc(sizeof(struct FileOperations));
    *heap_allocation = (struct FileOperations) {
        .special_data = (void*)file,
        .read_nonblocking = file_read,
        .write = file_write,
        .close = just_free_close,
        .offset = file_lseek,
        .is_a_tty = false,
    };

    return heap_allocation;
}

void fop_generate_pipe(struct FileOperations* output[2]) {
    struct PipeSpecialData* a = malloc(sizeof(struct PipeSpecialData));
    struct PipeSpecialData* b = malloc(sizeof(struct PipeSpecialData));

    *a = (struct PipeSpecialData) {
        .other_pipe_data = b,
        .buffer = NULL,
        .buffer_length = 0,
        .reader_next_byte = 0
    };
    *b = (struct PipeSpecialData) {
        .other_pipe_data = a,
        .buffer = NULL,
        .buffer_length = 0,
        .reader_next_byte = 0
    };

    struct FileOperations* a_ops = malloc(sizeof(struct FileOperations));
    struct FileOperations* b_ops = malloc(sizeof(struct FileOperations));

    *a_ops = (struct FileOperations) {
        .special_data = a,
        .read_nonblocking = pipe_read,
        .write = pipe_write,
        .close = pipe_close,
        .offset = 0,
        .is_a_tty = false,
    };
    *b_ops = (struct FileOperations) {
        .special_data = b,
        .read_nonblocking = pipe_read,
        .write = pipe_write,
        .close = pipe_close,
        .offset = 0,
        .is_a_tty = false,
    };

    output[0] = a_ops;
    output[1] = b_ops;
}
