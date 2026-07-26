#include "cpuid.h"
#include "kern_libc.h"
#include <uapi/stdint.h>
#include "display.h"
#include "fs.h"
#include "idt.h"
#include "limine.h"
#include "ps2.h"
#include "memory.h"
#include "elf.h"
#include "debugging.h"
#include "scheduling.h"
#include "tty.h"
#include "pci.h"
#include "fs_dev.h"
#include "fs_fat.h"
#include "apic.h"

//assembly functions
extern void loop_hlt();
extern void enable_sse();
extern void syscall_init();

// Set the base revision to 3
//this may be required since the rsdp response will then give a physical address?
__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(6);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = LIMINE_API_REVISION
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request =  {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = LIMINE_API_REVISION,
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = LIMINE_API_REVISION
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST,
    .revision = LIMINE_API_REVISION
};

// Finally, define the start and end markers for the Limine requests.

__attribute__((used, section(".limine_requests_start")))
static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER;

// The following will be our kernel's entry point.
void kmain(void) {
    // Ensure the bootloader actually understands our base revision (see spec).
    if (LIMINE_BASE_REVISION_SUPPORTED == false) {
        HCF
    }

    // Ensure we got a framebuffer and a filesystem
    if (framebuffer_request.response == NULL
     || framebuffer_request.response->framebuffer_count != 1
     || memmap_request.response == NULL
     || hhdm_request.response == NULL
     || rsdp_request.response == NULL) {
        HCF
    }

    // Fetch the first framebuffer.
    volatile struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];
    volatile struct limine_memmap_response *memmap_response = memmap_request.response;
    uint64_t hhdm_offset = hhdm_request.response->offset;
    
    if(sse_supported()) {
        enable_sse();
    } else {
        //how did we get here? what sort of weird processor is this running on?
        loop_hlt();
    }

    debugging_init();
    display_init(framebuffer);
    initialise_ps2();
    memory_init(memmap_response, hhdm_offset);
    initialise_tty();
    // for(char* c="hello world!";*c; c++) tty_write_char(*c);
    setup_idt();
    apic_init((uint64_t)rsdp_request.response);
    devfs_init();
    syscall_init();
    initialise_pci();
    __asm("sti");
    while(1) {
        __asm("nop");
    }
    mount_fat16(vfs_get("/", "/dev/blkAp1", 0), "fat");

    // struct VNode fuzz = vfs_get("/", "testing.out", 0);
    // const struct LoadedProgram elf = instantiate_ELF(fuzz, (char*[]){"testing.out", "helloworld", NULL});
    struct VNode fuzz = vfs_get("/", "stush", 0);
    const struct LoadedProgram elf = instantiate_ELF(fuzz, (char*[]){"stush", NULL});

    add_new_process(elf);
    run_next_task(NULL);
}