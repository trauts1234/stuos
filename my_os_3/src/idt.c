#include <uapi/stdint.h>
#include "kern_libc.h"

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

static struct InterruptDescriptor interrupt_descriptor_table[256];
static struct IdtPtr { uint16_t limit; void* base;} __attribute__((packed)) idt_table_ptr = {.limit = sizeof(interrupt_descriptor_table) - 1, .base = &interrupt_descriptor_table};

extern void vector_14_handler();
/// interrupt handler for interrupt 32 - PIT timer
extern void vector_32_handler();
/// Interrupt handler for interrupt 33 - keyboard interrupt
extern void vector_33_handler();

extern void apply_idt(struct IdtPtr* idt_base);

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

void setup_idt() {
    memset(interrupt_descriptor_table, 0, sizeof(interrupt_descriptor_table));
    generate_idt_entry(32, vector_32_handler);
    generate_idt_entry(33, vector_33_handler);
    generate_idt_entry(14, vector_14_handler);
    apply_idt(&idt_table_ptr);
}