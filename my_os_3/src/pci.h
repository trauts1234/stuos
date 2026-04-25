#ifndef PCI_H
#define PCI_H

#include "uapi/stdint.h"

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

void initialise_pci();

#endif