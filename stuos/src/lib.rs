#![no_std]

extern crate alloc;

pub mod print;
mod scheduling;
mod pipes_and_files;
pub mod rs_uapi;
pub mod processes;
mod syscall_handlers;
#[allow(dead_code, nonstandard_style)]
pub mod memory;
pub mod apic;
pub mod fs2;
pub mod kern_std;
pub mod vfs;