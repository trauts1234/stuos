#include "apic.h"
#include "debugging.h"
#include "kern_libc.h"
#include "io.h"
#include "physical_slab_allocation.h"
#include "memory.h"
#include "xhci_driver.h"
#include <uapi/stddef.h>

struct LapicReg {uint32_t data; uint32_t reserved[3];};

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

static uint32_t pm_timer_port = 0;//if 0, then use mmio
static volatile uint32_t *pm_timer_mmio = NULL;

extern void enable_apic();

static bool matches_checksum(void* data, uint64_t len) {
    uint8_t sum = 0;
    for(uint64_t i=0; i<len; i++) {
        sum += ((uint8_t*)data)[i];
    }
    return sum == 0;
}

struct RSDP {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;      // deprecated since version 2.0

    uint32_t length;
    uint64_t xsdt_address;
    uint8_t ext_checksum;
    uint8_t reserved[3];
} __attribute__ ((packed));

struct SDTHeader {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__ ((packed));

struct RSDT {
  struct SDTHeader header;
  uint32_t entries[];//length (header.Length - sizeof(header)) / 4
} __attribute__ ((packed));
struct XSDT {
  struct SDTHeader header;
  uint64_t entries[];//length (header.Length - sizeof(header)) / 8
} __attribute__ ((packed));
struct MADT {
    struct SDTHeader header;
    uint32_t local_apic_address;
    uint32_t flags;
    uint8_t entries[];//starts with [0] entry type, [1] record length, then a struct
} __attribute__ ((packed));
struct GenericAddressStructure
{
  uint8_t address_space;
  uint8_t bit_width;
  uint8_t bit_offset;
  uint8_t access_size;
  uint64_t address;
} __attribute__ ((packed));
struct FADT
{
    struct   SDTHeader h;
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

struct MessageData {

};

union MessageAddress {
    
};

static uint32_t io_red_tbl(uint32_t i) {return 0x10 + 2*i;}
struct IOAPICData {
    //register selector
    volatile uint32_t io_reg_sel;
    uint32_t reserved[3];
    //read/write this
    volatile uint32_t io_win;
};

struct IOAPIC {
    volatile struct IOAPICData *access;//NULL for empty entry
    uint8_t interrupt_destination_apic_id;
    uint8_t max_redirection_entry;
} ioapic = {};

static void detect_pm_timer(struct FADT *fadt) {
    assert(fadt->PMTimerLength == 4);
    if(false) {
        //Xtended (disabled because I don't know when to use this)
        switch(fadt->X_PMTimerBlock.address_space) {
            case 0:
            //mmio
            pm_timer_mmio = setup_mmio(fadt->X_PMTimerBlock.address, PAGE_SIZE);
            break;
            case 1:
            //port
            pm_timer_port = fadt->X_PMTimerBlock.address;
            break;
            default:
            HCF
        }
    } else {
        pm_timer_port = fadt->PMTimerBlock;
    }
}
static void setup_ioapic(uint32_t ioapic_phys_addr, uint8_t interrupt_destination_apic_id) {
    assert(ioapic.access == NULL);
    ioapic.access = setup_mmio(ioapic_phys_addr, PAGE_SIZE);

    ioapic.access->io_reg_sel = 1;//IOAPICVER
    ioapic.max_redirection_entry = ioapic.access->io_win >> 16; //max irqs
}
//maps irq to vector
static void map_ioapic_interrupt(uint8_t irq, uint8_t vector) {
    assert(irq < ioapic.max_redirection_entry);
    union RedirectionEntry entry = {
        .vector = 32 + irq,
        .destination = ioapic.interrupt_destination_apic_id,
        .mask = 0,
    };

    ioapic.access->io_reg_sel = io_red_tbl(irq);
    ioapic.access->io_win = entry.lower;

    ioapic.access->io_reg_sel = io_red_tbl(irq) + 1;
    ioapic.access->io_win = entry.upper;
}

static void handle_sdt(struct SDTHeader* curr, uint8_t interrupt_destination_apic_id) {
    //TODO handle revision
    assert(matches_checksum(curr, curr->length));

    if(memcmp(curr->signature, "APIC", 4) == 0) {
        struct MADT *madt = (void*)curr;
        for(uint32_t i=0; 0x2C + i < madt->header.length; i += madt->entries[i+1]) {
            switch (madt->entries[i]) {
                case 0://LAPIC
                case 2://IOAPIC interrupt source override
                case 3://IOAPIC non maskable interrupt source
                case 4://LAPIC non maskable interrupts
                case 5://LAPIC address override
                case 9://processor L x2APIC
                break;

                case 1://IOAPIC
                assert(madt->entries[i+1] == 12);
                setup_ioapic(*(uint32_t*)(madt->entries + i+4), interrupt_destination_apic_id);
                break;

                default:
                HCF
            }
        }
    } else if(memcmp(curr->signature, "FACP", 4) == 0) {
        detect_pm_timer((void*)curr);
    }
}

static void handle_rsdp(struct RSDP *rsdp, uint8_t interrupt_destination_apic_id) {
    //check signature
    assert(memcmp(rsdp->signature, "RSD PTR ", 8) == 0)

    //check first checksum
    assert(matches_checksum(rsdp, 20))

    //check revision
    if(rsdp->revision == 2) {
        //check second checksum
        assert(matches_checksum(rsdp, rsdp->length))

        struct XSDT *xsdt = phys_to_hhdm(rsdp->xsdt_address);
        assert(memcmp(xsdt->header.signature, "XSDT", 4) == 0)
        assert(matches_checksum(xsdt, xsdt->header.length))
        
        uint64_t entries = (xsdt->header.length - sizeof(xsdt->header)) / 8;
        for(uint64_t i=0; i<entries; i++) {
            struct SDTHeader *h = phys_to_hhdm(xsdt->entries[i]);
            handle_sdt(h, interrupt_destination_apic_id);
        }
    } else {
        struct RSDT *rsdt = phys_to_hhdm(rsdp->rsdt_address);
        assert(memcmp(rsdt->header.signature, "RSDT", 4) == 0)
        assert(matches_checksum(rsdt, rsdt->header.length))

        uint32_t entries = (rsdt->header.length - sizeof(rsdt->header)) / 4;
        for(uint32_t i=0; i<entries; i++) {
            struct SDTHeader *h = phys_to_hhdm(rsdt->entries[i]);
            handle_sdt(h, interrupt_destination_apic_id);
        }
    }
}

static uint32_t read_pm_timer() {
    uint32_t res = 0;
    if(pm_timer_port) {
        res = in32(pm_timer_port);
    } else {
        res = *pm_timer_mmio;
    }
    res &= 0xFFFFFF;
    return res;
}

void apic_init(void *rsdp_response) {

    //memory map the local APIC
    lapic_registers = setup_mmio(LAPIC_PHYS_ADDR, PAGE_SIZE);
    enable_apic();
    //Set the Spurious Interrupt Vector Register bit 8 to start receiving interrupts
    // this should be set by the limine bootloader
    assert(lapic_registers->spurious_interrupt_vector.data == 0x1FF);

    lapic_registers->initial_count.data = 1'000'000;
    lapic_registers->divide_configuration.data = 0;// /2

    lapic_registers->lvt_timer.data = 32 | (1 << 17);

    //this will enable the IOAPIC and handle timer things
    handle_rsdp(rsdp_response, lapic_registers->lapic_id.data);

    //PS/2 keyboard
    // map_ioapic_interrupt(1, 33);

    //test LAPIC
    uint32_t * special = ((void*)lapic_registers) + (lapic_registers->lapic_id.data << 12);
    *special = 69;
}

//call to restart interrupts once this one is done
void apic_eoi() {
    poll_xhci();//TODO this is in a super random place
    //end of interrupt
    lapic_registers->end_of_interrupt.data = 0;
}

static uint64_t total_interrupts = 0;
static uint64_t microseconds_per_interrupt = 0;
void apic_timer_counter() {
    static uint32_t training_temp;

    if(total_interrupts == 100) {
        training_temp = read_pm_timer();
    }
    if(total_interrupts == 101) {
        uint64_t timer_ticks_between_interrupts = read_pm_timer() - training_temp;
        microseconds_per_interrupt = (1000000ull*timer_ticks_between_interrupts) / 3579545ull;
    }

    total_interrupts++;
}
uint64_t get_uptime_ms() {
    return (total_interrupts*microseconds_per_interrupt) / 1000ull;
}

/// Handles interrupt 14
__attribute__((noreturn))
void memory_exception_handle(void* bad_address, void* rip) {
    printf("failed to access address: %p with RIP=%p\n", bad_address, rip);
    HCF
}