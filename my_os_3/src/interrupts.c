
#include "interrupts.h"
#include "debugging.h"
#include "io.h"
#include "kern_libc.h"
#include <uapi/stdint.h>

static const uint16_t PIC1_COMMAND = 0x20;
static const uint16_t PIC1_DATA = 0x21;
static const uint16_t PIC2_COMMAND = 0xA0;
static const uint16_t PIC2_DATA = 0xA1;

static uint8_t interrupt_stack[4096 * 4] __attribute__ ((__aligned__(16)));
uint8_t *const interrupt_stack_top = interrupt_stack + sizeof(interrupt_stack);

const uint32_t FREQ = 100;//100hz
extern uint64_t total_timer_interrupts;

uint64_t get_uptime_ms() {
    return total_timer_interrupts * 1000 / FREQ;
}

static void setup_pic() {
    const uint8_t PIC1_OFFSET = 32;
    const uint8_t PIC2_OFFSET = PIC1_OFFSET + 8;
    const uint8_t ICW1_ICW4 = 0x01;		/* Indicates that ICW4 will be present */
    const uint8_t ICW1_INIT = 0x10;		/* Initialization - required! */
    const uint8_t ICW4_8086 = 0x01;		/* 8086/88 (MCS-80/85) mode */
    const uint8_t CASCADE_IRQ = 2;

    // starts the initialization sequence (in cascade mode)
    out8(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
	out8(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);

    //add an offset to all the PIC interrupt numbers, to make room for error interrupts (0 to 32)
	out8(PIC1_DATA, PIC1_OFFSET);
	out8(PIC2_DATA, PIC2_OFFSET);
	out8(PIC1_DATA, 1 << CASCADE_IRQ);        // ICW3: tell Master PIC that there is a slave PIC at IRQ2
	out8(PIC2_DATA, 2);                       // ICW3: tell Slave PIC its cascade identity (0000 0010)
	
	out8(PIC1_DATA, ICW4_8086);               // ICW4: have the PICs use 8086 mode (and not 8080 mode)
	out8(PIC2_DATA, ICW4_8086);

	// Unmask both PICs.
	out8(PIC1_DATA, 0);
	out8(PIC2_DATA, 0);
}

static void setup_pit() {
    const uint32_t PIT_BASE = 1193182;
    const uint16_t divisor = (uint16_t)(PIT_BASE / FREQ);

    const uint16_t PIT_CMD = 0x43;
    const uint16_t PIT_CHANNEL0 = 0x40;
    out8(PIT_CMD, 0x36); /* channel 0, access: lobyte/hibyte, mode 3 (square wave) */
    out8(PIT_CHANNEL0, divisor & 0xFF);       /* low byte */
    out8(PIT_CHANNEL0, (divisor >> 8) & 0xFF);/* high byte */
}

void setup_pic_pit() {
    setup_pic();
    setup_pit();
}

void acknowledge_interrupt(uint8_t irq) {
    const uint8_t PIC_EOI = 0x20;//end of interrupt command
    if(irq >= 8) {
        //IRQ came from second chip
        out8(PIC2_COMMAND, PIC_EOI);
    }
    //need to send to master PIC either way
    out8(PIC1_COMMAND, PIC_EOI);
}

/// Handles interrupt 14
__attribute__((noreturn))
void memory_exception_handle(void* bad_address, void* rip) {
    debug_print("failed to access address: ");
    debug_hex((uint64_t)bad_address);
    debug_print("\nwith RIP = ");
    debug_hex((uint64_t)rip);
    debug_print("\n");
    HCF
}