#include "debugging.h"
#include "kern_libc.h"
#include "pci.h"
#include "uapi/stdint.h"
#include "virtio_driver.h"

struct VirtioIORegisters {
    //R
    uint32_t device_features;
    //RW
    uint32_t guest_features;
    //RW
    uint32_t queue_address;
    //R
    uint16_t queue_size;
    //RW
    uint16_t queue_select;
    //RW
    uint32_t queue_notify;
    //RW
    uint8_t device_status;
    //R
    uint8_t isr_status;

    struct BlockDeviceRegisters {
        uint64_t total_sector_count;
        uint32_t maximum_segment_size;
        uint32_t maximum_segment_count;
        uint16_t cylinder_count;
        uint8_t head_count;
        uint8_t sector_count;
        uint32_t block_length;
    } block_device_regs;
};

struct VirtioCapabilitiesHeader {
    uint8_t capability_id;
    //0 for NULL
    uint8_t next;
    uint8_t capability_length;
    uint8_t config_type;
    uint8_t bar;
    uint8_t padding[3];
    uint32_t offset_in_bar;
    uint32_t length_in_bar;
};

void initialise_virtio(struct PciConfigurationHeader header, void* header_buffer) {
    if(header.vendor_id != 0x1AF4) HCF
    if(header.device_id < 0x1000 || header.device_id > 0x103F) HCF

    if((header.status & 0b10000) == 0) HCF//need capabilities list
    struct VirtioCapabilitiesHeader *capabilities = (struct VirtioCapabilitiesHeader*)(header_buffer + (header.capabilities_pointer & ~0b11));
    while(capabilities->capability_id != 0x09) {
        if(capabilities->next == 0) HCF//ran out of capabilities
        capabilities = (struct VirtioCapabilitiesHeader*)(header_buffer + capabilities->next);
    }

    //virtio registers should be immediately after the capabilities header
    //TODO set up and read from the BAR
    // struct VirtioIORegisters virtio_regs = *(struct VirtioIORegisters*)(capabilities + 1);
    
    switch(header.subsystem_id) {
        case 2:
        //block device

        struct VirtioIORegisters *v = &virtio_regs;

        kprintf("VirtioIORegisters {\n");
    kprintf("  device_features      = 0x%08X\n", v->device_features);
    kprintf("  guest_features       = 0x%08X\n", v->guest_features);
    kprintf("  queue_address        = 0x%08X\n", v->queue_address);
    kprintf("  queue_size           = %u (0x%04X)\n", v->queue_size, v->queue_size);
    kprintf("  queue_select         = %u (0x%04X)\n", v->queue_select, v->queue_select);
    kprintf("  queue_notify         = 0x%08X\n", v->queue_notify);
    kprintf("  device_status        = 0x%02X\n", v->device_status);
    kprintf("  isr_status           = 0x%02X\n", v->isr_status);

    /* BlockDeviceRegisters ----------------------------------------------*/
    const struct BlockDeviceRegisters *b = &v->block_device_regs;
    kprintf("  block_device_regs {\n");
    kprintf("    total_sector_count   = %llu (0x%016llX)\n",
           (unsigned long long)b->total_sector_count,
           (unsigned long long)b->total_sector_count);
    kprintf("    maximum_segment_size = 0x%08X\n", b->maximum_segment_size);
    kprintf("    maximum_segment_count= 0x%08X\n", b->maximum_segment_count);
    kprintf("    cylinder_count       = %u (0x%04X)\n", b->cylinder_count, b->cylinder_count);
    kprintf("    head_count           = %u (0x%02X)\n", b->head_count, b->head_count);
    kprintf("    sector_count         = %u (0x%02X)\n", b->sector_count, b->sector_count);
    kprintf("    block_length         = 0x%08X\n", b->block_length);
    kprintf("  }\n");
    kprintf("}\n");

        default:
        return;
    }
}