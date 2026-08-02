#ifndef EHCI_DRIVER_H
#define EHCI_DRIVER_H

#include "pci.h"

void initialise_ehci(struct PciDevice dev, struct BarInfo bar);

#endif