use crate::{processes::{LoadedProgram, Process, ProcessorState, WaitingState}, rs_uapi::types::Pid};
use alloc::{boxed::Box, collections::VecDeque};
use spin::Mutex;

//the front element is the currently running process
static PROCESSES_QUEUE: Mutex<ProcessQueue> = Mutex::new(ProcessQueue::new());

struct ProcessQueue {
    processes: VecDeque<Box<Process>>,
}
impl ProcessQueue {
    const fn new() -> Self {
        Self{ processes: VecDeque::new() }
    }

    fn get_current(&self) -> Option<&Process> {
        self.processes.front().map(|b| b.as_ref())
    }
    fn get_current_mut(&mut self) -> Option<&mut Process> {
        self.processes.front_mut().map(|b| b.as_mut())
    }

    fn push(&mut self, program: LoadedProgram) -> Pid {

        let new_proc = match self.get_current() {
            Some(parent) => Process::create_with_parent(program,parent),
            None => Process::create(program)
        };
        let new_pid = new_proc.identity.pid;
        self.processes.push_back(Box::new(new_proc));
        new_pid
    }

    fn run_next_task(&mut self, interrupted_processor_state: *const ProcessorState) -> ! {
        if let Some(curr) = self.get_current_mut() {
            assert!(!interrupted_processor_state.is_null());
            unsafe{curr.paused_state = *interrupted_processor_state;}
        } else {
            assert!(interrupted_processor_state.is_null())
        }

        loop {
            //move the previous process to the back
            let prev = self.processes.pop_front().unwrap();
            self.processes.push_back(prev);

            let curr = self.get_current().unwrap();
            let result = curr.poll(self.processes.iter().map(|b| b.as_ref()));
            if result.stop_waiting {
                self.get_current_mut().unwrap().waiting_state = None;
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
    let mut queue = PROCESSES_QUEUE.lock();
    queue.push(program)
}

/// Called by an assembly interrupt handler
/// If you have just killed the current process, you can pass NULL here
#[unsafe(no_mangle)]
pub extern "C" fn run_next_task(interrupted_processor_state: *const ProcessorState) -> ! {
    let mut queue = PROCESSES_QUEUE.lock();
    queue.run_next_task(interrupted_processor_state);
}

#[unsafe(no_mangle)]
pub extern "C" fn set_current_as_zombie(exit_code: u8) {
    let mut queue = PROCESSES_QUEUE.lock();
    queue.processes[0].waiting_state = Some(WaitingState::AmZombie { exit_code })
}