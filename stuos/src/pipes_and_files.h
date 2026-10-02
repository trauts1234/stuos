#ifndef PIPES_AND_FILES_H
#define PIPES_AND_FILES_H


#include <uapi/stdint.h>
#include <uapi/stdbool.h>
#include <uapi/stddef.h>

struct FopReadResult {
    // Set as true if the operation has completed
    bool read_something;
    // if read_something, 0 = EOF, >0 = bytes read
    size_t bytes_read;
};
typedef struct FopReadResult (*FopReadFn)(void* special_data, void* output_buf, size_t num_bytes);
/// Attempts to write `num_bytes` from `output_buf` using `special_data`, returning the number of bytes actually written
typedef size_t (*FopWriteFn)(void* special_data, const void* input_buf, size_t num_bytes);

typedef uint64_t (*FopLseekFn)(void* special_data, int64_t off, int whence);
/// Destructs the FileOperations
typedef void (*FopCloseFn) (void* special_data);

/// Represents an open Device - This is not a file descriptor
///
/// Multiple file descriptors can point to me
struct FileOperations {
    /// Heap allocated extra data that the FileOperations uses to do stuff
    void* special_data;
    /// Call this with `special_data` to try and read nonblocking - this should be polled instead of the calling process getting a turn on the scheduler
    FopReadFn read_nonblocking;
    /// Call this with `special_data` to write
    FopWriteFn write;
    /// the byte offset into the Device
    FopLseekFn offset;
    /// Call this only when reference_count == 0, and should free the underlying data
    FopCloseFn close;

    bool is_a_tty;
};

struct FileOperations* fop_generate_file(const char* cwd, const char* path, int open_flags);
void fop_generate_pipe(struct FileOperations* output[2]);

#endif