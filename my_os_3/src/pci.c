#include "debugging.h"
#include "ehci_driver.h"
#include "io.h"
#include "kern_libc.h"
#include "pci.h"
#include "virtio_driver.h"
#include <uapi/stdint.h>
#include "memory.h"

#define CONFIG_ADDRESS 0xCF8
#define CONFIG_DATA 0xCFC

//what raw data can be sent to a port
union ConfigAddress {
    uint32_t data;

    struct {
        uint8_t register_offset;
        struct PciDevice device;
        uint8_t reserved:7;
        uint8_t enable_bit:1;
    };
};

static uint32_t config_read(union ConfigAddress address) {
    out32(CONFIG_ADDRESS, address.data);
    return in32(CONFIG_DATA);
}
static void config_write(union ConfigAddress address, uint32_t value) {
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
    int bar_number=0;
    while (bar_number < 6) {
        union ConfigAddress addr_low = {
            .register_offset = 0x10 + 4 * bar_number,
            .device = device,
            .reserved = 0,
            .enable_bit = 1
        };
        union ConfigAddress addr_high = {
            .register_offset = 0x14 + 4 * bar_number,
            .device = device,
            .reserved = 0,
            .enable_bit = 1
        };

        uint32_t bar_val_low = header.BAR[bar_number];
        if(bar_val_low & 1) {
            //uses IN/OUT to write, as this is an IO space BAR

            output_bar_list[bar_number] = (struct BarInfo) {
                .bar_size = get_bar_size_32(bar_val_low, addr_low),
                .address = bar_val_low & ~0xFul,
                .is_io_bar = true
            };

            bar_number += 1;
        } else {
            switch ((bar_val_low >> 1) & 0b11) {
                case 0:
                {
                    uint64_t bar_size = get_bar_size_32(bar_val_low, addr_low);
                    uint64_t address = bar_val_low & ~0xFul;

                    void* virtual_address = setup_mmio(address, bar_size);

                    output_bar_list[bar_number] = (struct BarInfo) {
                        .bar_size = bar_size,
                        .address = address,
                        .virtual_address = virtual_address,
                        .is_io_bar = false,
                    };

                    bar_number += 1;
                    break;
                }

                case 1:
                case 3:
                HCF//invalid
                
                case 2:
                // 64 bit address
                {
                    uint32_t bar_val_high = header.BAR[bar_number + 1];
                    uint64_t bar_size = get_bar_size_64(bar_val_low, bar_val_high, addr_low, addr_high);
                    uint64_t address = ((uint64_t)bar_val_high << 32) | (bar_val_low & ~0xFul);

                    void* virtual_address = setup_mmio(address, bar_size);

                    output_bar_list[bar_number + 1] = output_bar_list[bar_number] = (struct BarInfo) {
                        .bar_size = bar_size,
                        .address = address,
                        .virtual_address = virtual_address,
                        .is_io_bar = false,
                    };

                    bar_number += 2;
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

                struct BarInfo bar_list[6];
                handle_bar(device, header, bar_list);

                if(header.vendor_id == 0x1AF4) {
                    //virtio device
                    initialise_virtio(header, header_buffer, bar_list);
                }

                printf("cc: %X sc: %X IF: %X\n", header.class_code, header.subclass, header.prog_if);

                if(header.class_code == 0x0C && header.subclass == 0x03 && header.prog_if == 0x20) {
                    //EHCI USB controller
                    printf("EHCI found\n");
                    initialise_ehci(device, bar_list[0]);
                }
                
            }
        }
    }
}