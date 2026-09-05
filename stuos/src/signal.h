#ifndef SIGNALS_H
#define SIGNALS_H

#include <uapi/stdint.h>
#include <uapi/stdbool.h>

#define NUM_SIGNALS 32

enum SignalDefaultAction {
    DF_TERM, DF_DUMP, DF_CONTINUE, DF_STOP, DF_NOTHING
};

enum SignalState {
    STATE_UNSET=0, STATE_SET, STATE_RUNNING
};

extern const enum SignalDefaultAction default_actions[NUM_SIGNALS];

#endif