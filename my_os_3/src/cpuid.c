#include "cpuid.h"
#include "uapi/stdint.h"

struct CpuIdResult {
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
};

static struct CpuIdResult get_cpuid(unsigned int input_eax) {
    uint32_t eax = input_eax;
    uint32_t ebx, ecx, edx;

    // The __asm__ block:
    // "cpuid"        : The instruction to execute
    // "=a" (eax)     : Output: EAX is written back to our eax variable
    // "=b" (ebx)     : Output: EBX is written back to our ebx variable
    // "=c" (ecx)     : Output: ECX is written back to our ecx variable
    // "=d" (edx)     : Output: EDX is written back to our edx variable
    // "a" (eax)      : Input: Use the value in eax as input to the instruction
    // "cc"           : Clobber: Tells the compiler that the condition codes are modified
    __asm__ volatile (
        "cpuid"
        : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
        : "a" (eax)
        : "cc"
    );

    return (struct CpuIdResult) {
        .eax=eax,.ebx=ebx,.ecx=ecx,.edx=edx
    };
}

bool sse_supported() {
    const uint32_t sse_mask = 0x06000000;//all of these bits must be set
    uint32_t edx = get_cpuid(1).edx;
    return (edx & sse_mask) == sse_mask;
}