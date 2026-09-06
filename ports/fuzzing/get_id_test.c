#include "get_id_test.h"
#include "assert.h"
#include <unistd.h>

void get_id_test() {
    assert(getuid() == 0);
    assert(getgid() == 0);
    assert(geteuid() == 0);
    assert(getegid() == 0);
    assert(getpgrp() == 1);
    // assert(getpid() == 1);

    setgid(1000);

    assert(getuid() == 0);
    assert(getgid() == 1000);
    assert(geteuid() == 0);
    assert(getegid() == 1000);
    assert(getpgrp() == 1);
    // assert(getpid() == 1);
}