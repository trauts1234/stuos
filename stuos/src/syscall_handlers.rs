use core::{ffi::CStr, mem::MaybeUninit, ptr::write_volatile};
use alloc::{borrow::ToOwned, rc::Rc};
use crate::{memory::{clone_virtual_addressing, set_pml4_phys}, pipes_and_files::{FileOperations, fop_generate_file, fop_generate_pipe}, println, processes::{ChildType, LoadedProgram, ProcessorState, WaitingState}, rs_uapi::{limits::OPEN_MAX, syscalls::{CHDIR_SYSCALL, CLOSE_FD_SYSCALL, ChdirData, CloseFDData, DUPFD_SYSCALL, DupFdData, EXECVE_SYSCALL, ExecveData, FORK_SYSCALL, ForkData, GET_CWD_SYSCALL, GET_HEAP_START_SYSCALL, GET_PGRP_SYSCALL, GET_PID_SYSCALL, GET_UPTIME_MS_SYSCALL, GETRLIMIT_SYSCALL, GetCwdData, GetHeapStartData, GetPgrpData, GetPidData, GetRLimitData, GetUptimeMsData, HALT_SYSCALL, HaltSyscallData, ISATTY_SYSCALL, IsattyData, KILL_SYSCALL, LSEEK_FD_SYSCALL, LseekFDData, OPEN_FILE_SYSCALL, OpenFileData, PIPE_SYSCALL, PipeData, READ_FD_SYSCALL, REQUEST_PAGE_SYSCALL, ReadFDData, RequestPageData, SETRLIMIT_SYSCALL, SETSIGNALHANDLER_SYSCALL, SIGPROCMASK_SYSCALL, STAT_SYSCALL, SetRLimitData, StatData, TCGETATTR_SYSCALL, WAIT_SYSCALL, WRITE_FD_SYSCALL, WaitData, WriteFDData, YIELD_SYSCALL}}, scheduling::{get_cwd, queue, run_next_task}};

const DEBUG_SYSCALLS: bool = false;

unsafe extern "C" {
    fn syscall_halt(data: *mut HaltSyscallData);
    fn syscall_get_uptime_ms(data: *mut GetUptimeMsData);
    fn syscall_request_page(data: *mut RequestPageData);
    fn syscall_getcwd(data: *mut GetCwdData);
    fn syscall_execve(data: *const ExecveData);
    fn syscall_stat(data: *mut StatData);
    fn syscall_yield(data: *mut (), processor_state: *const ProcessorState);
}

fn run_rs<T>(f: fn(input: T) -> T, data: *mut ()) {
    unsafe {
        let ptr = data.cast::<T>();
        ptr.write_volatile(f(ptr.read()));
    }
}
fn run_rs_with_processor_state<T>(f: fn(input: T, processor_state: ProcessorState) -> T, data: *mut (), processor_state: ProcessorState) {
    unsafe {
        let ptr = data.cast::<T>();
        ptr.write_volatile(f(ptr.read(), processor_state));
    }
}

//called by the assembly stub
#[unsafe(no_mangle)]
extern "C" fn process_syscall(syscall_number: u64, data: *mut (), processor_state: *const ProcessorState) {
    let processor_state = unsafe {processor_state.read()};
    match syscall_number {
        HALT_SYSCALL => unsafe {syscall_halt(data.cast())},
        GET_UPTIME_MS_SYSCALL => unsafe {syscall_get_uptime_ms(data.cast());},
        REQUEST_PAGE_SYSCALL => unsafe {syscall_request_page(data.cast());},
        GET_HEAP_START_SYSCALL => run_rs(syscall_get_heap_start, data),
        WRITE_FD_SYSCALL => run_rs(syscall_write_fd, data),
        OPEN_FILE_SYSCALL => run_rs(syscall_open_file, data),
        READ_FD_SYSCALL => run_rs_with_processor_state(syscall_read_fd, data, processor_state),
        LSEEK_FD_SYSCALL => run_rs(syscall_lseek_fd, data),
        CLOSE_FD_SYSCALL => run_rs(syscall_close_fd, data),
        FORK_SYSCALL => run_rs_with_processor_state(syscall_fork, data, processor_state),
        GET_PGRP_SYSCALL => run_rs(syscall_get_pgrp, data),
        GET_PID_SYSCALL => run_rs(syscall_get_pid, data),
        DUPFD_SYSCALL => run_rs(syscall_dupfd, data),
        GET_CWD_SYSCALL => unsafe {syscall_getcwd(data.cast());},
        CHDIR_SYSCALL => run_rs(syscall_chdir, data),
        EXECVE_SYSCALL => unsafe {syscall_execve(data.cast())},
        WAIT_SYSCALL => run_rs_with_processor_state(syscall_wait, data, processor_state),
        ISATTY_SYSCALL => run_rs(syscall_isatty, data),
        PIPE_SYSCALL => run_rs(syscall_pipe, data),
        STAT_SYSCALL => unsafe {syscall_stat(data.cast())},
        SIGPROCMASK_SYSCALL => todo!(),
        SETSIGNALHANDLER_SYSCALL => todo!(),
        KILL_SYSCALL => todo!(),
        TCGETATTR_SYSCALL => todo!(),
        SETRLIMIT_SYSCALL => run_rs(syscall_setrlimit, data),
        GETRLIMIT_SYSCALL => run_rs(syscall_getrlimit, data),
        YIELD_SYSCALL => unsafe {syscall_yield(data, &raw const processor_state)},

        x => panic!("invalid syscall {}", x)
    }
}

