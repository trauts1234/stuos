#include "debugging.h"
#include "io.h"
#include "kern_libc.h"
#include "pci.h"
#include "memory.h"
#include "uapi/stdint.h"
#include "virtio_driver.h"

// struct BlockDeviceRegisters {
//     uint64_t total_sector_count;
//     uint32_t maximum_segment_size;
//     uint32_t maximum_segment_count;
//     uint16_t cylinder_count;
//     uint8_t head_count;
//     uint8_t sector_count;
//     uint32_t block_length;
// };

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

struct VQBuffer {
    uint64_t Address; // 64-bit address of the buffer on the guest machine.
    uint32_t Length;  // 32-bit length of the buffer.
    uint16_t Flags;   // 1: Next field contains linked buffer index;  2: Buffer is write-only (clear for read-only).
                        // 4: Buffer contains additional buffer addresses.
    uint16_t Next;    // If flag is set, contains index of next buffer in chain.
};
struct VQRing {
    uint32_t id;
    uint32_t len;
};

#define DEVICE_FEATURES_OFF 0
#define GUEST_FEATURES_OFF 4
#define QUEUE_ADDRESS_OFF 8
#define QUEUE_SIZE_OFF 12
#define QUEUE_SELECT_OFF 14
#define QUEUE_NOTIFY_OFF 16
#define DEVICE_STATUS_OFF 18
#define ISR_STATUS_OFF 19

// struct VirtioIORegisters {
//     //R
//     uint32_t device_features;
//     //RW
//     uint32_t guest_features;
//     //RW
//     uint32_t queue_address;
//     //R
//     uint16_t queue_size;
//     //RW
//     uint16_t queue_select;
//     //RW
//     uint32_t queue_notify;
//     //RW
//     uint8_t device_status;
//     //R
//     uint8_t isr_status;

//     struct BlockDeviceRegisters {
//         uint64_t total_sector_count;
//         uint32_t maximum_segment_size;
//         uint32_t maximum_segment_count;
//         uint16_t cylinder_count;
//         uint8_t head_count;
//         uint8_t sector_count;
//         uint32_t block_length;
//     } block_device_regs;
// };

struct VirtioQueueMetadata {
    uint64_t queue_size;
    //HHDM pointer to the array of queue_size buffers
    struct VQBuffer *buffers_arr;

    uint16_t *available_flags;
    uint16_t *available_index;
    uint16_t *available_ring_arr;

    uint16_t *used_flags;
    uint16_t *used_index;
    struct VQRing *used_ring_arr;
    uint16_t *used_avail_event;// Only used if VIRTIO_F_EVENT_IDX was negotiated
};

struct VirtioDevice {
    enum VirtioDeviceType {VIRTIO_BLOCK_DEV} dev_type;
    union {
        struct VirtioBlockDev {
            struct VirtioQueueMetadata queue;
        } block;
    } dev_data;
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

    switch(header.subsystem_id) {
        case 2:
        //block device

        default:
        return;
    }
}