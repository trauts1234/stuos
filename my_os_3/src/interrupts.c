
#include "interrupts.h"
#include "debugging.h"
#include "io.h"
#include "kern_libc.h"
#include "ps2.h"
#include "tty.h"
#include "uapi/stdint.h"

struct InterruptDescriptor
{
    uint16_t address_low;
    uint16_t selector; // a code segment selector in GDT or LDT
    uint8_t ist; // bits 0..2 holds Interrupt Stack Table offset, rest of bits zero.
    uint8_t flags; // gate type, dpl, and p fields
    uint16_t address_mid;
    uint32_t address_high;
    uint32_t reserved; //zero
} __attribute__((packed));

static const uint16_t PIC1_COMMAND = 0x20;
static const uint16_t PIC1_DATA = 0x21;
static const uint16_t PIC2_COMMAND = 0xA0;
static const uint16_t PIC2_DATA = 0xA1;

static struct InterruptDescriptor interrupt_descriptor_table[256];
static struct IdtPtr { uint16_t limit; void* base;} __attribute__((packed)) idt_table_ptr = {.limit = sizeof(interrupt_descriptor_table) - 1, .base = &interrupt_descriptor_table};

static uint8_t interrupt_stack[4096 * 4] __attribute__ ((__aligned__(16)));
uint8_t *const interrupt_stack_top = interrupt_stack + sizeof(interrupt_stack);

const uint32_t FREQ = 100;//100hz
extern uint64_t total_timer_interrupts;

extern void apply_idt(struct IdtPtr* idt_base);
extern void vector_14_handler();
/// interrupt handler for interrupt 32 - PIT timer
extern void vector_32_handler();
/// Interrupt handler for interrupt 33 - keyboard interrupt
extern void vector_33_handler();

uint64_t get_uptime_ms() {
    return total_timer_interrupts * 1000 / FREQ;
}

static void generate_idt_entry(int vec, void *handler) {
    uint64_t h = (uint64_t) handler;
    interrupt_descriptor_table[vec].address_low  = h & 0xFFFF;
    interrupt_descriptor_table[vec].selector    = 0x08;
    interrupt_descriptor_table[vec].ist         = 0 & 0x7; //should be zero? "interrupt stack table" - should be an index into something in TSS
    interrupt_descriptor_table[vec].flags   = 0x8E;//present, interrupt
    interrupt_descriptor_table[vec].address_mid  = (h >> 16) & 0xFFFF;
    interrupt_descriptor_table[vec].address_high = (h >> 32) & 0xFFFFFFFF;
    interrupt_descriptor_table[vec].reserved        = 0;
}

static void setup_pic() {
    const uint8_t PIC1_OFFSET = 32;
    const uint8_t PIC2_OFFSET = PIC1_OFFSET + 8;
    const uint8_t ICW1_ICW4 = 0x01;		/* Indicates that ICW4 will be present */
    const uint8_t ICW1_INIT = 0x10;		/* Initialization - required! */
    const uint8_t ICW4_8086 = 0x01;		/* 8086/88 (MCS-80/85) mode */
    const uint8_t CASCADE_IRQ = 2;

    // starts the initialization sequence (in cascade mode)
    out_byte(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
	io_wait();
	out_byte(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
	io_wait();

    //add an offset to all the PIC interrupt numbers, to make room for error interrupts (0 to 32)
	out_byte(PIC1_DATA, PIC1_OFFSET);
	io_wait();
	out_byte(PIC2_DATA, PIC2_OFFSET);
	io_wait();
	out_byte(PIC1_DATA, 1 << CASCADE_IRQ);        // ICW3: tell Master PIC that there is a slave PIC at IRQ2
	io_wait();
	out_byte(PIC2_DATA, 2);                       // ICW3: tell Slave PIC its cascade identity (0000 0010)
	io_wait();
	
	out_byte(PIC1_DATA, ICW4_8086);               // ICW4: have the PICs use 8086 mode (and not 8080 mode)
	io_wait();
	out_byte(PIC2_DATA, ICW4_8086);
	io_wait();

	// Unmask both PICs.
	out_byte(PIC1_DATA, 0);
	out_byte(PIC2_DATA, 0);
}

static void setup_pit() {
    const uint32_t PIT_BASE = 1'193'182;
    const uint16_t divisor = (uint16_t)(PIT_BASE / FREQ);

    const uint16_t PIT_CMD = 0x43;
    const uint16_t PIT_CHANNEL0 = 0x40;
    out_byte(PIT_CMD, 0x36); /* channel 0, access: lobyte/hibyte, mode 3 (square wave) */
    io_wait();
    out_byte(PIT_CHANNEL0, divisor & 0xFF);       /* low byte */
    out_byte(PIT_CHANNEL0, (divisor >> 8) & 0xFF);/* high byte */
}

static void setup_idt() {
    memset(interrupt_descriptor_table, 0, sizeof(interrupt_descriptor_table));
    generate_idt_entry(32, vector_32_handler);
    generate_idt_entry(33, vector_33_handler);
    generate_idt_entry(14, vector_14_handler);
    apply_idt(&idt_table_ptr);
}

void setup_pic_pit_idt() {
    setup_pic();
    setup_pit();
    setup_idt();
}

void acknowledge_interrupt(uint8_t irq) {
    const uint8_t PIC_EOI = 0x20;//end of interrupt command
    if(irq >= 8) {
        //IRQ came from second chip
        out_byte(PIC2_COMMAND, PIC_EOI);
    }
    //need to send to master PIC either way
    out_byte(PIC1_COMMAND, PIC_EOI);
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