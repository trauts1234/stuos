#include "xhci_msd.h"
#include "xhci_driver.h"
#include "kern_libc.h"
#include "debugging.h"

void initialise_msd(struct xHCIData *xhci, struct ExternConfigDesc config_descriptor) {
    printf("initialising MSD:\n");

    assert(config_descriptor.num_interfaces == 1);
    const struct ExternIfDesc if_descriptor = config_descriptor.interfaces[0];

    //should only be an in and out endpoint, however some devices may have a dead interrupt endpoint that must be ignored
    assert(if_descriptor.num_endpoints == 2);
    assert(if_descriptor.protocol == ExternIfProtocolBulkOnly);
    assert(if_descriptor.class_code == ExternIfClassMSD);
    assert(if_descriptor.sub_class == ExternIfSubClassSCSI);

    struct ExternEpDesc in, out;
    if(if_descriptor.endpoints[0].is_in) {
        in = if_descriptor.endpoints[0];
        out = if_descriptor.endpoints[1];
    } else {
        in = if_descriptor.endpoints[1];
        out = if_descriptor.endpoints[0];
    }
    assert(in.is_in);
    assert(!out.is_in);
    assert(in.transfer_type = EpTransferBulk);
    assert(out.transfer_type = EpTransferBulk);

    //TODO page 385 requests we fetch a different device qualifier, so that we know settings for the USB drive when it is in full and high speed

    //387
}