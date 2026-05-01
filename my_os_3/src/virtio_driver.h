#ifndef VIRTIO_DRIVER_H
#define VIRTIO_DRIVER_H

#include "pci.h"

void initialise_virtio(struct PciConfigurationHeader header, void* header_buffer, struct BarInfo bar_list[6]);

#define BLOCK_DEVICE_READ_SIZE 512
void virtio_block_write(uint64_t sector_number, uint8_t input[BLOCK_DEVICE_READ_SIZE]);
void virtio_block_read(uint64_t sector_number, uint8_t output[BLOCK_DEVICE_READ_SIZE]);

#endif