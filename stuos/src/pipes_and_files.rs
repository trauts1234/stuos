use core::ffi::{c_int, c_void};

/// Represents an open Device - This is not a file descriptor
///
/// Multiple file descriptors can point to me
#[repr(C)]
#[derive(Debug, Clone)]
pub struct FileOperations {
    /// Heap allocated extra data that the FileOperations uses to do stuff
    pub special_data: *mut c_void,
    /// Call this with `special_data` to try and read nonblocking - this should be polled instead of the calling process getting a turn on the scheduler
    pub read_nonblocking: FopReadFn,
    /// Call this with `special_data` to write
    pub write: FopWriteFn,
    /// the byte offset into the Device
    pub offset: FopLseekFn,
    pub close: FopCloseFn,
    pub is_a_tty: bool,
}
impl Drop for FileOperations {
    fn drop(&mut self) {
        unsafe {(self.close)(self.special_data)}
    }
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
pub type FopReadFn = unsafe extern "C" fn(
    special_data: *mut c_void,
    output_buf: *mut (),
    num_bytes: usize,
) -> FopReadResult;

/// Attempts to write `num_bytes` from `input_buf` using `special_data`, returning the number of bytes actually written
pub type FopWriteFn = unsafe extern "C" fn(
    special_data: *mut c_void,
    input_buf: *const (),
    num_bytes: usize,
) -> usize;

/// Seek to a new offset
pub type FopLseekFn = unsafe extern "C" fn(
    special_data: *mut c_void,
    off: i64,
    whence: c_int,
) -> u64;

/// Destructs the FileOperations
pub type FopCloseFn = unsafe extern "C" fn(special_data: *mut c_void);

pub extern "C" fn invalid_read(_special_data: *mut c_void, _output_buf: *mut (), _num_bytes: usize) -> FopReadResult {
    panic!("tried to read a file descriptor that can't read")
}
pub extern "C" fn invalid_write(_special_data: *mut c_void, _input_buf: *const (), _num_bytes: usize) -> usize {
    panic!("tried to write a file descriptor that can't write");
}
pub extern "C" fn invalid_lseek(_special_data: *mut c_void, _off: i64, _whence: c_int) -> u64 {
    panic!("tried to seek a file descriptor that is not seekable");
}
pub extern "C" fn do_nothing_close(special_data: *mut c_void) {
    assert!(special_data.is_null());
}

unsafe extern "C" {
    pub fn stdout_write(special_data: *mut c_void, output_buf: *const (), num: usize) -> usize;
    pub fn stdin_read(special_data: *mut c_void, _output_buf: *mut (), num: usize) -> FopReadResult;
}
