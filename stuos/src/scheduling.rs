use core::{fmt::Debug, ptr::null};
use crate::{memory::set_pml4_phys, pipes_and_files::FileOperations, println, processes::{LimitData, LoadedProgram, PollResult, Process, ProcessorState, WaitingState}, rs_uapi::{resource::{RLIMIT_DATA, RLimit}, types::Pid}};
use alloc::{boxed::Box, collections::VecDeque};
use spin::{Mutex, MutexGuard};

unsafe extern "C" {
    fn start_userland(processor_state: *const ProcessorState) -> !;
}

//the front element is the currently running process
static PROCESSES_QUEUE: Mutex<ProcessQueue> = Mutex::new(ProcessQueue::new());

pub fn queue<'a>() -> MutexGuard<'a, ProcessQueue>{
    PROCESSES_QUEUE.try_lock().unwrap()
}

pub struct ProcessQueue {
    processes: VecDeque<Box<Process>>,
}
impl ProcessQueue {
    const fn new() -> Self {
        Self{ processes: VecDeque::new() }
    }

    pub fn current(&self) -> &Process {
        &self.processes[0]
    }
    pub fn current_mut(&mut self) -> &mut Process {
        &mut self.processes[0]
    }

    pub fn push(&mut self, program: LoadedProgram) -> Pid {

        let new_proc = match self.processes.front() {
            Some(parent) => Process::create_with_parent(program, parent),
            None => Process::create(program)
        };
        let new_pid = new_proc.identity.pid;
        self.processes.push_back(Box::new(new_proc));
        new_pid
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn add_new_process(program: LoadedProgram) -> Pid {
    queue().push(program)
}

/// Called by an assembly interrupt handler
/// If you have just killed the current process, you can pass NULL here
#[unsafe(no_mangle)]
pub extern "C" fn run_next_task(interrupted_processor_state: *const ProcessorState) -> ! {
    let mut q = queue();

    if !interrupted_processor_state.is_null() {
        unsafe{q.current_mut().paused_state = *interrupted_processor_state;}
    }

    loop {
        //move the previous process to the back
        let prev = q.processes.pop_front().unwrap();
        q.processes.push_back(prev);

        let curr = q.current();
        unsafe {set_pml4_phys(curr.page_table_root)};
        let result = curr.poll(q.processes.iter().map(|b| b.as_ref()));

        match result {
            PollResult::DoNothing => {},
            PollResult::StartUserland(processor_state) => unsafe {
                drop(q);
                start_userland(&processor_state);
            },
            PollResult::HandleWaiting { remove_zombie } => {
                q.current_mut().waiting_state = None;
                if let Some(pid) = remove_zombie {
                    let (index, _) = q.processes.iter().enumerate().find(|(_,x)| x.identity.pid == pid).unwrap();
                    q.processes.swap_remove_back(index).unwrap();
                }
            },
        }
        
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn set_current_as_zombie(exit_code: u8) {
    queue().current_mut().waiting_state = Some(WaitingState::AmZombie { exit_code })
}
#[unsafe(no_mangle)]
pub extern "C" fn get_current_heap_start() -> *mut () {
    queue().current().heap_start
}
#[unsafe(no_mangle)]
pub extern "C" fn get_file_descriptor(fd_number: i32) -> *const FileOperations {
    //I point into a mutex that then gets unlocked?! danger!!!
    queue().current().file_descriptors[fd_number as usize].as_ref().map(|x| &raw const *x.as_ref()).unwrap_or(null())
}
#[unsafe(no_mangle)]
pub extern "C" fn get_cwd() -> *const i8 {
    //I point into a mutex that then gets unlocked?! danger!!!
    queue().current().cwd.as_ptr()
}

#[unsafe(no_mangle)]
pub extern "C" fn get_rlimit(resource: i32) -> LimitData {
    match resource {
        RLIMIT_DATA => queue().current().memory_limit,
        x => panic!("invalid resource {}", x)
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn set_rlimit(resource: i32, new_limit: RLimit) {
    let mut q = queue();
    let limit = match resource {
        RLIMIT_DATA => &mut q.current_mut().memory_limit.limit,
        x => panic!("invalid resource {}", x)
    };

    assert!(new_limit.rlim_max <= limit.rlim_max);
    limit.rlim_max = new_limit.rlim_max;

    assert!(new_limit.rlim_cur <= limit.rlim_max);
    limit.rlim_cur = new_limit.rlim_cur;
}

#[unsafe(no_mangle)]
pub extern "C" fn replace_current_process(program: LoadedProgram) {
    let mut q = queue();
    let ready = Process::create_to_replace(program, *q.processes.pop_front().unwrap());
    q.processes.push_front(Box::new(ready));
}