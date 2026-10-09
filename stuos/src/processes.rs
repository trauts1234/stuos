use core::{ffi::c_void, ptr::null_mut};
use alloc::{ffi::CString, rc::Rc};
use crate::{memory::{get_pml4_phys, remove_virtual_addressing, set_pml4_phys}, pipes_and_files::{FileOperations, do_nothing_close, invalid_lseek, invalid_read, invalid_write, stdin_read, stdout_write}, rs_uapi::{limits::OPEN_MAX, syscalls::{RLIMIT_DATA, rlim_t, rlimit}, types::Pid}};

impl Default for rlimit {
    fn default() -> Self {
        Self { rlim_cur: rlim_t::MAX, rlim_max: rlim_t::MAX }
    }
}

#[derive(Default, Clone, Copy, Debug)]
#[repr(C)]
pub struct LimitData {
    pub current_value: rlim_t,
    pub limit: rlimit
}

pub enum AllocateLimitResult<S,E> {
    //limit usage was increased, and closure was successful
    Success(S),
    //limit was hit, and closure wasn't run
    AboveLimit,
    //limit left unchanged as closure failed
    Error(E)
}

impl LimitData {
    pub fn try_allocate<S,E,F>(&mut self, increase: rlim_t, f: F) -> AllocateLimitResult<S,E>
    where F: FnOnce() -> Result<S,E>
    {
        let new_value = self.current_value.checked_add(increase).unwrap();

        if new_value > self.limit.rlim_cur {
            AllocateLimitResult::AboveLimit
        } else {
            //limit can be raised, so try and run the closure
            match f() {
                Ok(x) => {
                    self.current_value = new_value;
                    AllocateLimitResult::Success(x)
                }
                Err(x) => AllocateLimitResult::Error(x)
            }
        }
    }
    pub fn try_update(&mut self, new_limit: &rlimit) {
        assert!(new_limit.rlim_max <= self.limit.rlim_max);
        self.limit.rlim_max = new_limit.rlim_max;

        assert!(new_limit.rlim_cur <= self.limit.rlim_max);
        self.limit.rlim_cur = new_limit.rlim_cur;
    }
}

#[derive(Clone, Copy, Debug)]
pub enum ChildType {
    WithPgrp(Pid),
    Any,
    PgrpIsMyPgrp,
    WithPid(Pid)
}
impl ChildType {
    //follows the rules from wait() on what to filter based on pid value
    pub fn from_pid(pid: Pid) -> Self {
        match pid {
            -1 => Self::Any,
            0 => Self::PgrpIsMyPgrp,
            x@1.. => Self::WithPid(x),
            x@Pid::MIN..-1 => Self::WithPgrp(x.abs())
        }
    }
    pub fn is_valid(&self, my_pgrp: Pid, candidate_pid: Pid, candidate_pgrp: Pid) -> bool {
        match *self {
            Self::WithPgrp(x) => x == candidate_pid,
            Self::Any => true,
            Self::PgrpIsMyPgrp => my_pgrp == candidate_pgrp,
            Self::WithPid(x) => x == candidate_pid,
        }
    }
}

#[derive(Clone, Copy, Debug)]
pub enum WaitingState {
    WaitingRead {
        fd_num: usize,//type?
        output_buf: *mut c_void,
        num_bytes: usize,
        output_num_bytes_ptr: *mut usize
    },
    WaitingChild {
        child_type: ChildType,
        status: *mut i32,
        options: i32,//?
        output_pid: *mut i32
    },
    AmZombie {
        exit_code: u8
    }
}


#[repr(C)]
pub struct LoadedProgram {
    pub heap_start: *mut (),
    pub page_table_root: u64,//or usize?
    pub initial_state: ProcessorState,
}

#[repr(C)]
#[derive(Debug, Copy, Clone, Default)]
pub struct ProcessorState {
    // General purpose registers saved by assembly stub
    pub r15: u64,
    pub r14: u64,
    pub r13: u64,
    pub r12: u64,
    pub r11: u64,
    pub r10: u64,
    pub r9: u64,
    pub r8: u64,
    pub rsi: u64,
    pub rdi: u64,
    pub rbp: u64,
    pub rdx: u64,
    pub rcx: u64,
    pub rbx: u64,
    pub rax: u64,

    // CPU-pushed state from actual interrupt
    pub rip: u64,
    pub cs: u64,
    pub rflags: u64,
    pub rsp: u64,
    pub ss: u64,
}

#[derive(Clone, Copy, Debug)]
pub struct ProcessIdentity {
    // pub pid: Pid,
    pub pgrp: Pid,
    pub ppid: Pid,
}

pub enum PollResult {
    DoNothing,
    StartUserland(ProcessorState),
    HandleWaiting {
        remove_zombie: Option<Pid>
    }
}

#[derive(Debug)]
pub struct Thread {
    pub paused_state: ProcessorState,
    pub waiting_state: Option<WaitingState>,
}
impl Thread {
    fn new(state: ProcessorState) -> Self {
        Thread {
            paused_state: state,
            waiting_state: None,
        }
    }
}

//can I be sure this send is OK? perhaps heap_start should just be usize
unsafe impl Send for Process{}
#[derive(Debug)]
pub struct Process {
    /// PAGE_SIZE aligned, represents where the ELF's heap starts - This is only to tell the ELF if they request this information via syscall
    pub heap_start: *mut (),

    pub file_descriptors: [Option<Rc<FileOperations>>; OPEN_MAX],

