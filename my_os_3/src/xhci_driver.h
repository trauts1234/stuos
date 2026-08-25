#ifndef XHCI_DRIVER_H
#define XHCI_DRIVER_H

#include "pci.h"

void initialise_xhci(struct PciDevice dev, struct PciData *dev_data);

#endif