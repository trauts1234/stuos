use core::{ffi::CStr, mem::MaybeUninit, ptr::write_volatile};
use alloc::{borrow::ToOwned, rc::Rc};
use crate::{memory::{clone_virtual_addressing, set_pml4_phys}, pipes_and_files::{FileOperations, fop_generate_file, fop_generate_pipe}, println, processes::{ChildType, LoadedProgram, ProcessorState, WaitingState}, rs_uapi::{limits::OPEN_MAX, syscalls::{ChdirData, CloseFDData, DupFdData, ForkData, GetPgrpData, GetPidData, IsattyData, LseekFDData, OpenFileData, PipeData, ReadFDData, WaitData}}, scheduling::{get_cwd, queue, run_next_task}};

const DEBUG_SYSCALLS: bool = false;

#[unsafe(no_mangle)]
extern "C" fn syscall_open_file(data: *mut OpenFileData) {
    let data =  unsafe {&mut *data};
    let path = unsafe {CStr::from_ptr(data.path)};
    if DEBUG_SYSCALLS {println!("syscall open file: path: {}", path.to_str().unwrap());};
    let file = unsafe {fop_generate_file(get_cwd(), data.path, data.open_flags)};
    let mut q = queue();
    let (fd_number, free_fd) = q.current_mut().find_free_fd(0).unwrap();
    *free_fd = Some(Rc::new(file));
    data.output_file_descriptor_number = fd_number.try_into().unwrap();
}

#[unsafe(no_mangle)]
extern "C" fn syscall_read_fd(data: *mut ReadFDData, processor_state: *const ProcessorState) {
    let data =  unsafe {&mut *data};
    if DEBUG_SYSCALLS {println!("syscall read fd: read up to {} bytes from fd {}", data.num_bytes, data.file_descriptor_number);}
    let mut q = queue();
    assert!(q.current().waiting_state.is_none());
    q.current_mut().waiting_state = Some(WaitingState::WaitingRead {
        fd_num: data.file_descriptor_number.try_into().unwrap(),
        output_buf: data.buffer,
        num_bytes: data.num_bytes.try_into().unwrap(),
        output_num_bytes_ptr: &raw mut data.num_bytes_actually_read
    });
    drop(q);
    run_next_task(processor_state);
}

#[unsafe(no_mangle)]
extern "C" fn syscall_lseek_fd(data: *mut LseekFDData) {
    let data =  unsafe {&mut *data};
    if DEBUG_SYSCALLS {println!("syscall lseek fd: offset {} in fd {}. whence: {}", data.offset, data.file_descriptor_number, data.whence);}
    let q = queue();
    let file_operations = q.current().file_descriptors[data.file_descriptor_number as usize].as_ref().unwrap();
    data.actual_offset = unsafe {(file_operations.offset)(file_operations.special_data, data.offset, data.whence)}.try_into().unwrap();
}

#[unsafe(no_mangle)]
extern "C" fn syscall_close_fd(data: *mut CloseFDData) {
    let data =  unsafe {&mut *data};
    if DEBUG_SYSCALLS {println!("syscall close fd: fd {}", data.file_descriptor_number);}
    queue().current_mut().file_descriptors[data.file_descriptor_number as usize].take().unwrap();
}

#[unsafe(no_mangle)]
extern "C" fn syscall_fork(data: *mut ForkData, parent_state: *const ProcessorState) {
    if DEBUG_SYSCALLS {println!("syscall fork:");}
    let data =  unsafe {&mut *data};
    let mut q = queue();
    let child_page_table = unsafe {clone_virtual_addressing(q.current().page_table_root)};

    let child = LoadedProgram {
        heap_start: q.current().heap_start,
        page_table_root: child_page_table,
        initial_state: unsafe {parent_state.read()},
    };

    let child_pid = q.push(child);

    //now here's a mad bit - data is a userspace pointer, so changing CR3 will cause the same pointer to point to a different page
    unsafe {
        set_pml4_phys(child_page_table);
        write_volatile(&raw mut data.pid, 0);//give child 0
        set_pml4_phys(q.current().page_table_root);
        write_volatile(&raw mut data.pid, child_pid);// pass the child's PID to the parent
    }
}

