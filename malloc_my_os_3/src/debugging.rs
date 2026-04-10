

unsafe extern "C" {
    fn debug_print(a: *const u8);
    fn debug_int(num: u64);
    fn debug_hex(num: u64);
}

pub fn print_str(a: &str) {
    assert!(a.ends_with("\0"));
    unsafe {debug_print(a.as_ptr());}
}
pub fn print_int(num: u64) {
    unsafe {debug_int(num);}
}
pub fn print_hex(num: u64) {
    unsafe {debug_hex(num);}
}