#ifndef XHCI_MSD_H
#define XHCI_MSD_H

#include "xhci_driver.h"

void initialise_msd(struct xHCIData *xhci, uint8_t slot_number, struct ExternConfigDesc config_descriptor);

#endif