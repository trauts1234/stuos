#include "debugging.h"
#include "io.h"
#include "kern_libc.h"
#include "pci.h"
#include "memory.h"
#include "physical_slab_allocation.h"
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

//all the pointers are HHDM
struct VirtioQueueMetadata {
    uint16_t queue_size;
    //HHDM pointer to the array of queue_size buffers
    struct VQBuffer *buffers_arr;

    uint16_t *available_flags;
    uint16_t *available_index;
    uint16_t *available_ring_arr;

    //4096 byte alignment for the uint16
    uint16_t *used_flags;
    uint16_t *used_index;
    struct VQRing *used_ring_arr;
    // uint16_t *used_avail_event;// Only used if VIRTIO_F_EVENT_IDX was negotiated
};

struct VirtioBlockDevice {
    uint16_t bar_port;
    struct VirtioQueueMetadata queue;
};

struct __attribute__((packed)) BlockDevicePacket {
    uint32_t Type;              // 0: Read; 1: Write; 4: Flush; 11: Discard; 13: Write zeroes
    uint32_t Reserved;
    uint64_t Sector; //get the (Sector*512)'th bytes of the drive
    volatile uint8_t Data[BLOCK_DEVICE_READ_SIZE];             // Data's size must be a multiple of 512
    volatile uint8_t Status;             // 0: OK; 1: Error; 2: Unsupported
};

static struct VirtioBlockDevice block_device = {};

//result.queue_size == 0 => null value
static struct VirtioQueueMetadata initialise_queue(uint64_t bar_port, uint32_t queue_number) {
    out16(bar_port + QUEUE_SELECT_OFF, queue_number);
    uint16_t sz = in16(bar_port + QUEUE_SIZE_OFF);
    if(sz == 0) return (struct VirtioQueueMetadata) {.queue_size=0};//return sentinel

    //the size includes some unused fields
    uint64_t first_part_size_bytes = sizeof(struct VQBuffer)*sz + 2 + 2 + 2*sz + 2;
    uint64_t first_part_size_pages = round_up_pages(first_part_size_bytes);
    uint64_t second_part_size_bytes = 2 + 2 + sizeof(struct VQRing)*sz + 2;
    uint64_t second_part_size_pages = round_up_pages(second_part_size_bytes);
    uint64_t total_pages = first_part_size_pages + second_part_size_pages;

    //allocate and zero contiguous region
    uint64_t first_part_phys = malloc_contiguous_phys(total_pages);
    uint64_t second_part_phys = first_part_phys + PAGE_SIZE*first_part_size_pages;
    memset(phys_to_hhdm(first_part_phys), 0, total_pages * PAGE_SIZE);

    //tell the virtio device about the region
    out32(bar_port + QUEUE_ADDRESS_OFF, first_part_phys/PAGE_SIZE);//write the page number
    
    struct VirtioQueueMetadata r;
    r.queue_size = sz;
    r.buffers_arr = phys_to_hhdm(first_part_phys);
    r.available_flags = phys_to_hhdm(first_part_phys += (sizeof(struct VQBuffer) * sz));
    r.available_index = phys_to_hhdm(first_part_phys += 2);
    r.available_ring_arr = phys_to_hhdm(first_part_phys += 2);

    r.used_flags = phys_to_hhdm(second_part_phys);
    r.used_index = phys_to_hhdm(second_part_phys += 2);
    r.used_ring_arr = phys_to_hhdm(second_part_phys += 2);

    *r.available_flags = 1;//disable interrupts

    return r;
}

enum {VQBufferFlagsWrite = 0b10, VQBufferFlagsNotLast = 0b1};// not including any of these flags means the opposite
enum {VQTypeRead, VQTypeWrite};//this is to be read only or write only *for the deviec* - you must do the opposite

void virtio_block_read(uint64_t sector_number, uint8_t output[BLOCK_DEVICE_READ_SIZE]) {
    if(block_device.queue.queue_size == 0) HCF

    uint64_t packet_phys = malloc4k_phys();

    const uint8_t PLACEHOLDER_STATUS = 69;

    struct BlockDevicePacket *packet_hhdm = phys_to_hhdm(packet_phys);
    *packet_hhdm = (struct BlockDevicePacket) {
        .Type = VQTypeRead,
        .Reserved = 0,
        .Sector = 0,
        .Data = {},
        .Status = PLACEHOLDER_STATUS
    };

    struct VirtioQueueMetadata md = block_device.queue;
    md.buffers_arr[0] = (struct VQBuffer) {
        .Address = packet_phys,
        .Flags = VQBufferFlagsNotLast,//next is valid, read only (for the device)
        .Next = 1,
        .Length = 16
    };
    md.buffers_arr[1] = (struct VQBuffer) {
        .Address = packet_phys + 16,
        .Flags = VQBufferFlagsNotLast | VQBufferFlagsWrite,
        .Next = 2,
        .Length = sizeof(struct BlockDevicePacket) - 16 - 1
    };
    md.buffers_arr[2] = (struct VQBuffer) {
        .Address = packet_phys + sizeof(struct BlockDevicePacket) - 1,
        .Flags = VQBufferFlagsWrite,
        .Next = 0,
        .Length = 1,
    };

    uint16_t ring_slot = *md.available_index % md.queue_size;
    md.available_ring_arr[ring_slot] = 0;//index into buffers_arr
    (*md.available_index)++;

    out32(block_device.bar_port + QUEUE_NOTIFY_OFF, 0);//write the buffer select number
    
    while (packet_hhdm->Status == PLACEHOLDER_STATUS) {
        spin_wait();
    }

    if(packet_hhdm->Status != 0) HCF
    memcpy(output, (const void*)packet_hhdm->Data, BLOCK_DEVICE_READ_SIZE);

    //free the page again
    free4k_phys(packet_phys);
}

