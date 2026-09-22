#include <alloca.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>

void alloca_test() {
    volatile int a = 69;
    volatile uint8_t *x = alloca(10);
    volatile int b = 67;
    memset((void*)x, 3, 10);
    assert(a == 69);
    assert(b == 67);
    assert(x[0] == 3);
    assert(x[9] == 3);
}