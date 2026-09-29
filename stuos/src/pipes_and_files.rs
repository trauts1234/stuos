use core::ffi::{c_int, c_void};

/// Represents an open Device - This is not a file descriptor
///
/// Multiple file descriptors can point to me
#[repr(C)]
#[derive(Debug, Clone)]
pub struct FileOperations {
    /// How many file descriptors point to me
    pub reference_count: u64,
    /// Heap allocated extra data that the FileOperations uses to do stuff
    pub special_data: *mut c_void,
    /// Call this with `special_data` to try and read nonblocking - this should be polled instead of the calling process getting a turn on the scheduler
    pub read_nonblocking: FopReadFn,
    /// Call this with `special_data` to write
    pub write: FopWriteFn,
    /// the byte offset into the Device
    pub offset: FopLseekFn,
    /// Call this only when reference_count == 0, and should free the underlying data
    pub close: FopCloseFn,
    pub is_a_tty: bool,
}

/// Set as true if the operation has completed
#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct FopReadResult {
    pub read_something: bool,
    /// if read_something, 0 = EOF, >0 = bytes read
    pub bytes_read: usize,
}

/// Attempt to read `num_bytes` into `output_buf` using `special_data`
///
/// Returns a `FopReadResult`
pub type FopReadFn = extern "C" fn(
    special_data: *mut c_void,
    output_buf: *mut (),
    num_bytes: usize,
) -> FopReadResult;

/// Attempts to write `num_bytes` from `input_buf` using `special_data`, returning the number of bytes actually written
pub type FopWriteFn = extern "C" fn(
    special_data: *mut c_void,
    input_buf: *const (),
    num_bytes: usize,
) -> usize;

/// Seek to a new offset
pub type FopLseekFn = extern "C" fn(
    special_data: *mut c_void,
    off: i64,
    whence: c_int,
) -> u64;

/// Destructs the FileOperations
pub type FopCloseFn = extern "C" fn(special_data: *mut c_void);

unsafe extern "C" {
    pub fn fop_generate_stdout() -> *mut FileOperations;
    pub fn fop_generate_stdin() -> *mut FileOperations;
}
