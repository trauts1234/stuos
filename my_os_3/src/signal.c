#include "signal.h"

const enum SignalDefaultAction default_actions[NUM_SIGNALS] = {
    DF_NOTHING,//0
    DF_TERM, DF_TERM,//1..=2
    DF_DUMP, DF_DUMP, DF_DUMP, DF_DUMP, DF_DUMP, DF_DUMP,// 3..=8
    DF_TERM, DF_TERM,// 9..=10
    DF_DUMP,// 11
    DF_TERM, DF_TERM, DF_TERM, DF_TERM, DF_TERM,// 12..=16
    DF_NOTHING,// 17
    DF_CONTINUE,// 18
    DF_STOP, DF_STOP, DF_STOP, DF_STOP,// 19..=22
    DF_NOTHING,// 23
    DF_DUMP, DF_DUMP,// 24..=25
    DF_TERM, DF_TERM,// 26..=27
    DF_NOTHING,// 28
    DF_TERM, DF_TERM,// 29..=30
    DF_DUMP,// 31
};

//TODO