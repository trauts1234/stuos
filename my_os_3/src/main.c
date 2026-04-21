#include "uapi/stdint.h"
#include <stddef.h>
#include <stdbool.h>
#include "display.h"
#include "fs.h"
#include "fs_ram.h"
#include "interrupts.h"
#include "limine.h"
#include "ps2.h"
#include "fs_tar.h"
#include "memory.h"
#include "elf.h"
#include "debugging.h"
#include "scheduling.h"
#include "tty.h"

//assembly functions
extern void loop_hlt();
extern bool check_cpuid_sse();
extern void enable_sse();
extern void syscall_init();

// Set the base revision to 3, this is recommended as this is the latest
// base revision described by the Limine boot protocol specification.
// See specification for further info.

__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(3);

// The Limine requests can be placed anywhere, but it is important that
// the compiler does not optimise them away, so, usually, they should
// be made volatile or equivalent, _and_ they should be accessed at least
// once or marked as used with the "used" attribute as done here.

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = LIMINE_API_REVISION
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_module_request filesystem_request = {
    .id = LIMINE_MODULE_REQUEST,
    .revision = LIMINE_API_REVISION,
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
     || framebuffer_request.response->framebuffer_count < 1
     || filesystem_request.response == NULL
     || memmap_request.response == NULL
     || hhdm_request.response == NULL) {
        HCF
    }

    // Fetch the first framebuffer.
    volatile struct limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[0];
    volatile struct limine_file *file_response = filesystem_request.response->modules[0];
    volatile struct limine_memmap_response *memmap_response = memmap_request.response;
    uint64_t hhdm_offset = hhdm_request.response->offset;
    
    if(check_cpuid_sse()) {
        enable_sse();
    } else {
        //how did we get here? what sort of weird processor is this running on?
        loop_hlt();
    }

    debugging_init();
    display_init(framebuffer);
    initialise_ps2();
    memory_init(memmap_response, hhdm_offset);
    setup_gdt_tss();
    setup_pic_pit_idt();
    ramfs_init();
    tarfs_init(file_response->address);
    syscall_init();
    initialise_tty();
    DEBUG_HERE

    struct VNode fuzz = vfs_get("/", "/tarfs/testing.out", 0);
    const struct LoadedProgram elf = instantiate_ELF(fuzz, (char*[]){"testing.out", "helloworld", NULL});

    add_new_process(elf);
    run_next_task(NULL);
}