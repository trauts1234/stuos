#include "debugging.h"
#include "io.h"
#include "uapi/stdint.h"
#include "kern_libc.h"

#define CONFIG_ADDRESS 0xCF8
#define CONFIG_DATA 0xCFC

//represents a PCI device whose configuration can be read by a port
struct PciDevice {
    uint8_t function_number:3;
    uint8_t device_number:5;
    uint8_t bus_number;
};

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

/// This is a dump of registers 0..=3, so offsets 0x0 to 0xC
struct PciConfigurationHeader {
    uint16_t vendor_id, device_id;
    uint16_t command, status;
    uint8_t revision_id, prog_if, subclass, class_code;
    uint8_t cache_line_size, latency_timer, header_type, bist;
};

#define HEADER_0x0_OFFSET 0x10
//This is only valid if PciConfigurationHeader.header_type == 0
struct PciExtendedHeaderType0x0 {
    //this starts at register 0x4 (offset HEADER_0x0_OFFSET)
    uint32_t BAR[6];//base address 0..=5
    uint32_t cardbus_cis_pointer;

    uint16_t subsystem_vendor_id, subsystem_id;
    uint32_t expansion_rom_base_address;
    uint8_t capabilities_pointer, reserved0; uint16_t reserved1;
    uint32_t reserved2;
    uint8_t interrupt_line, interrupt_pin, min_grant, max_latency;
};

static uint32_t config_read(union ConfigAddress address) {
    out_long(CONFIG_ADDRESS, address.data);
    return in_long(CONFIG_DATA);
}

static struct PciConfigurationHeader read_header(struct PciDevice device) {
    struct PciConfigurationHeader result;
    uint32_t *dwords = (uint32_t*)&result;

    for(int i=0; i<=sizeof(struct PciConfigurationHeader); i += 4) {

        union ConfigAddress addr = {
            .register_offset = i,
            .device = device,
            .reserved = 0,
            .enable_bit = 1,
        };
        *dwords++ = config_read(addr);
    }
        
    return result;
}
static struct PciExtendedHeaderType0x0 read_0x0_header(struct PciDevice device) {
    struct PciExtendedHeaderType0x0 result;
    uint32_t *dwords = (uint32_t*)&result;

    for(int i=HEADER_0x0_OFFSET; i <= HEADER_0x0_OFFSET + sizeof(struct PciExtendedHeaderType0x0); i += 4) {

        union ConfigAddress addr = {
            .register_offset = i,
            .device = device,
            .reserved = 0,
            .enable_bit = 1,
        };
        *dwords++ = config_read(addr);
    }
        
    return result;
}

static void initialise_virtio(struct PciConfigurationHeader header, struct PciExtendedHeaderType0x0 header_ext) {
    if(header.vendor_id != 0x1AF4) HCF
    if(header.device_id < 0x1000 || header.device_id > 0x103F) HCF
    

    switch(header_ext.subsystem_id) {
        case 2:
        //block device

    }
}

void initialise_pci() {
    for(unsigned int bus_number = 0; bus_number < 256; bus_number++) {
        for(unsigned int device_number = 0; device_number < 32; device_number++) {
            for(unsigned int function_number = 0; function_number < 8; function_number++) {
                struct PciDevice device = {
                    .function_number = function_number, .device_number = device_number, .bus_number = bus_number
                };
                struct PciConfigurationHeader header = read_header(device);

                if(header.vendor_id == 0xFFFF) continue;//FFFF means no device
                if(header.header_type != 0) continue;//not a standard PCI device

                struct PciExtendedHeaderType0x0 header_ext = read_0x0_header(device);

                // kprintf("vendor: %x, device_id: %x\n", header.vendor_id, header.device_id);

                if(header.vendor_id == 0x1AF4) {
                    //virtio device
                    initialise_virtio(header, header_ext);
                }
            }
        }
    }
}