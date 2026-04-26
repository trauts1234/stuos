#ifndef VIRTIO_DRIVER_H
#define VIRTIO_DRIVER_H

#include "pci.h"

void initialise_virtio(struct PciConfigurationHeader header, void* header_buffer, struct BarInfo bar_list[6]);

#endif