    pub page_table_root: u64,//or usize?

    pub identity: ProcessIdentity,

    //TODO array of these
    threads: Thread,

    //TODO signals

    pub cwd: CString,

    pub memory_limit: LimitData,
}

impl Drop for Process {
    fn drop(&mut self) {
        //remove page tables
        unsafe {
            let pt = get_pml4_phys();
            set_pml4_phys(self.page_table_root);
            remove_virtual_addressing();
            set_pml4_phys(pt);
        }

        //file descriptors are dropped implicitly
        // TODO should this happen when they go zombie, not now
    }
}

impl Process {
    pub fn create(program: LoadedProgram) -> Self {
        Self {
            heap_start: program.heap_start,
            file_descriptors: default_fd(),
            page_table_root: program.page_table_root,
            cwd: CString::new("/").unwrap(),
            memory_limit: Default::default(),
            threads: Thread::new(program.initial_state),
            identity: ProcessIdentity {pgrp: 1, ppid: 0},
        }
    }
    pub fn create_with_parent(program: LoadedProgram, parent: &Self, parent_pid: Pid) -> Self {
        Process {
            heap_start: program.heap_start,
            file_descriptors: parent.file_descriptors.clone(),
            page_table_root: program.page_table_root,
            cwd: parent.cwd.clone(),
            memory_limit: parent.memory_limit,
            threads: Thread::new(program.initial_state),
            identity: ProcessIdentity { pgrp: parent.identity.pgrp, ppid: parent_pid }
        }
    }
    pub fn replace(&mut self, program: LoadedProgram) {
        *self = Process {
            heap_start: program.heap_start,
            file_descriptors: self.file_descriptors.clone(),
            page_table_root: program.page_table_root,
            cwd: self.cwd.clone(),
            memory_limit: self.memory_limit,
            threads: Thread::new(program.initial_state),
            identity: self.identity
        }
    }
    pub fn find_free_fd(&mut self, minimum: usize) -> Result<(usize, &mut Option<Rc<FileOperations>>), ()> {
        for (i, item) in self.file_descriptors.iter_mut().enumerate() {
            if item.is_none() && i >= minimum {
                return Ok((i, item))
            }
        }
        Err(())
    }

    pub fn rlimit_mut(&mut self, resource: i32) -> &mut LimitData {
        match resource.try_into().unwrap() {
            RLIMIT_DATA => &mut self.memory_limit,
            x => panic!("invalid resource {}", x)
        }
    }
    pub fn rlimit(&self, resource: i32) -> &LimitData {
        match resource.try_into().unwrap() {
            RLIMIT_DATA => &self.memory_limit,
            x => panic!("invalid resource {}", x)
        }
    }

    pub fn curr_thread(&self) -> &Thread {
        &self.threads
    }
    pub fn curr_thread_mut(&mut self) -> &mut Thread {
        &mut self.threads
    }

    pub fn poll<'a>(&'a self, children: impl IntoIterator<Item=(Pid, &'a Process)>) -> PollResult {
        match self.threads.waiting_state {
            None => {
                PollResult::StartUserland(self.threads.paused_state)
            },
            Some(WaitingState::WaitingRead { fd_num, output_buf, num_bytes, output_num_bytes_ptr }) => {
                let fop = self.file_descriptors[fd_num].as_ref().unwrap();
                let result = unsafe {(fop.read_nonblocking)(fop.special_data, output_buf, num_bytes)};
                if result.read_something {
                    unsafe {*output_num_bytes_ptr = result.bytes_read}
                    PollResult::HandleWaiting { remove_zombie: None }
                } else {
                    PollResult::DoNothing
                }
            }
            Some(WaitingState::WaitingChild {child_type, status, options, output_pid }) => {
                assert!(options == 0);//TODO options
                //TODO what if I wait for myself
                for (child_pid, proc) in children {
                    if !child_type.is_valid(self.identity.pgrp, child_pid, proc.identity.pgrp) {continue;}//only find acceptable children
                    if let Some(WaitingState::AmZombie { exit_code }) = proc.threads.waiting_state {
                        assert!(!output_pid.is_null());
                        unsafe {*output_pid = exit_code.into();}
                        if !status.is_null() {
                            unsafe {*status = exit_code as i32}
                        }
                        return PollResult::HandleWaiting { remove_zombie: Some(child_pid) };
                    }
                }
                PollResult::DoNothing
            }
            Some(WaitingState::AmZombie { .. }) => {PollResult::DoNothing}
        }
    }
}

fn default_fd() -> [Option<Rc<FileOperations>>; OPEN_MAX] {
    let mut result = [const { None }; OPEN_MAX];
    //stdin
    result[0] = Some(Rc::new(FileOperations {
        special_data: null_mut(),
        read_nonblocking: stdin_read,
        write: invalid_write,
        offset: invalid_lseek,
        close: do_nothing_close,
        is_a_tty: true,
    }));
    //stdout
    result[1] = Some(Rc::new(FileOperations {
        special_data: null_mut(),
        read_nonblocking: invalid_read,
        write: stdout_write,
        offset: invalid_lseek,
        close: do_nothing_close,
        is_a_tty: true
    }));
    //stderr
    result[2] = Some(Rc::new(FileOperations {
        special_data: null_mut(),
        read_nonblocking: invalid_read,
        write: stdout_write,
        offset: invalid_lseek,
        close: do_nothing_close,
        is_a_tty: true
    }));

    result
}