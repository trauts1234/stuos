#include "debugging.h"
#include "io.h"
#include "kern_libc.h"
#include "pci.h"
#include "memory.h"
#include "uapi/stdint.h"
#include "virtio_driver.h"

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

//u32, R
#define DEVICE_FEATURES_OFF 0

//u32, RW
#define GUEST_FEATURES_OFF 4

//u32, RW
#define QUEUE_ADDRESS_OFF 8

//u16, R
#define QUEUE_SIZE_OFF 12

//u16, RW
#define QUEUE_SELECT_OFF 14

//u32, RW
#define QUEUE_NOTIFY_OFF 16

//u8, RW
#define DEVICE_STATUS_OFF 18
enum {DEVICE_STATUS_ACKNOWLEDGE=1, DEVICE_STATUS_DRIVER=2, DEVICE_STATUS_DRIVER_OK=4, DEVICE_STATUS_FAILED=128};

//u8, R
#define ISR_STATUS_OFF 19

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
    enum VirtioDeviceType {VIRTIO_NO_DEV=0,VIRTIO_BLOCK_DEV} dev_type;
    union {
        struct VirtioBlockDev {
            struct VirtioQueueMetadata queue;
        } block;
    } dev_data;
};

#define VIRTIO_MAX_DEVICES 10
struct VirtioDevice installed_devices[VIRTIO_MAX_DEVICES] = {};

//result.queue_size == 0 => null value
static struct VirtioQueueMetadata initialise_queue(uint64_t bar_port, uint32_t queue_number) {
    out16(bar_port + QUEUE_SELECT_OFF, queue_number);
    uint16_t sz = in16(bar_port + QUEUE_SIZE_OFF);
    if(sz == 0) return (struct VirtioQueueMetadata) {.queue_size=0};//return sentinel
    kprintf("detected queue %d size %d\n", queue_number, sz);
    HCF
}

void initialise_virtio(struct PciConfigurationHeader header, void* header_buffer, struct BarInfo bar_list[6]) {
    if(header.vendor_id != 0x1AF4) HCF
    if(header.device_id < 0x1000 || header.device_id > 0x103F) HCF
    if(header.revision_id != 0) HCF

    if(!bar_list[0].is_io_bar) HCF;
    uint64_t bar_port = bar_list[0].address;

    //find a slot for the new device
    struct VirtioDevice* new_device = NULL;
    for(uint64_t i=0; i<VIRTIO_MAX_DEVICES; i++) {
        if(installed_devices[i].dev_type == VIRTIO_NO_DEV) {
            new_device = installed_devices + i;
            break;
        }
    }
    if(new_device == NULL) HCF

    //reset the device
    out8(bar_port + DEVICE_STATUS_OFF, 0);
    spin_wait();
    uint8_t status = in8(bar_port + DEVICE_STATUS_OFF);
    kprintf("after resetting virtio, status: 0x%X\n", status);
    out8(bar_port + DEVICE_STATUS_OFF, DEVICE_STATUS_ACKNOWLEDGE | DEVICE_STATUS_DRIVER);
    spin_wait();
    status = in8(bar_port + DEVICE_STATUS_OFF);
    kprintf("after acknowledging, status: 0x%X\n", status);


    switch(header.subsystem_id) {
        case 2:
        //block device

        //load first queue
        struct VirtioQueueMetadata queue = initialise_queue(bar_port, 0);
        if(queue.queue_size == 0) HCF//queue is invalid

        default:
        return;
    }
}