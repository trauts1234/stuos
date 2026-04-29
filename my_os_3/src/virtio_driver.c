#include "debugging.h"
#include "io.h"
#include "kern_libc.h"
#include "pci.h"
#include "memory.h"
#include "uapi/stdint.h"
#include "virtio_driver.h"

struct BlockDeviceRegisters {
    uint64_t total_sector_count;
    uint32_t maximum_segment_size;
    uint32_t maximum_segment_count;
    uint16_t cylinder_count;
    uint8_t head_count;
    uint8_t sector_count;
    uint32_t block_length;
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

// Descriptor Table entry (16 bytes each)
struct VQBuffer {
    uint64_t Address; // 64-bit address of the buffer on the guest machine.
    uint32_t Length;  // 32-bit length of the buffer.
    uint16_t Flags;   // 1: Next field contains linked buffer index;  2: Buffer is write-only (clear for read-only).
                        // 4: Buffer contains additional buffer addresses.
    uint16_t Next;    // If flag is set, contains index of next buffer in chain.
};

// Available Ring (driver → device)
struct VringAvail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[queue_size];
    // uint16_t used_event; // optional, at end
};

// Used Ring (device → driver)
struct VringUsed {
    uint16_t flags;
    uint16_t idx;
    struct { uint32_t id; uint32_t len; } ring[queue_size];
    // uint16_t avail_event; // optional, at end
};

#define DEVICE_FEATURES_OFF 0
#define GUEST_FEATURES_OFF 4
#define QUEUE_ADDRESS_OFF 8
#define QUEUE_SIZE_OFF 12
#define QUEUE_SELECT_OFF 14
#define QUEUE_NOTIFY_OFF 16
#define DEVICE_STATUS_OFF 18
#define ISR_STATUS_OFF 19

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

void initialise_virtio(struct PciConfigurationHeader header, void* header_buffer, struct BarInfo bar_list[6]) {
    if(header.vendor_id != 0x1AF4) HCF
    if(header.device_id < 0x1000 || header.device_id > 0x103F) HCF
    if(header.revision_id != 0) HCF

    if(!bar_list[0].is_io_bar) HCF;
    uint64_t bar_port = bar_list[0].address;

    //reset the device
    out8(bar_port + DEVICE_STATUS_OFF, 0);
    spin_wait();

    //print queues
    for(uint32_t i=0; i<0xFFFF; i++) {
        out16(bar_port + QUEUE_SELECT_OFF, i);
        uint16_t sz = in16(bar_port + QUEUE_SIZE_OFF);
        if(sz == 0) break;
        kprintf("detected queue %d size %d\n", i, sz);

        //TODO set queue address
    }

    // struct VirtioIORegisters virtio_regs = {
    //     .device_features = in32(bar_port),
    //     .guest_features = in32(bar_port + 4),
    //     .queue_address = in32(bar_port + 8),
    //     .queue_size = in16(bar_port + 12),
    //     .queue_select = in16(bar_port + 14),
    //     .queue_notify = in16(bar_port + 16),
    //     .device_status = in8(bar_port + 18),
    //     .isr_status = in8(bar_port + 19),
    // };
    
    // struct VirtioIORegisters *v = &virtio_regs;

    //     kprintf("VirtioIORegisters {\n");
    // kprintf("  device_features      = 0x%08X\n", v->device_features);
    // kprintf("  guest_features       = 0x%08X\n", v->guest_features);
    // kprintf("  queue_address        = 0x%08X\n", v->queue_address);
    // kprintf("  queue_size           = %u (0x%04X)\n", v->queue_size, v->queue_size);
    // kprintf("  queue_select         = %u (0x%04X)\n", v->queue_select, v->queue_select);
    // kprintf("  queue_notify         = 0x%08X\n", v->queue_notify);
    // kprintf("  device_status        = 0x%02X\n", v->device_status);
    // kprintf("  isr_status           = 0x%02X\n", v->isr_status);

    // /* BlockDeviceRegisters ----------------------------------------------*/
    // const struct BlockDeviceRegisters *b = &v->block_device_regs;
    // kprintf("  block_device_regs {\n");
    // kprintf("    total_sector_count   = %llu (0x%016llX)\n",
    //        (unsigned long long)b->total_sector_count,
    //        (unsigned long long)b->total_sector_count);
    // kprintf("    maximum_segment_size = 0x%08X\n", b->maximum_segment_size);
    // kprintf("    maximum_segment_count= 0x%08X\n", b->maximum_segment_count);
    // kprintf("    cylinder_count       = %u (0x%04X)\n", b->cylinder_count, b->cylinder_count);
    // kprintf("    head_count           = %u (0x%02X)\n", b->head_count, b->head_count);
    // kprintf("    sector_count         = %u (0x%02X)\n", b->sector_count, b->sector_count);
    // kprintf("    block_length         = 0x%08X\n", b->block_length);
    // kprintf("  }\n");
    // kprintf("}\n");

    switch(header.subsystem_id) {
        case 2:
        //block device

        default:
        return;
    }
}