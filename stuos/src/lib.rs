#![no_std]

use core::panic::PanicInfo;

#[unsafe(no_mangle)]
pub extern "C" fn givethree() -> i32 {
    return 3;
}


#[panic_handler]
fn p(_info: &PanicInfo) -> ! {
    loop{}
}