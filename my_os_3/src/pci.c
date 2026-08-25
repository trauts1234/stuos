#include "apic.h"
#include "debugging.h"
#include "idt.h"
#include "io.h"
#include "kern_libc.h"
#include "pci.h"
#include "virtio_driver.h"
#include <uapi/stdint.h>
#include "memory.h"
#include "xhci_driver.h"

#define CONFIG_ADDRESS 0xCF8
#define CONFIG_DATA 0xCFC

#define USE_MSI_MSIX false

//this is _after_ the capability ID and next pointer
struct MSIData {
    //message control
    uint16_t
        enable: 1,
        multiple_message_capable: 3,
        multiple_message_enable: 3,
        is_64_bit: 1,
        per_vector_masking: 1,
        reserved_1: 7;
    uint64_t message_address;
    //message data
    uint16_t 
        vector: 8,
        delivery_mode: 3,
        reserved_2: 3,
        level: 1,
        trigger_mode: 1;
    uint16_t reserved_4;
    //only used if per_vector_masking:
    // uint32_t mask;//mask message by setting 1<<n
    // uint32_t pending;//n is pending if 1<<n set
} __attribute__ ((packed));

struct MSIXData {
    uint16_t
        table_size: 11,
        reserved_0: 3,
        function_mask: 1,
        enable: 1;
    uint32_t table_address; // &0b111 = bar number, &~0b111 = offset in BAR
    uint32_t pending_bit; //&0b111 = pending bar number, &~0b111 = pending bit offset
} __attribute__ ((packed));

uint32_t config_read(union ConfigAddress address) {
    out32(CONFIG_ADDRESS, address.data);
    return in32(CONFIG_DATA);
}
void config_write(union ConfigAddress address, uint32_t value) {
    out32(CONFIG_ADDRESS, address.data);
    out32(CONFIG_DATA, value);
}

void read_header(struct PciDevice device, uint8_t output_buffer[256]) {
    uint32_t *dwords = (uint32_t*)output_buffer;

    for(int i=0; i<256; i += 4) {

        union ConfigAddress addr = {
            .register_offset = i,
            .device = device,
            .reserved = 0,
            .enable_bit = 1,
        };
        *dwords++ = config_read(addr);
    }       
}
void write_header(struct PciDevice device, uint8_t input_buffer[256]) {
    uint32_t *dwords = (uint32_t*)input_buffer;

    for(int i=0; i<256; i += 4) {

        union ConfigAddress addr = {
            .register_offset = i,
            .device = device,
            .reserved = 0,
            .enable_bit = 1,
        };
        config_write(addr, *dwords++);
    }       
}

static uint32_t get_bar_size_32(uint32_t original_bar, union ConfigAddress addr) {
    //find bar length
    config_write(addr, ~0);

    uint32_t bar_size = config_read(addr);
    bar_size = ~bar_size + 1;

    //restore BAR
    config_write(addr, original_bar);

    return bar_size;
}

static uint64_t get_bar_size_64(uint32_t original_bar_low, uint32_t original_bar_high, union ConfigAddress addr_low, union ConfigAddress addr_high) {
    //find bar length
    config_write(addr_low, ~0);
    config_write(addr_high, ~0);

    uint64_t bar_size = ((uint64_t)config_read(addr_high) << 32) | config_read(addr_low);
    bar_size = ~bar_size + 1;

    //restore BAR
    config_write(addr_low, original_bar_low);
    config_write(addr_high, original_bar_high);

    return bar_size;
}

static void handle_bar(struct PciDevice device, struct PciConfigurationHeader header, struct BarInfo output_bar_list[6]) {
    int index_in_config=0;
    while (index_in_config < 6) {
        union ConfigAddress addr_low = {
            .register_offset = 0x10 + 4 * index_in_config,
            .device = device,
            .reserved = 0,
            .enable_bit = 1
        };
        union ConfigAddress addr_high = {
            .register_offset = 0x14 + 4 * index_in_config,
            .device = device,
            .reserved = 0,
            .enable_bit = 1
        };

        uint32_t bar_val_low = header.BAR[index_in_config];
        if(bar_val_low & 1) {
            //uses IN/OUT to write, as this is an IO space BAR
            output_bar_list[index_in_config] = (struct BarInfo) {
                .bar_size = get_bar_size_32(bar_val_low, addr_low),
                .address = bar_val_low & ~0xFul,
                .is_io_bar = true
            };

            index_in_config += 1;
        } else {
            switch ((bar_val_low >> 1) & 0b11) {
                case 0:
                {
                    uint64_t bar_size = get_bar_size_32(bar_val_low, addr_low);
                    uint64_t address = bar_val_low & ~0xFul;

                    void* virtual_address = setup_mmio(address, bar_size);
                    output_bar_list[index_in_config] = (struct BarInfo) {
                        .bar_size = bar_size,
                        .address = address,
                        .virtual_address = virtual_address,
                        .is_io_bar = false,
                    };

                    index_in_config += 1;
                    break;
                }

                case 1:
                case 3:
                HCF//invalid
                
                case 2:
                // 64 bit address
                {
                    uint32_t bar_val_high = header.BAR[index_in_config + 1];
                    uint64_t bar_size = get_bar_size_64(bar_val_low, bar_val_high, addr_low, addr_high);
                    uint64_t address = ((uint64_t)bar_val_high << 32) | (bar_val_low & ~0xFul);

                    void* virtual_address = setup_mmio(address, bar_size);
                    output_bar_list[index_in_config + 1] = output_bar_list[index_in_config] = (struct BarInfo) {
                        .bar_size = bar_size,
                        .address = address,
                        .virtual_address = virtual_address,
                        .is_io_bar = false,
                    };

                    index_in_config += 2;
                    break;
                }
            }
        }
    }
}