void virtio_block_write(uint64_t sector_number, uint8_t input[BLOCK_DEVICE_READ_SIZE]) {
    if(block_device.queue.queue_size == 0) HCF

    uint64_t packet_phys = malloc4k_phys();

    const uint8_t PLACEHOLDER_STATUS = 69;

    struct BlockDevicePacket *packet_hhdm = phys_to_hhdm(packet_phys);
    *packet_hhdm = (struct BlockDevicePacket) {
        .Type = VQTypeWrite,//write
        .Reserved = 0,
        .Sector = 0,
        .Data = {},
        .Status = PLACEHOLDER_STATUS
    };
    memcpy((void*)packet_hhdm->Data, input, BLOCK_DEVICE_READ_SIZE);

    struct VirtioQueueMetadata md = block_device.queue;
    md.buffers_arr[0] = (struct VQBuffer) {
        .Address = packet_phys,
        .Flags = VQBufferFlagsNotLast,//next is valid, read only (for the device)
        .Next = 1,
        .Length = 16
    };
    md.buffers_arr[1] = (struct VQBuffer) {
        .Address = packet_phys + 16,
        .Flags = VQBufferFlagsNotLast,//next is valid, read only
        .Next = 2,
        .Length = sizeof(struct BlockDevicePacket) - 16 - 1
    };
    md.buffers_arr[2] = (struct VQBuffer) {
        .Address = packet_phys + sizeof(struct BlockDevicePacket) - 1,
        .Flags = VQBufferFlagsWrite,//write only
        .Next = 0,
        .Length = 1,
    };

    uint16_t ring_slot = *md.available_index % md.queue_size;
    md.available_ring_arr[ring_slot] = 0;//index into buffers_arr
    (*md.available_index)++;

    out32(block_device.bar_port + QUEUE_NOTIFY_OFF, 0);//write the buffer select number
    
    while (packet_hhdm->Status == PLACEHOLDER_STATUS) {
        spin_wait();
    }

    if(packet_hhdm->Status != 0) HCF

    //free the page again
    free4k_phys(packet_phys);
}

void initialise_virtio(struct PciConfigurationHeader header, void* header_buffer, struct BarInfo bar_list[6]) {
    if(header.vendor_id != 0x1AF4) HCF
    if(header.device_id < 0x1000 || header.device_id > 0x103F) HCF
    if(header.revision_id != 0) HCF

    if(!bar_list[0].is_io_bar) HCF;

    //check whether a drive has already been set up
    if(block_device.queue.queue_size != 0) HCF

    const uint16_t bar_port = bar_list[0].address;

    //reset the device
    out8(bar_port + DEVICE_STATUS_OFF, 0);
    spin_wait();
    uint8_t status = in8(bar_port + DEVICE_STATUS_OFF);
    out8(bar_port + DEVICE_STATUS_OFF, DEVICE_STATUS_ACKNOWLEDGE | DEVICE_STATUS_DRIVER);
    spin_wait();
    status = in8(bar_port + DEVICE_STATUS_OFF);


    switch(header.subsystem_id) {
        case 2:
        block_device.bar_port = bar_port;

        //load first queue
        struct VirtioQueueMetadata queue = initialise_queue(bar_port, 0);
        if(queue.queue_size == 0) HCF//queue is invalid

        spin_wait();
        status = in8(bar_port + DEVICE_STATUS_OFF);

        block_device.queue = queue;

        break;

        default:
        return;
    }

    out8(bar_port + DEVICE_STATUS_OFF, DEVICE_STATUS_ACKNOWLEDGE | DEVICE_STATUS_DRIVER | DEVICE_STATUS_DRIVER_OK);
    spin_wait();

    // char data[512] = "this was written";
    // virtio_block_write(0, (uint8_t*)data);
    // char data2[512];
    // virtio_block_read(0, (uint8_t*) data2);
    // kprintf(data2);

}