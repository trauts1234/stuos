use core::panic::PanicInfo;

use crate::println;

unsafe extern "C" {
    fn loop_hlt() -> !;
}

#[panic_handler]
fn panic_handler(info: &PanicInfo) -> ! {
    println!(
        "halt and catch fire! {}:{} with error {}",
        info.location().map(|loc| loc.file()).unwrap_or("???"),
        info.location().map(|loc| loc.line()).unwrap_or(0),
        info.message().as_str().unwrap_or("")
    );
    unsafe {
        loop_hlt();
    }
}