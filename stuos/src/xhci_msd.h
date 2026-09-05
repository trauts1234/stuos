#ifndef XHCI_MSD_H
#define XHCI_MSD_H

#include "xhci_driver.h"

//xhci should live forever, as it is stored in a heap struct
void initialise_msd(struct xHCIData *xhci, uint8_t slot_number, struct ExternConfigDesc config_descriptor, uint8_t interface_num);

#endif