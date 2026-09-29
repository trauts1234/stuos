use crate::{processes::{PROCESSES, Pid, ProcessIdentity, ProcessorState, WaitingState}, rs_uapi::wait::WIFEXITED_MASK};
use alloc::collections::VecDeque;
use spin::Mutex;

unsafe extern "C" {
    fn start_userland(processor_state: *const ProcessorState) -> !;
}

//the front element is the currently running process
static PROCESSES_QUEUE: Mutex<ProcessQueue> = Mutex::new(ProcessQueue::new());

struct ProcessQueue {
    others: VecDeque<Pid>
}
impl ProcessQueue {
    const fn new() -> Self {
        Self{ others: VecDeque::new() }
    }

    fn get_current(&self) -> Option<Pid> {
        self.others.front().copied()
    }
    fn get_others(&self) -> &VecDeque<Pid> {
        &self.others
    }

    //panics if there are no processes
    fn skip_to_next_process(&mut self) -> Pid {
        if let Some(curr) = self.others.pop_front() {
            self.others.push_back(curr);
        }
        return self.others.front().copied().unwrap()
    }
    fn schedule_process(&mut self, pid: Pid) {
        self.others.push_back(pid);
    }
    //stops the current process from being scheduled, but it stays in the process list
    fn deschedule_current_process(&mut self) {
        self.others.pop_front().expect("tried to remove current process but there wasn't one");
    }

    fn run_next_task(&mut self, interrupted_processor_state: *const ProcessorState) -> ! {
        let mut procs = PROCESSES.lock();

        if let Some(curr_pid) = self.get_current() {
            assert!(!interrupted_processor_state.is_null());
            unsafe{procs[curr_pid].paused_state = *interrupted_processor_state;}
        } else {
            assert!(interrupted_processor_state.is_null())
        }

        loop {
            let curr_pid = self.skip_to_next_process();
            let curr = &mut procs[curr_pid];

            match curr.waiting_state {
                None => {
                    unsafe {start_userland(&curr.paused_state)}
                },
                Some(WaitingState::WaitingRead { fd_num, output_buf, num_bytes, output_num_bytes_ptr }) => {
                    let fop = curr.file_descriptors[fd_num].as_ref().unwrap();
                    let result = (fop.read_nonblocking)(fop.special_data, output_buf, num_bytes);
                    if result.read_something {
                        unsafe {*output_num_bytes_ptr = result.bytes_read}
                        curr.waiting_state = None;
                    }
                }
                Some(WaitingState::WaitingChild {child_type, status, options, output_pid }) => {
                    assert!(options == 0);//TODO options
                    //TODO what if I wait for myself
                    for &proc_pid in self.get_others() {
                        let ProcessIdentity{pgrp: proc_pgrp, ppid: proc_ppid, ..} = procs[proc_pid].identity;
                        
                        if proc_ppid != curr_pid {continue;}//only find children
                        if !child_type.is_valid(curr_pid, proc_pid, proc_pgrp) {continue;}//only find acceptable children
                        if let Some(WaitingState::AmZombie { exit_code }) = procs[proc_pid].waiting_state {
                            assert!(!output_pid.is_null());
                            unsafe {*output_pid = exit_code.into();}
                            if !status.is_null() {
                                unsafe {*status = exit_code as i32 | WIFEXITED_MASK}
                            }

                        }
                    }
                }
                Some(WaitingState::AmZombie { exit_code: _ }) => {
                    self.deschedule_current_process();
                }
            }
        }
    }
}

/// Called by an assembly interrupt handler
/// If you have just killed the current process, you can pass NULL here
#[unsafe(no_mangle)]
pub extern "C" fn run_next_task(interrupted_processor_state: *const ProcessorState) -> ! {
    let mut queue = PROCESSES_QUEUE.lock();
    queue.run_next_task(interrupted_processor_state);
}
#[unsafe(no_mangle)]
pub extern "C" fn schedule_process(pid: Pid) {
    let mut queue = PROCESSES_QUEUE.lock();
    queue.schedule_process(pid);
}