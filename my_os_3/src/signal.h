#ifndef SIGNALS_H
#define SIGNALS_H

#include "uapi/stdint.h"

#define NUM_SIGNALS 32

enum SignalDefaultAction {
    DF_TERM, DF_DUMP, DF_CONTINUE, DF_STOP, DF_NOTHING
};

enum SignalState {
    STATE_UNSET, STATE_SET, STATE_RUNNING
};

struct SignalData {
    enum SignalState state;
    /// Bit set if the signal should be ignored by the thread (1 is masked, 0 is unmasked)
    bool masked;
    /// userspace pointers to signal handler (process wide)
    void* custom_handler;
};

struct SignalsData {
    struct SignalData inner[NUM_SIGNALS];
};


#endif