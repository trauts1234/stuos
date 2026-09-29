use core::{ops::{Index, IndexMut}, sync::atomic::{AtomicI32, Ordering}};

use alloc::{string::String, vec::Vec};
use spin::Mutex;

use crate::{pipes_and_files::FileOperations, rs_uapi::limits::OPEN_MAX};

pub static PROCESSES: Mutex<ProcessList> = Mutex::new(ProcessList::new());

pub type Pid = i32;
static NEXT_FREE_PID: AtomicI32 = AtomicI32::new(1);
fn allocate_pid() -> Pid {
    let next = NEXT_FREE_PID.fetch_add(1, Ordering::Relaxed);
    assert!(next > 0);
    next
}

#[derive(Clone, Copy)]
pub enum ChildType {
    WithPgrp(Pid),
    Any,
    PgrpIsMyPid,
    WithPid(Pid)
}
impl ChildType {
    pub fn is_valid(&self, my_pid: Pid, candidate_pid: Pid, candidate_pgrp: Pid) -> bool {
        match *self {
            Self::WithPgrp(x) => x == candidate_pid,
            Self::Any => true,
            Self::PgrpIsMyPid => my_pid == candidate_pgrp,
            Self::WithPid(x) => x == candidate_pid,
        }
    }
}

#[derive(Clone, Copy)]
pub enum WaitingState {
    WaitingRead {
        fd_num: usize,//type?
        output_buf: *mut (),
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
    heap_start: *const (),
    page_table_root: u64,//or usize?
    file_descriptors: [Option<FileOperations>; OPEN_MAX],
    initial_state: ProcessorState,
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

#[derive(Clone, Copy)]
pub struct ProcessIdentity {
    //only for searching - you should already know this
    pid: Pid,
    pub pgrp: Pid,
    pub ppid: Pid,
}

//can I be sure this send is OK? perhaps heap_start should just be usize
unsafe impl Send for Process{}
pub struct Process {
    /// PAGE_SIZE aligned, represents where the ELF's heap starts - This is only to tell the ELF if they request this information via syscall
    pub heap_start: *const (),

    pub file_descriptors: [Option<FileOperations>; OPEN_MAX],

    pub page_table_root: u64,//or usize?

    pub identity: ProcessIdentity,

    //TODO signals

    pub cwd: String,//or some sort of CString

    pub paused_state: ProcessorState,
    pub waiting_state: Option<WaitingState>,
}

pub struct ProcessList {
    inner: Vec<Process>
}
impl ProcessList {
    const fn new() -> Self {
        Self { inner: Vec::new() }
    }
    pub fn push(&mut self, process: LoadedProgram) {
        self.inner.push(process);
    }
    pub fn remove(&mut self, pid: Pid) {

    }
}
impl Index<Pid> for ProcessList {
    type Output = Process;

    fn index(&self, index: Pid) -> &Self::Output {
        self.inner.iter().find(|x| x.identity.pid == index).unwrap()
    }
}
impl IndexMut<Pid> for ProcessList {
    fn index_mut(&mut self, index: Pid) -> &mut Self::Output {
        self.inner.iter_mut().find(|x| x.identity.pid == index).unwrap()
    }
}