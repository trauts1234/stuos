use core::ptr::null;
use crate::{memory::set_pml4_phys, pipes_and_files::FileOperations, processes::{LoadedProgram, PollResult, Process, ProcessorState, WaitingState}, rs_uapi::{types::Pid}};
use alloc::collections::VecDeque;
use foldhash::fast::FixedState;
use hashbrown::HashMap;
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
    processes: HashMap<Pid,Process, FixedState>,
    order: VecDeque<Pid>,
    next_free_pid: Pid
}
impl ProcessQueue {
    const fn new() -> Self {
        Self {
            processes: HashMap::with_hasher(FixedState::with_seed(69)),
            order: VecDeque::new(),
            next_free_pid: 1
        }
    }

    pub fn current_pid(&self) -> Pid {
        self.order[0]
    }
    pub fn current(&self) -> &Process {
        self.processes.get(&self.order[0]).unwrap()
    }
    pub fn current_mut(&mut self) -> &mut Process {
        self.processes.get_mut(&self.order[0]).unwrap()
    }

    pub fn push(&mut self, program: LoadedProgram) -> Pid {

        let new_proc = match self.order.front() {
            Some(parent_pid) => Process::create_with_parent(program, self.processes.get(parent_pid).unwrap(), *parent_pid),
            None => Process::create(program)
        };

        let new_pid = self.allocate_pid();
        assert!(self.processes.insert(new_pid, new_proc).is_none());
        self.order.push_back(new_pid);
        new_pid
    }

    fn allocate_pid(&mut self) -> Pid {
        let res = self.next_free_pid;
        self.next_free_pid += 1;
        res
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
        unsafe{q.current_mut().curr_thread_mut().paused_state = *interrupted_processor_state;}
    }

    loop {
        //move the previous process to the back
        let prev = q.order.pop_front().unwrap();
        q.order.push_back(prev);

        let curr = q.current();
        let curr_pid = q.current_pid();
        unsafe {set_pml4_phys(curr.page_table_root)};

        let children = q.processes.iter()
            .filter(|(_, proc)| proc.identity.ppid == curr_pid)
            .map(|(&pid, proc)| (pid, proc));
        let result = curr.poll(children);

        match result {
            PollResult::DoNothing => {},
            PollResult::StartUserland(processor_state) => unsafe {
                drop(q);
                start_userland(&processor_state);
            },
            PollResult::HandleWaiting { remove_zombie } => {
                q.current_mut().curr_thread_mut().waiting_state = None;
                if let Some(pid) = remove_zombie {
                    q.processes.remove(&pid).unwrap();

                    let (index, _) = q.order.iter().enumerate().find(|(_,x)| **x == pid).unwrap();
                    q.order.swap_remove_back(index).unwrap();
                }
            },
        }
        
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn set_current_as_zombie(exit_code: u8) {
    queue().current_mut().curr_thread_mut().waiting_state = Some(WaitingState::AmZombie { exit_code })
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
pub extern "C" fn replace_current_process(program: LoadedProgram) {
    let mut q = queue();
    q.current_mut().replace(program);
}