#![no_std]

extern crate alloc;

mod debugging;
pub mod print;
mod kern_libc;
mod scheduling;
mod pipes_and_files;
pub mod rs_uapi;
pub mod processes;
mod syscall_handlers;
#[allow(dead_code, nonstandard_style)]
pub mod memory;