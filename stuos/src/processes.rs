use core::{ops::{Index, IndexMut}, sync::atomic::{AtomicI32, Ordering}};

use alloc::{string::{String, ToString}, vec::Vec};
use spin::Mutex;

use crate::{pipes_and_files::{FileOperations, fop_generate_stdin, fop_generate_stdout}, rs_uapi::limits::OPEN_MAX};

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
    pub pgrp: Pid,
    pub ppid: Pid,
}

//can I be sure this send is OK? perhaps heap_start should just be usize
unsafe impl Send for Process{}
pub struct Process {
    //only for searching - you should already know this
    pid: Pid,
    
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

fn default_fd() -> [Option<FileOperations>; OPEN_MAX] {
    let mut result = [const { None }; OPEN_MAX];
    result[0] = Some(unsafe {
        (*fop_generate_stdin()).clone()
    });
    result[1] = Some(unsafe {
        (*fop_generate_stdout()).clone()
    });
    result[2] = Some(unsafe {
        (*fop_generate_stdout()).clone()
    });

    result
}
pub struct ProcessList {
    inner: Vec<Process>
}
impl ProcessList {
    const fn new() -> Self {
        Self { inner: Vec::new() }
    }
    #[unsafe(no_mangle)]
    pub extern "C" fn push_init_process(&mut self, program: LoadedProgram) {

        let new_proc = Process {
            pid: allocate_pid(),
            heap_start: program.heap_start,
            file_descriptors: default_fd(),
            page_table_root: program.page_table_root,
            cwd: "/".to_string(),
            paused_state: program.initial_state,
            waiting_state: None,
            identity: ProcessIdentity {pgrp: 1, ppid: 0}
        };

        self.inner.push(new_proc);
    }
    #[unsafe(no_mangle)]
    pub extern "C" fn push(&mut self, program: LoadedProgram, parent: Pid) {
        let curr_proc = &self[parent];

        let new_proc = Process {
            pid: allocate_pid(),
            heap_start: program.heap_start,
            file_descriptors: default_fd(),
            page_table_root: program.page_table_root,
            cwd: curr_proc.cwd.clone(),
            paused_state: program.initial_state,
            waiting_state: None,
            identity: curr_proc.identity
        };

        self.inner.push(new_proc);
    }
    pub fn remove(&mut self, pid: Pid) {
        let idx = self.inner.iter().enumerate().find(|(_, x)| x.pid == pid).map(|(i, _)| i).unwrap();
        let removed = self.inner.remove(idx);
        assert!(matches!(removed.waiting_state, Some(WaitingState::AmZombie {..})));
        for fd in removed.file_descriptors.into_iter().filter_map(|x| x) {
            (fd.close)(fd.special_data);
        }
    }
}
impl Index<Pid> for ProcessList {
    type Output = Process;

    fn index(&self, index: Pid) -> &Self::Output {
        self.inner.iter().find(|x| x.pid == index).unwrap()
    }
}
impl IndexMut<Pid> for ProcessList {
    fn index_mut(&mut self, index: Pid) -> &mut Self::Output {
        self.inner.iter_mut().find(|x| x.pid == index).unwrap()
    }
}