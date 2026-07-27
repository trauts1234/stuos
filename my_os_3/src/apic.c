#include "debugging.h"
#include "kern_libc.h"
#include "io.h"
#include "physical_slab_allocation.h"
#include "memory.h"

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

static bool matches_checksum(void* data, uint64_t len) {
    uint8_t sum = 0;
    for(uint64_t i=0; i<len; i++) {
        sum += ((uint8_t*)data)[i];
    }
    return sum == 0;
}

struct XSDP_t {
    char Signature[8];
    uint8_t Checksum;
    char OEMID[6];
    uint8_t Revision;
    uint32_t RsdtAddress;      // deprecated since version 2.0

    uint32_t Length;
    uint64_t XsdtAddress;
    uint8_t ExtendedChecksum;
    uint8_t reserved[3];
} __attribute__ ((packed));

struct SDT_header {
    char Signature[4];
    uint32_t Length;
    uint8_t Revision;
    uint8_t Checksum;
    char OEMID[6];
    char OEMTableID[8];
    uint32_t OEMRevision;
    uint32_t CreatorID;
    uint32_t CreatorRevision;
} __attribute__ ((packed));

struct RSDT {
  struct SDT_header header;
  uint32_t entries[];//length (header.Length - sizeof(header)) / 4
} __attribute__ ((packed));
struct XSDT {
  struct SDT_header header;
  uint64_t entries[];//length (header.Length - sizeof(header)) / 8
} __attribute__ ((packed));
struct MADT {
    struct SDT_header header;
    uint32_t local_apic_address;
    uint32_t flags;
    uint8_t entries[];//starts with [0] entry type, [1] record length, then a struct
} __attribute__ ((packed));
struct GenericAddressStructure
{
  uint8_t AddressSpace;
  uint8_t BitWidth;
  uint8_t BitOffset;
  uint8_t AccessSize;
  uint64_t Address;
} __attribute__ ((packed));
struct FADT
{
    struct   SDT_header h;
    uint32_t FirmwareCtrl;
    uint32_t Dsdt;

    // field used in ACPI 1.0; no longer in use, for compatibility only
    uint8_t  Reserved;

    uint8_t  PreferredPowerManagementProfile;
    uint16_t SCI_Interrupt;
    uint32_t SMI_CommandPort;
    uint8_t  AcpiEnable;
    uint8_t  AcpiDisable;
    uint8_t  S4BIOS_REQ;
    uint8_t  PSTATE_Control;
    uint32_t PM1aEventBlock;
    uint32_t PM1bEventBlock;
    uint32_t PM1aControlBlock;
    uint32_t PM1bControlBlock;
    uint32_t PM2ControlBlock;
    uint32_t PMTimerBlock;
    uint32_t GPE0Block;
    uint32_t GPE1Block;
    uint8_t  PM1EventLength;
    uint8_t  PM1ControlLength;
    uint8_t  PM2ControlLength;
    uint8_t  PMTimerLength;
    uint8_t  GPE0Length;
    uint8_t  GPE1Length;
    uint8_t  GPE1Base;
    uint8_t  CStateControl;
    uint16_t WorstC2Latency;
    uint16_t WorstC3Latency;
    uint16_t FlushSize;
    uint16_t FlushStride;
    uint8_t  DutyOffset;
    uint8_t  DutyWidth;
    uint8_t  DayAlarm;
    uint8_t  MonthAlarm;
    uint8_t  Century;

    // reserved in ACPI 1.0; used since ACPI 2.0+
    uint16_t BootArchitectureFlags;

    uint8_t  Reserved2;
    uint32_t Flags;

    // 12 byte structure; see below for details
    struct GenericAddressStructure ResetReg;

    uint8_t  ResetValue;
    uint8_t  Reserved3[3];
  
    // 64bit pointers - Available on ACPI 2.0+
    uint64_t                X_FirmwareControl;
    uint64_t                X_Dsdt;

    struct GenericAddressStructure X_PM1aEventBlock;
    struct GenericAddressStructure X_PM1bEventBlock;
    struct GenericAddressStructure X_PM1aControlBlock;
    struct GenericAddressStructure X_PM1bControlBlock;
    struct GenericAddressStructure X_PM2ControlBlock;
    struct GenericAddressStructure X_PMTimerBlock;
    struct GenericAddressStructure X_GPE0Block;
    struct GenericAddressStructure X_GPE1Block;
} __attribute__ ((packed));

union RedirectionEntry
{
    struct {
        uint64_t vector       : 8;
        uint64_t delvMode     : 3;
        uint64_t destMode     : 1;
        uint64_t delvStatus   : 1;
        uint64_t pinPolarity  : 1;
        uint64_t remoteIRR    : 1;
        uint64_t triggerMode  : 1;
        uint64_t mask         : 1;
        uint64_t reserved     : 39;
        uint64_t destination  : 8;
    };
    struct
    {
        uint32_t lower;
        uint32_t upper;
    };
};

