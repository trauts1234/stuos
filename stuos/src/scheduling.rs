use core::ptr::null;
use crate::{pipes_and_files::FileOperations, processes::{LimitData, LoadedProgram, Process, ProcessorState, WaitingState}, rs_uapi::{resource::{RLIMIT_DATA, RLimit}, types::Pid}};
use alloc::{boxed::Box, collections::VecDeque};
use spin::Mutex;

//the front element is the currently running process
static PROCESSES_QUEUE: Mutex<ProcessQueue> = Mutex::new(ProcessQueue::new());

macro_rules! queue {
    () => {
        PROCESSES_QUEUE.lock() 
    };
}
macro_rules! currproc {
    () => {
        PROCESSES_QUEUE.lock().processes[0] 
    };
}

struct ProcessQueue {
    processes: VecDeque<Box<Process>>,
}
impl ProcessQueue {
    const fn new() -> Self {
        Self{ processes: VecDeque::new() }
    }

    fn push(&mut self, program: LoadedProgram) -> Pid {

        let new_proc = match self.processes.front() {
            Some(parent) => Process::create_with_parent(program,parent),
            None => Process::create(program)
        };
        let new_pid = new_proc.identity.pid;
        self.processes.push_back(Box::new(new_proc));
        new_pid
    }

    fn run_next_task(&mut self, interrupted_processor_state: *const ProcessorState) -> ! {
        if let Some(curr) = self.processes.front_mut() {
            assert!(!interrupted_processor_state.is_null());
            unsafe{curr.paused_state = *interrupted_processor_state;}
        } else {
            assert!(interrupted_processor_state.is_null())
        }

        loop {
            //move the previous process to the back
            let prev = self.processes.pop_front().unwrap();
            self.processes.push_back(prev);

            let curr = &self.processes[0];
            let result = curr.poll(self.processes.iter().map(|b| b.as_ref()));
            if result.stop_waiting {
                self.processes[0].waiting_state = None;
            }
            if let Some(pid) = result.remove_zombie {
                let (index, _) = self.processes.iter().enumerate().find(|(_,x)| x.identity.pid == pid).unwrap();
                self.processes.swap_remove_back(index);
            }
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn add_new_process(program: LoadedProgram) -> Pid {
    queue!().push(program)
}

/// Called by an assembly interrupt handler
/// If you have just killed the current process, you can pass NULL here
#[unsafe(no_mangle)]
pub extern "C" fn run_next_task(interrupted_processor_state: *const ProcessorState) -> ! {
    queue!().run_next_task(interrupted_processor_state);
}

#[unsafe(no_mangle)]
pub extern "C" fn set_current_as_zombie(exit_code: u8) {
    currproc!().waiting_state = Some(WaitingState::AmZombie { exit_code })
}
#[unsafe(no_mangle)]
pub extern "C" fn get_current_heap_start() -> *mut () {
    currproc!().heap_start
}
#[unsafe(no_mangle)]
pub extern "C" fn get_file_descriptor(fd_number: i32) -> *const FileOperations {
    //I point into a mutex that then gets unlocked?! danger!!!
    currproc!().file_descriptors[fd_number as usize].as_ref().map(|x| &raw const *x.as_ref()).unwrap_or(null())
}
#[unsafe(no_mangle)]
pub extern "C" fn get_cwd() -> *const i8 {
    //I point into a mutex that then gets unlocked?! danger!!!
    currproc!().cwd.as_ptr()
}

#[unsafe(no_mangle)]
pub extern "C" fn get_rlimit(resource: i32) -> LimitData {
    match resource {
        RLIMIT_DATA => currproc!().memory_limit,
        x => panic!("invalid resource {}", x)
    }
}
#[unsafe(no_mangle)]
pub extern "C" fn set_rlimit(resource: i32, new_limit: RLimit) {
    let limit = match resource {
        RLIMIT_DATA => &mut currproc!().memory_limit.limit,
        x => panic!("invalid resource {}", x)
    };

    assert!(new_limit.rlim_max <= limit.rlim_max);
    limit.rlim_max = new_limit.rlim_max;

    assert!(new_limit.rlim_cur <= limit.rlim_max);
    limit.rlim_cur = new_limit.rlim_cur;
}