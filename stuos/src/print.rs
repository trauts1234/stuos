use core::fmt::{self, Write};
// use crate::print::KernelWriter;

unsafe extern "C" {
    fn printf(fmt: *const u8, ...);
}
pub struct KernelWriter;

impl Write for KernelWriter {
    fn write_str(&mut self, s: &str) -> fmt::Result {
        unsafe {
            printf("%.*s\0".as_ptr() as *const _, s.len(), s.as_ptr());
        }
        Ok(())
    }
}

#[macro_export]
macro_rules! print {
    ($($arg:tt)*) => {
        use core::fmt::Write;
        ::core::write!(crate::print::KernelWriter {}, $($arg)*).unwrap()
    };
}
#[macro_export]
macro_rules! println {
    () => {
        use core::fmt::Write;
        ::core::write!(crate::print::KernelWriter {}, "\n").unwrap()
    };
    ($($arg:tt)*) => {
        use core::fmt::Write;
        ::core::writeln!(crate::print::KernelWriter{}, $($arg)*).unwrap()
    };
}