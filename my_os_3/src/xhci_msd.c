#include "xhci_msd.h"
#include "xhci_driver.h"
#include "kern_libc.h"

void initialise_msd(struct xHCIData *xhci, struct ExternConfigDesc config_descriptor) {
    printf("initialising MSD:\nnum interfaces: %d\n", config_descriptor.num_interfaces);
    for(int i=0; i<config_descriptor.num_interfaces; i++) {
        struct ExternIfDesc interface = config_descriptor.interfaces[i];
        printf("interface %d:\n  endpoints: %d\n  sub class: 0x%x\n  protocol: 0x%x\n", i, interface.num_endpoints, interface.sub_class, interface.protocol);
        for(int j=0; j<interface.num_endpoints; j++) {
            struct ExternEpDesc ep = interface.endpoints[j];
            printf("  endpoint %d:\n    address: 0x%x\n    attributes: 0x%x\n    max packet size: %d\n", j, ep.address, ep.attributes, ep.max_packet_size);
        }
    }
}