#![no_std]

extern crate alloc;

mod debugging;
pub mod print;
mod kern_libc;
mod scheduling;
mod pipes_and_files;
pub mod rs_uapi;
pub mod processes;

// #[unsafe(no_mangle)]
// pub extern "C" fn givethree() -> i32 {
//     return 3;
// }