#[unsafe(no_mangle)]
extern "C" fn syscall_get_pgrp(data: *mut GetPgrpData) {
    if DEBUG_SYSCALLS {println!("syscall get pgrp:");}
    let data =  unsafe {&mut *data};
    data.result = queue().current().identity.pgrp;
}

#[unsafe(no_mangle)]
extern "C" fn syscall_get_pid(data: *mut GetPidData) {
    if DEBUG_SYSCALLS {println!("syscall get pid:");}
    let data =  unsafe {&mut *data};
    data.result = queue().current().identity.pid;
}

#[unsafe(no_mangle)]
extern "C" fn syscall_dupfd(data: *mut DupFdData) {
    let data =  unsafe {&mut *data};
    if DEBUG_SYSCALLS {println!("syscall dupfd: fd {} => fd>={}", data.fildes, data.min_new_fd);}
    assert!(data.min_new_fd >= 0 && (data.min_new_fd as usize) < OPEN_MAX);
    assert!(data.fildes >= 0 && (data.fildes as usize) < OPEN_MAX);

    let mut q = queue();
    let original_clone = q.current().file_descriptors[data.fildes as usize].as_ref().unwrap().clone();
    let (result_fd, new_fd_location) = q.current_mut().find_free_fd(data.min_new_fd.try_into().unwrap()).unwrap();
    *new_fd_location = Some(original_clone);
    data.result_fd = result_fd.try_into().unwrap();
}

#[unsafe(no_mangle)]
extern "C" fn syscall_chdir(data: *const ChdirData) {
    let data =  unsafe {&*data};
    let path = unsafe {CStr::from_ptr(data.path).to_owned()};
    if DEBUG_SYSCALLS {println!("syscall chdir: {}", path.to_str().unwrap());}
    queue().current_mut().cwd = path;
}

#[unsafe(no_mangle)]
extern "C" fn syscall_wait(data: *mut WaitData, state: *const ProcessorState) {
    let data =  unsafe {&mut *data};
    if DEBUG_SYSCALLS {println!("syscall wait: for pid {}", data.pid);}
    assert!(queue().current().waiting_state.is_none());
    queue().current_mut().waiting_state = Some(WaitingState::WaitingChild {
        child_type: ChildType::from_pid(data.pid),
        status: data.status,
        options: data.options,
        output_pid: &raw mut data.output_pid,
    });

    run_next_task(state);
}

#[unsafe(no_mangle)]
extern "C" fn syscall_isatty(data: *mut IsattyData) {
    let data =  unsafe {&mut *data};
    if DEBUG_SYSCALLS {println!("syscall isatty on fd {}", data.fd);}
    data.result = queue().current().file_descriptors[data.fd as usize].as_ref().unwrap().is_a_tty.into();
}

#[unsafe(no_mangle)]
extern "C" fn syscall_pipe(data: *mut PipeData) {
    let data =  unsafe {&mut *data};
    if DEBUG_SYSCALLS {println!("syscall pipe:");}
    let mut pipes: [MaybeUninit<FileOperations>; 2] = [MaybeUninit::uninit(), MaybeUninit::uninit()];
    unsafe {fop_generate_pipe((&raw mut pipes).cast());}
    let mut q = queue();
    let current = q.current_mut();

    let (fd_a, file) = current.find_free_fd(0).unwrap();
    *file = unsafe {Some(Rc::new(pipes[0].assume_init_read()))};
    let (fd_b, file) = current.find_free_fd(fd_a+1).unwrap();
    *file = unsafe {Some(Rc::new(pipes[1].assume_init_read()))};

    data.fd_a = fd_a.try_into().unwrap();
    data.fd_b = fd_b.try_into().unwrap();
}