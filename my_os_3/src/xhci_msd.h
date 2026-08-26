#ifndef XHCI_MSD_H
#define XHCI_MSD_H

#include "xhci_driver.h"

void initialise_msd(struct xHCIData *xhci, struct ExternConfigDesc config_descriptor);

#endif