fn syscall_get_heap_start(_: GetHeapStartData) -> GetHeapStartData {
    if DEBUG_SYSCALLS {println!("syscall get heap start:");}
    GetHeapStartData {
        output: queue().current().heap_start.cast()
    }
}

fn syscall_write_fd(mut data: WriteFDData) -> WriteFDData {
    if DEBUG_SYSCALLS {println!("syscall write fd: write {} bytes to fd {}", data.num_bytes, data.file_descriptor_number);}
    let q = queue();
    let fd = q.current().file_descriptors[data.file_descriptor_number as usize].as_ref().unwrap();
    data.num_bytes_actually_written = unsafe {(fd.write)(fd.special_data, data.buffer, data.num_bytes)};

    data
}

fn syscall_open_file(mut data: OpenFileData) -> OpenFileData {
    let path = unsafe {CStr::from_ptr(data.path)};
    if DEBUG_SYSCALLS {println!("syscall open file: path: {}", path.to_str().unwrap());};
    let file = unsafe {fop_generate_file(get_cwd(), data.path, data.open_flags)};
    let mut q = queue();
    let (fd_number, free_fd) = q.current_mut().find_free_fd(0).unwrap();
    *free_fd = Some(Rc::new(file));
    data.output_file_descriptor_number = fd_number.try_into().unwrap();

    data
}

fn syscall_read_fd(data: ReadFDData, processor_state: ProcessorState) -> ReadFDData {
    if DEBUG_SYSCALLS {println!("syscall read fd: read up to {} bytes from fd {}", data.num_bytes, data.file_descriptor_number);}
    let mut q = queue();
    assert!(q.current().waiting_state.is_none());
    q.current_mut().waiting_state = Some(WaitingState::WaitingRead {
        fd_num: data.file_descriptor_number.try_into().unwrap(),
        output_buf: data.buffer,
        num_bytes: data.num_bytes.try_into().unwrap(),
        output_num_bytes_ptr: data.num_bytes_actually_read
    });
    drop(q);
    run_next_task(&raw const processor_state);
}

fn syscall_lseek_fd(mut data: LseekFDData) -> LseekFDData {
    if DEBUG_SYSCALLS {println!("syscall lseek fd: offset {} in fd {}. whence: {}", data.offset, data.file_descriptor_number, data.whence);}
    let q = queue();
    let file_operations = q.current().file_descriptors[data.file_descriptor_number as usize].as_ref().unwrap();
    data.actual_offset = unsafe {(file_operations.offset)(file_operations.special_data, data.offset, data.whence)}.try_into().unwrap();

    data
}

fn syscall_close_fd(data: CloseFDData) -> CloseFDData {
    if DEBUG_SYSCALLS {println!("syscall close fd: fd {}", data.file_descriptor_number);}
    queue().current_mut().file_descriptors[data.file_descriptor_number as usize].take().unwrap();
    data
}

