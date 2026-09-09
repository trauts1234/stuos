#include <stdlib.h>
#include <sys/resource.h>
#include <assert.h>

void rlim_test() {
    struct rlimit lim;
    getrlimit(RLIMIT_DATA, &lim);
    assert(lim.rlim_cur == RLIM_INFINITY);
    assert(lim.rlim_max == RLIM_INFINITY);

    lim.rlim_cur = 0;
    setrlimit(RLIMIT_DATA, &lim);
    getrlimit(RLIMIT_DATA, &lim);
    assert(lim.rlim_cur == 0);

    assert(malloc(4096 * 3) == NULL);

    lim.rlim_cur = RLIM_INFINITY;
    setrlimit(RLIMIT_DATA, &lim);
}