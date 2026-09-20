#![no_std]

mod debugging;
pub mod print;
mod kern_libc;

// #[unsafe(no_mangle)]
// pub extern "C" fn givethree() -> i32 {
//     return 3;
// }