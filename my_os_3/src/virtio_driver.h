#ifndef VIRTIO_DRIVER_H
#define VIRTIO_DRIVER_H

#include "fs_dev.h"
#include "pci.h"

void initialise_virtio(struct PciConfigurationHeader header, void* header_buffer, struct BarInfo bar_list[6]);

void virtio_block_write(void* block_dev_data, uint64_t sector_number, uint8_t input[BLOCK_DEVICE_READ_SIZE]);
void virtio_block_read(void* block_dev_data, uint64_t sector_number, uint8_t output[BLOCK_DEVICE_READ_SIZE]);

#endif