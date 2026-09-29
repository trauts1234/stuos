#ifndef SCHEDULING_H
#define SCHEDULING_H

#include "signal.h"
#include <uapi/resource.h>
#include <uapi/signal.h>
#include <uapi/stdint.h>
#include <uapi/limits.h>
#include "processes.h"

// pass NULL if you don't want to save the processor state, i.e after killing the current process or when no process has been started yet
void run_next_task(const struct ProcessorState* const interrupted_processor_state) __attribute__((noreturn));

//TODO use
void schedule_process(int pid);


#endif