//may do any number of byte reads
uint32_t read_bar_32(struct BarInfo bar, uint64_t offset) {
    assert(offset % 4 == 0);

    if(bar.is_io_bar) {
        if(bar.address + offset > 0xFFFF) HCF
        return in32(bar.address + offset);
    } else {
        return *(volatile uint32_t*)(bar.virtual_address + offset);
    }
}
//forces 32 bit read/writes
void write_bar_32(struct BarInfo bar, uint32_t src, uint64_t offset) {
    assert(offset % 4 == 0);

    if(bar.is_io_bar) {
        if(bar.address + offset > 0xFFFF) HCF
        out32(bar.address + offset, src);
    } else {
        *(volatile uint32_t*)(bar.virtual_address + offset) = src;
    }
}

void initialise_pci() {
    for(unsigned int bus_number = 0; bus_number < 256; bus_number++) {
        for(unsigned int device_number = 0; device_number < 32; device_number++) {
            for(unsigned int function_number = 0; function_number < 8; function_number++) {
                struct PciDevice device = {
                    .function_number = function_number, .device_number = device_number, .bus_number = bus_number
                };

                uint8_t header_buffer[256] = {1};
                read_header(device, header_buffer);

                struct PciConfigurationHeader header = *(struct PciConfigurationHeader*)&header_buffer;
                if(header.vendor_id == 0xFFFF) continue;//FFFF means no device
                if(header.header_type != 0) continue;//not a standard PCI device

                struct PciData dev = {};

                handle_bar(device, header, dev.bar_list);

                if(header.status & (1 << 4)) {
                    //pointer to capabilities list is stored at index 0x34
                    uint8_t msi_idx=0, msix_idx=0;
                    for(uint8_t capability_pointer = header_buffer[0x34]; capability_pointer; capability_pointer = header_buffer[capability_pointer+1]) {
                        uint8_t capability_id = header_buffer[capability_pointer];
                        switch(capability_id) {
                            case 0x05:
                            msi_idx = capability_pointer+2;
                            break;
                            
                            case 0x11:
                            msix_idx = capability_pointer+2;
                            break;
                            
                            default:
                            break;//unknown
                        }
                    }
                    // assert((msi_idx && msix_idx) == 0);//can't have both?
                    if(msi_idx && USE_MSI_MSIX) {
                        struct MSIData *data = (struct MSIData *)&header_buffer[msi_idx];
                        assert(!data->enable);
                        assert(data->is_64_bit);
                        printf("device supports %u MSI interrupts\n", 1<<data->multiple_message_capable);
                        data->multiple_message_enable = 0;// enables 1<<0 messages
                        data->enable = 1;

                        data->message_address = LAPIC_PHYS_ADDR;//there are some flags here, but they are zeroed (page 3605 of the intel combined volumes)
                        int allocated_vec = allocate_free_idt_entry();
                        data->vector = allocated_vec;
                        write_header(device, header_buffer);

                        dev.allocated_interrupt = allocated_vec;
                    }
                    if(msix_idx && USE_MSI_MSIX) {
                        struct MSIXData *data = (struct MSIXData *)&header_buffer[msix_idx];
                        assert(!data->enable);
                        uint32_t table_size = data->table_size+1;
                        data->enable = 1;
                        
                        int bar_number = data->table_address & 0b111;
                        uint64_t table_addr = data->table_address & ~0b111;
                        
                        printf("device supports %u MSI-X interrupts (bar %d offset 0x%llx)\n", table_size, bar_number, table_addr);
                        int allocated_vec = allocate_free_idt_entry();

                        //point all interrupts to one handler for now
                        for(uint64_t i=0; i<table_size; i++) {
                            uint64_t offset = table_addr + i*16ull;
                            //message address
                            write_bar_32(dev.bar_list[bar_number], LAPIC_PHYS_ADDR & 0xFFFFFFFF, offset);
                            write_bar_32(dev.bar_list[bar_number], LAPIC_PHYS_ADDR >> 32, offset+4);
                            //message data
                            write_bar_32(dev.bar_list[bar_number], allocated_vec, offset+8);
                            //vector control
                            write_bar_32(dev.bar_list[bar_number], 0, offset+12);//enable
                        }

                        write_header(device, header_buffer);
                        dev.allocated_interrupt = allocated_vec;
                    }
                }

                if(header.vendor_id == 0x1AF4) {
                    //virtio device
                    initialise_virtio(header, header_buffer, dev.bar_list);
                }

                if(header.class_code == 0x0C && header.subclass == 0x03 && header.prog_if == 0x20) {
                    //EHCI USB controller
                    // printf("EHCI found\n");
                    // initialise_ehci(device, bar_list[0]);
                }
                if(header.class_code == 0x0C && header.subclass == 0x03 && header.prog_if == 0x30) {
                    printf("xHCI found\n");
                    initialise_xhci(device, &dev);
                }
                
            }
        }
    }
}