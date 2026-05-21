#ifndef FS_DEV
#define FS_DEV
#include "uapi/stdint.h"

#define BLOCK_DEVICE_READ_SIZE 512

// Add a device, that will be named diskX
//
// I should be supplied with functions to read 512 bytes sectors of the disk
//
// This will be called by drivers once they are set up
void fs_dev_add_block_device(
    void* driver_private,
    void (*block_read)(void* driver_private, uint64_t sector_number, uint8_t output[BLOCK_DEVICE_READ_SIZE]),
    void (*block_write)(void* driver_private, uint64_t sector_number, uint8_t input[BLOCK_DEVICE_READ_SIZE])
);

//mounts /dev - you can add devices before running this if you want
void devfs_init();

#endif