fn syscall_fork(data: ForkData, parent_state: ProcessorState) -> ForkData {
    if DEBUG_SYSCALLS {println!("syscall fork:");}
    let mut q = queue();
    let child_page_table = unsafe {clone_virtual_addressing(q.current().page_table_root)};

    let child = LoadedProgram {
        heap_start: q.current().heap_start,
        page_table_root: child_page_table,
        initial_state: parent_state,
    };

    let child_pid = q.push(child);

    //now here's a mad bit - data.pid is a userspace pointer, so changing CR3 will cause the same pointer to point to a different page
    unsafe {
        set_pml4_phys(child_page_table);
        write_volatile(data.pid, 0);//give child 0
        set_pml4_phys(q.current().page_table_root);
        write_volatile(data.pid, child_pid);// pass the child's PID to the parent
    }

    data
}

fn syscall_get_pgrp(_: GetPgrpData) -> GetPgrpData {
    if DEBUG_SYSCALLS {println!("syscall get pgrp:");}
    GetPgrpData {
        result: queue().current().identity.pgrp
    }
}

fn syscall_get_pid(_: GetPidData) -> GetPidData {
    if DEBUG_SYSCALLS {println!("syscall get pid:");}
    GetPidData { result: queue().current().identity.pid }
}

fn syscall_dupfd(mut data: DupFdData) -> DupFdData {
    if DEBUG_SYSCALLS {println!("syscall dupfd: fd {} => fd>={}", data.fildes, data.min_new_fd);}
    assert!(data.min_new_fd >= 0 && (data.min_new_fd as usize) < OPEN_MAX);
    assert!(data.fildes >= 0 && (data.fildes as usize) < OPEN_MAX);

    let mut q = queue();
    let original_clone = q.current().file_descriptors[data.fildes as usize].as_ref().unwrap().clone();
    let (result_fd, new_fd_location) = q.current_mut().find_free_fd(data.min_new_fd.try_into().unwrap()).unwrap();
    *new_fd_location = Some(original_clone);
    data.result_fd = result_fd.try_into().unwrap();

    data
}

fn syscall_chdir(data: ChdirData) -> ChdirData {
    let path = unsafe {CStr::from_ptr(data.path).to_owned()};
    if DEBUG_SYSCALLS {println!("syscall chdir: {}", path.to_str().unwrap());}
    queue().current_mut().cwd = path;
    data
}

fn syscall_wait(data: WaitData, state: ProcessorState) -> WaitData {
    if DEBUG_SYSCALLS {println!("syscall wait: for pid {}", data.pid);}
    assert!(queue().current().waiting_state.is_none());
    queue().current_mut().waiting_state = Some(WaitingState::WaitingChild {
        child_type: ChildType::from_pid(data.pid),
        status: data.status,
        options: data.options,
        output_pid: data.output_pid,
    });

    run_next_task(&raw const state);
}

fn syscall_isatty(mut data: IsattyData) -> IsattyData {
    if DEBUG_SYSCALLS {println!("syscall isatty on fd {}", data.fd);}
    data.result = queue().current().file_descriptors[data.fd as usize].as_ref().unwrap().is_a_tty.into();
    
    data
}

fn syscall_pipe(_: PipeData) -> PipeData {
    if DEBUG_SYSCALLS {println!("syscall pipe:");}
    let mut pipes: [MaybeUninit<FileOperations>; 2] = [MaybeUninit::uninit(), MaybeUninit::uninit()];
    unsafe {fop_generate_pipe((&raw mut pipes).cast());}
    let mut q = queue();
    let current = q.current_mut();

    let (fd_a, file) = current.find_free_fd(0).unwrap();
    *file = unsafe {Some(Rc::new(pipes[0].assume_init_read()))};
    let (fd_b, file) = current.find_free_fd(fd_a+1).unwrap();
    *file = unsafe {Some(Rc::new(pipes[1].assume_init_read()))};

    PipeData {
        fd_a: fd_a.try_into().unwrap(),
        fd_b: fd_b.try_into().unwrap()
    }
}

fn syscall_setrlimit(data: SetRLimitData) -> SetRLimitData {
    if DEBUG_SYSCALLS {println!("syscall setrlimit: {:?}", data)}
    queue().current_mut().set_rlimit(data.resource, data.limit);

    data
}
fn syscall_getrlimit(mut data: GetRLimitData) -> GetRLimitData {
    if DEBUG_SYSCALLS {println!("syscall getrlimit: {:?}", data)}
    data.limit = queue().current_mut().get_rlimit(data.resource).limit;

    data
}