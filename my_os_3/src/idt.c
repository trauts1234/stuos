#include <uapi/stdint.h>
#include "debugging.h"
#include "kern_libc.h"
#include "ps2.h"

#define N_INTERRUPTS 256

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

struct IdtPtr {
    uint16_t limit;
    void* base;
} __attribute__((packed));

static struct InterruptDescriptor interrupt_descriptor_table[N_INTERRUPTS];
static struct IdtPtr idt_table_ptr = {.limit = sizeof(interrupt_descriptor_table) - 1, .base = &interrupt_descriptor_table};

const extern void *vector_n_handlers[N_INTERRUPTS];
extern void apply_idt(struct IdtPtr* idt_base);

//first 33 entries are reserved since they aren't called
void (*general_purpose_interrupt_handlers[N_INTERRUPTS])(int) = {};

static void unused_general_purpose_slot(int x) {
    printf("interrupt %d was called, but it wasn't allocated or initialised\n", x);
    HCF
}
static void allocated_general_purpose_slot(int x) {
    printf("the allocated interrupt %d was called, but it wasn't initialised\n", x);
    HCF
}

static void set_idt_entry(int vec, const void *handler) {
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
    // memset(interrupt_descriptor_table, 0, sizeof(interrupt_descriptor_table));
    for(int i=0; i<256; i++) {
        set_idt_entry(i, vector_n_handlers[i]);
    }
    apply_idt(&idt_table_ptr);

    for(int i=33; i<N_INTERRUPTS; i++) {
        general_purpose_interrupt_handlers[i] = unused_general_purpose_slot;
    }
    general_purpose_interrupt_handlers[33] = handle_incoming_byte;//TODO make this set itself up like PCI MSI does
}

int allocate_free_idt_entry() {
    for(int i=33; i<256; i++) {
        assert(general_purpose_interrupt_handlers[i] != 0);
        if(general_purpose_interrupt_handlers[i] == unused_general_purpose_slot) {
            general_purpose_interrupt_handlers[i] = allocated_general_purpose_slot;
            return i;
        }
    }
    HCF
}
void initialise_idt_entry(int free_idt_entry, void (*handler)(int)) {
    assert(free_idt_entry > 33);
    assert(free_idt_entry < 256);
    assert(general_purpose_interrupt_handlers[free_idt_entry] == allocated_general_purpose_slot);//I may be overwriting an initialised slot or an unallocated slot
    general_purpose_interrupt_handlers[free_idt_entry] = handler;
}