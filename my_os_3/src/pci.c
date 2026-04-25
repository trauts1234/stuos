#include "debugging.h"
#include "io.h"
#include "kern_libc.h"
#include "pci.h"
#include "virtio_driver.h"

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

static uint32_t config_read(union ConfigAddress address) {
    out_long(CONFIG_ADDRESS, address.data);
    return in_long(CONFIG_DATA);
}

// output_buffer must be 256 bytes writeable
static void read_header(struct PciDevice device, void *output_buffer) {
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


                if(header.vendor_id == 0x1AF4) {
                    //virtio device
                    initialise_virtio(header, header_buffer);
                }
            }
        }
    }
}