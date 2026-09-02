#ifndef XHCI_MSD_H
#define XHCI_MSD_H

#include "xhci_driver.h"

struct MassStorageDeviceXHCI {
    //either 10,12,or 16
    //ensure to choose read(10), read(12), read(16) based on this, etc.
    uint8_t size_class;
};

void initialise_msd(struct xHCIData *xhci, uint8_t slot_number, struct ExternConfigDesc config_descriptor);

#endif