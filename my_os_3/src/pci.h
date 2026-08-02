#ifndef PCI_H
#define PCI_H

#include <uapi/stdint.h>
#include <uapi/stdbool.h>

/// This is a dump of:
/// - the shared header: registers 0..=3, so offsets 0x0 to 0xC
/// - the type 0x0 header: registers 0x4..=0xF, so offsets 0x10 to 0x3C
struct PciConfigurationHeader {
    uint16_t vendor_id, device_id;
    uint16_t command, status;
    uint8_t revision_id, prog_if, subclass, class_code;
    uint8_t cache_line_size, latency_timer, header_type, bist;

    //The following are only valid if PciConfigurationHeader.header_type == 0
    uint32_t BAR[6];//base address 0..=5
    uint32_t cardbus_cis_pointer;

    uint16_t subsystem_vendor_id, subsystem_id;
    uint32_t expansion_rom_base_address;
    uint8_t capabilities_pointer, reserved0; uint16_t reserved1;
    uint32_t reserved2;
    uint8_t interrupt_line, interrupt_pin, min_grant, max_latency;
};

struct BarInfo {
    uint64_t bar_size;
    uint64_t address;
    // only if `!is_io_bar`
    volatile void* virtual_address;
    //true if bar uses in/out - else uses memory mapped IO
    bool is_io_bar;
};

//represents a PCI device whose configuration can be read by a port
struct PciDevice {
    uint8_t function_number:3;
    uint8_t device_number:5;
    uint8_t bus_number;
};

void initialise_pci();

uint32_t read_bar_32(struct BarInfo bar, uint64_t offset);
void write_bar_32(struct BarInfo bar, uint32_t src, uint64_t offset);
void read_header(struct PciDevice device, uint8_t output_buffer[256]);
void write_header(struct PciDevice device, uint8_t input_buffer[256]);

#endif