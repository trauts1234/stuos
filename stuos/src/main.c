#include "cpuid.h"
#include "kern_libc.h"
#include <uapi/stdint.h>
#include "display.h"
#include "fs.h"
#include "idt.h"
#include "limine.h"
#include "limine_settings.h"
#include "memory.h"
#include "elf.h"
#include "debugging.h"
#include "ps2_driver.h"
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

extern int32_t givethree();

__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[3] = LIMINE_BASE_REVISION(LIMINE_API_REVISION);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_stack_size_request stack_size_request = {
    .id = LIMINE_STACK_SIZE_REQUEST_ID,
    .revision = LIMINE_API_REVISION,
    .stack_size = 1024*1024
};

// Finally, define the start and end markers for the Limine requests.

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_requests_start_marker[4] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_requests_end_marker[4] = LIMINE_REQUESTS_END_MARKER;

// The following will be our kernel's entry point.
void kmain(void) {
    // Ensure the bootloader actually understands our base revision
    if (LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == false) {
        loop_hlt();
    }
    if(!sse_supported()) {
        //how did we get here? what sort of weird processor is this running on?
        loop_hlt();
    }

    enable_sse();
    debugging_init();
    display_init();
    memory_init();
    initialise_tty();
    printf("stuos booting\n");
    assert(givethree() == 3);
    setup_idt();
    apic_init();
    devfs_init();
    syscall_init();
    initialise_pci();
    initialise_ps2();
    mount_fat16(vfs_get("/", "/dev/blkAp1", 0), "/");

    // struct VNode fuzz = vfs_get("/", "testing.out", 0);
    // const struct LoadedProgram elf = instantiate_ELF(fuzz, (char*[]){"testing.out", "helloworld", NULL});
    struct VNode fuzz = vfs_get("/", "stush", 0);
    const struct LoadedProgram elf = instantiate_ELF(fuzz, (char*[]){"stush", NULL});

    add_new_process(elf);
    run_next_task(NULL);
}