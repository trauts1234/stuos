use core::sync::atomic::{AtomicI32, Ordering};
use alloc::string::{String, ToString};
use crate::{pipes_and_files::{FileOperations, fop_generate_stdin, fop_generate_stdout}, rs_uapi::{limits::OPEN_MAX, types::Pid, wait::WIFEXITED_MASK}};

unsafe extern "C" {
    fn start_userland(processor_state: *const ProcessorState) -> !;
}

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
    pub pid: Pid,
    pub pgrp: Pid,
    pub ppid: Pid,
}

pub struct PollResult {
    pub stop_waiting: bool,
    pub remove_zombie: Option<Pid>
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

impl Drop for Process {
    fn drop(&mut self) {
        assert!(matches!(self.waiting_state, Some(WaitingState::AmZombie {..})));
        for fd in self.file_descriptors.iter().filter_map(|x| x.as_ref()) {
            (fd.close)(fd.special_data);
        }
        todo!();
    }
}

impl Process {
    pub fn create(program: LoadedProgram) -> Self {
        Self {
            heap_start: program.heap_start,
            file_descriptors: default_fd(),
            page_table_root: program.page_table_root,
            cwd: "/".to_string(),
            paused_state: program.initial_state,
            waiting_state: None,
            identity: ProcessIdentity {pgrp: 1, ppid: 0, pid: allocate_pid()}
        }
    }
    pub fn create_with_parent(program: LoadedProgram, parent: &Self) -> Self {
        Process {
            heap_start: program.heap_start,
            file_descriptors: default_fd(),
            page_table_root: program.page_table_root,
            cwd: parent.cwd.clone(),
            paused_state: program.initial_state,
            waiting_state: None,
            identity: ProcessIdentity { pid: allocate_pid(), pgrp: parent.identity.pgrp, ppid: parent.identity.ppid }
        }
    }

    pub fn poll<'a>(&'a self, others: impl IntoIterator<Item=&'a Process>) -> PollResult {
        match self.waiting_state {
            None => {
                unsafe {start_userland(&self.paused_state)}
            },
            Some(WaitingState::WaitingRead { fd_num, output_buf, num_bytes, output_num_bytes_ptr }) => {
                let fop = self.file_descriptors[fd_num].as_ref().unwrap();
                let result = (fop.read_nonblocking)(fop.special_data, output_buf, num_bytes);
                if result.read_something {
                    unsafe {*output_num_bytes_ptr = result.bytes_read}
                    PollResult{ stop_waiting: true, remove_zombie: None }
                } else {
                    PollResult { stop_waiting: false, remove_zombie: None }
                }
            }
            Some(WaitingState::WaitingChild {child_type, status, options, output_pid }) => {
                assert!(options == 0);//TODO options
                //TODO what if I wait for myself
                for proc in others {
                    if proc.identity.ppid != self.identity.pid {continue;}//only find children
                    if !child_type.is_valid(self.identity.pid, proc.identity.pid, proc.identity.pgrp) {continue;}//only find acceptable children
                    if let Some(WaitingState::AmZombie { exit_code }) = proc.waiting_state {
                        assert!(!output_pid.is_null());
                        unsafe {*output_pid = exit_code.into();}
                        if !status.is_null() {
                            unsafe {*status = exit_code as i32 | WIFEXITED_MASK}
                            return PollResult { stop_waiting: true, remove_zombie: Some(proc.identity.pid) };
                        }

                    }
                }
                PollResult { stop_waiting: false, remove_zombie: None }
            }
            Some(WaitingState::AmZombie { .. }) => {PollResult { stop_waiting: false, remove_zombie: None }}
        }
    }
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