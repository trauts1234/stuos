#include <stddef.h>
#include "pipes_and_files.h"
#include "fs.h"
#include "kb_active_polling.h"
#include "kern_libc.h"
#include "debugging.h"
#include "display.h"
#include "uapi/fcntl.h"

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

/// This is put as a fake read when reading is impossible
static struct FopReadResult invalid_read(void*, uint8_t*, uint64_t) {HCF}
/// This is put as a fake seek when seeking is impossible
static uint64_t invalid_lseek(void*, int64_t, int) {HCF}
/// This is put as a fake write when writing is impossible
static uint64_t invalid_write(void*, const uint8_t*, uint64_t) {HCF}
/// When nothing is required to close the special data, run this
static void do_nothing_close(void* special_data) {
    if(special_data != NULL) {HCF}
}
/// When the data just needs to be freed to close the special data, run this
static void just_free_close(void* special_data) {
    kfree(special_data);
}

static uint64_t pipe_write(void* special_data, const uint8_t* output_buf, uint64_t num) {
    if(special_data == NULL) {HCF}
    struct PipeSpecialData* data = ((struct PipeSpecialData*)special_data)->other_pipe_data;
    if(data == NULL) {HCF}

    //reallocate the buffer every time
    uint64_t new_buffer_length = num + data->buffer_length - data->reader_next_byte;
    void* new_buffer = kmalloc(new_buffer_length);
    if(data->buffer_length) memcpy(new_buffer, data->buffer, data->buffer_length);
    memcpy(new_buffer + data->buffer_length, output_buf, num);

    //replace buffer
    kfree(data->buffer);
    data->buffer = new_buffer;
    data->buffer_length = new_buffer_length;
    data->reader_next_byte = 0;

    return num;
}

static struct FopReadResult pipe_read(void* special_data, uint8_t* output_buf, uint64_t num) {
    if(special_data == NULL) {HCF}
    struct PipeSpecialData* data = special_data;

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

    kfree(data->buffer);
    kfree(data);
}

/// Prints to stdout, and can be put as a file operation
static uint64_t stdout_write(void* special_data, const uint8_t* output_buf, uint64_t num) {
    if(special_data != NULL) {HCF}
    for(uint64_t i=0; i<num; i++) {
        display_write_char(output_buf[i]);
    }
    return num;
}
static struct FopReadResult stdin_read(void* special_data, uint8_t* output_buf, uint64_t num) {
    if(special_data != NULL) {HCF}
    
    int pressed;
    char next = read_char_nonblocking(&pressed);

    //button was released or no button was pressed
    if(!next || !pressed) {
        return (struct FopReadResult) {
            .read_something = false,
            .bytes_read = 0,
        };
    }

    *output_buf = (uint8_t)next;
    return (struct FopReadResult) {
        .read_something = true,
        .bytes_read = 1
    };

}
static uint64_t file_write(void* special_data, const uint8_t* input_buf, uint64_t num) {
    struct OpenVnodeSpecialData* data = special_data;
    return data->file.write_file(data->file.inode_number, data->offset, input_buf, num);
}

static struct FopReadResult file_read(void* special_data, uint8_t* output_buf, uint64_t num) {
    struct OpenVnodeSpecialData* data = special_data;
    uint64_t bytes_read = data->file.read_file(data->file.inode_number, data->offset, output_buf, num);

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

struct FileOperations* fop_generate_stdout(){
    struct FileOperations* heap_allocation = kmalloc(sizeof(struct FileOperations));
    *heap_allocation = (struct FileOperations){
        .reference_count = 1,
        .special_data = NULL,
        .read_nonblocking = invalid_read,
        .write = stdout_write,
        .close = do_nothing_close,
        .offset = invalid_lseek,
        .is_a_tty = true,
    };

    return heap_allocation;
}
struct FileOperations* fop_generate_stdin(){
    struct FileOperations* heap_allocation = kmalloc(sizeof(struct FileOperations));
    *heap_allocation = (struct FileOperations){
        .reference_count = 1,
        .special_data = NULL,
        .read_nonblocking = stdin_read,
        .write = invalid_write,
        .close = do_nothing_close,
        .offset = invalid_lseek,
        .is_a_tty = true,
    };

    return heap_allocation;
}

struct FileOperations* fop_generate_file(const char* cwd, const char* path, int open_flags) {
    struct OpenVnodeSpecialData* file = kmalloc(sizeof(struct OpenVnodeSpecialData));
    *file = (struct OpenVnodeSpecialData) {
        .file = vfs_get(cwd, path, open_flags),
        .offset = 0
    };

    if(open_flags & O_APPEND) HCF// need to set offset to point at EOF

    struct FileOperations* heap_allocation = kmalloc(sizeof(struct FileOperations));
    *heap_allocation = (struct FileOperations) {
        .reference_count = 1,
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
    struct PipeSpecialData* a = kmalloc(sizeof(struct PipeSpecialData));
    struct PipeSpecialData* b = kmalloc(sizeof(struct PipeSpecialData));

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

    struct FileOperations* a_ops = kmalloc(sizeof(struct FileOperations));
    struct FileOperations* b_ops = kmalloc(sizeof(struct FileOperations));

    *a_ops = (struct FileOperations) {
        .reference_count= 1,
        .special_data = a,
        .read_nonblocking = pipe_read,
        .write = pipe_write,
        .close = pipe_close,
        .offset = invalid_lseek,
        .is_a_tty = false,
    };
    *b_ops = (struct FileOperations) {
        .reference_count= 1,
        .special_data = b,
        .read_nonblocking = pipe_read,
        .write = pipe_write,
        .close = pipe_close,
        .offset = invalid_lseek,
        .is_a_tty = false,
    };

    output[0] = a_ops;
    output[1] = b_ops;
}

void free_file_operations(struct FileOperations* ptr){
    if(ptr->reference_count == 0) {HCF}
    ptr->reference_count--;
    if(ptr->reference_count == 0) {
        ptr->close(ptr->special_data);
        kfree(ptr);
    }
}