static uint32_t io_red_tbl(uint32_t i) {return 0x10 + 2*i;}
struct IOAPICData {
    //register selector
    volatile uint32_t io_reg_sel;
    uint32_t reserved[3];
    //read/write this
    volatile uint32_t io_win;
};

static void handle_acpi_table(struct SDT_header* curr, uint8_t interrupt_destination_apic_id) {
    //TODO handle revision
    assert(matches_checksum(curr, curr->Length));

    if(memcmp(curr->Signature, "APIC", 4) == 0) {
        struct MADT *madt = (void*)curr;
        for(uint32_t i=0; 0x2C + i < madt->header.Length; i += madt->entries[i+1]) {
            switch (madt->entries[i]) {
                case 0://LAPIC
                case 2://IOAPIC interrupt source override
                case 3://IOAPIC non maskable interrupt source
                case 4://LAPIC non maskable interrupts
                case 5://LAPIC address override
                case 9://processor L x2APIC
                break;

                case 1://IOAPIC
                assert(madt->entries[i+1] == 12)
                printf("IOAPIC: id %d, address %u, interrupt base %u\n", madt->entries[i+2], *(uint32_t*)(madt->entries + i+4), *(uint32_t*)(madt->entries + i+8));

                struct IOAPICData *ioapic = setup_mmio(*(uint32_t*)(madt->entries + i+4), PAGE_SIZE);

                ioapic->io_reg_sel = 1;//IOAPICVER
                uint8_t max_redirection_entry = ioapic->io_win >> 16;
                printf("can handle %d irqs\n", max_redirection_entry);

                //write keyboard interrupt
                union RedirectionEntry ps2_keyboard_entry = {
                    .vector = 33,
                    .destination = interrupt_destination_apic_id
                };
                ioapic->io_reg_sel = io_red_tbl(1);
                ioapic->io_win = ps2_keyboard_entry.lower;
                ioapic->io_reg_sel = io_red_tbl(1) + 1;
                ioapic->io_win = ps2_keyboard_entry.upper;

                break;

                default:
                HCF
            }
        }
    } else if(memcmp(curr->Signature, "FACP", 4) == 0) {
        struct FADT *fadt = (void*)curr;
        printf("PM timer: %d\n", fadt->PMTimerLength);
    }
}

static void ioapic_init(struct XSDP_t *rsdp, uint8_t interrupt_destination_apic_id) {
    // phys_to_hhdm(rsdp_response_phys);
    //check signature
    assert(memcmp(rsdp->Signature, "RSD PTR ", 8) == 0)

    //check first checksum
    assert(matches_checksum(rsdp, 20))

    //check revision
    if(rsdp->Revision == 2) {
        //check second checksum
        assert(matches_checksum(rsdp, rsdp->Length))

        struct XSDT *xsdt = phys_to_hhdm(rsdp->XsdtAddress);
        assert(memcmp(xsdt->header.Signature, "XSDT", 4) == 0)
        assert(matches_checksum(xsdt, xsdt->header.Length))
        
        uint64_t entries = (xsdt->header.Length - sizeof(xsdt->header)) / 8;
        for(uint64_t i=0; i<entries; i++) {
            struct SDT_header *h = phys_to_hhdm(xsdt->entries[i]);
            handle_acpi_table(h, interrupt_destination_apic_id);
        }
    } else {
        struct RSDT *rsdt = phys_to_hhdm(rsdp->RsdtAddress);
        assert(memcmp(rsdt->header.Signature, "RSDT", 4) == 0)
        assert(matches_checksum(rsdt, rsdt->header.Length))

        uint32_t entries = (rsdt->header.Length - sizeof(rsdt->header)) / 4;
        for(uint32_t i=0; i<entries; i++) {
            struct SDT_header *h = phys_to_hhdm(rsdt->entries[i]);
            handle_acpi_table(h, interrupt_destination_apic_id);
        }
    }
}

void apic_init(void *rsdp_response) {
    disable_legacy_pic();

    //memory map the local APIC
    lapic_registers = setup_mmio(0xFEE00000, PAGE_SIZE);
    enable_apic();
    //Set the Spurious Interrupt Vector Register bit 8 to start receiving interrupts
    lapic_registers->spurious_interrupt_vector.data |= 0x100;

    lapic_registers->initial_count.data = 1'000'000;
    lapic_registers->divide_configuration.data = 0;// /2

    lapic_registers->lvt_timer.data = 32 | (1 << 17);

    ioapic_init(rsdp_response, lapic_registers->lapic_id.data);
}

void apic_eoi() {
    //end of interrupt
    lapic_registers->end_of_interrupt.data = 0;
}

/// Handles interrupt 14
__attribute__((noreturn))
void memory_exception_handle(void* bad_address, void* rip) {
    printf("failed to access address: %p with RIP=%p\n", bad_address, rip);
    HCF
}