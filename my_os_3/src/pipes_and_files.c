#include <stddef.h>
#include "uapi/stdint.h"
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
    };

    return heap_allocation;
}

void free_file_operations(struct FileOperations* ptr){
    if(ptr->reference_count == 0) {HCF}
    ptr->reference_count--;
    if(ptr->reference_count == 0) {
        ptr->close(ptr->special_data);
        kfree(ptr);
    }
}
