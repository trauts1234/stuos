#include "debugging.h"
#include "kern_libc.h"
#include "io.h"
#include "physical_slab_allocation.h"
#include "memory.h"
#include "tty.h"

struct LapicReg {uint32_t data; uint32_t reserved[3];};
struct PriorityReg {
    uint32_t sub_class: 4;
    uint32_t class: 4;
    uint32_t reserved_1: 24;
    uint32_t reserved_2[3];
};
struct LvtTimerReg {
    uint32_t vector_number: 8,
    reserved: 4,
    interrupt_status: 1,// 0 when interrupt has been accepted, R
    reserved_2: 3,
    interrupt_is_disabled: 1, //1 to disable
    operating_mode: 2,//0=one shot, 1=periodic, 2=deadline, 3=reserved
    reserved_3: 13,
    reserved_4[3];
};

static volatile struct {
    struct LapicReg 
        reserved_1[2],
        lapic_id, //RW
        lapic_version, //R
        reserved_2[4],
        task_priority, //RW
        arbitration_priority, //R
        processor_priority, //R
        end_of_interrupt, //W
        remote_read, //R
        logical_destination, //RW
        destination_format, //RW
        spurious_interrupt_vector, //RW
        in_service[8], //R
        trigger_mode[8], //R
        interrupt_request[8], //R
        error_status, //R
        reserved_3[6],
        lvt_corrected_machine_check_interrupt, //RW
        interrupt_command_register[2], //RW
        lvt_timer, //RW
        lvt_thermal_sensor, //RW
        lvt_performance_monitoring_counters, //RW
        lvt_lint0, lvt_lint1, //RW
        lvt_error, //RW
        initial_count, //RW
        current_count, //R
        reserved_4[4],
        divide_configuration, //RW
        reserved_5;
} *lapic_registers;

extern void enable_apic();

static void disable_legacy_pic() {
    static const uint16_t PIC1_COMMAND = 0x20;
    static const uint16_t PIC1_DATA = 0x21;
    static const uint16_t PIC2_COMMAND = 0xA0;
    static const uint16_t PIC2_DATA = 0xA1;
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

	// Mask all interrupts on both PICs.
	out8(PIC1_DATA, 0xFF);
	out8(PIC2_DATA, 0xFF);
}

void apic_init() {
    disable_legacy_pic();

    //memory map the local APIC
    lapic_registers = setup_mmio(0xFEE00000, PAGE_SIZE);
    enable_apic();
    //Set the Spurious Interrupt Vector Register bit 8 to start receiving interrupts
    lapic_registers->spurious_interrupt_vector.data |= 0x100;

    lapic_registers->initial_count.data = 1'000'000;
    lapic_registers->divide_configuration.data = 0;// /2

    lapic_registers->lvt_timer.data = 32 | (1 << 17);
}

void apic_eoi() {
    //end of interrupt
    tty_write_char('e');
    lapic_registers->end_of_interrupt.data = 0;
}

/// Handles interrupt 14
__attribute__((noreturn))
void memory_exception_handle(void* bad_address, void* rip) {
    printf("failed to access address: %p with RIP=%p\n", bad_address, rip);
    HCF
}