#include "xhci_msd.h"
#include "physical_slab_allocation.h"
#include "xhci_driver.h"
#include "kern_libc.h"
#include "debugging.h"
#include "xhci_trb.h"
#include "memory.h"

static uint32_t next_free_tag = 69;

struct CommandBlockWrapper {
    uint32_t signature;//0x43425355
    uint32_t tag;//this value is repeated in the returned CSW
    uint32_t transfer_length;//bytes transferred, not counting CBW & CSW
    uint8_t
        reserved: 7,
        //0=>write, 1=>read
        direction: 1;
    uint8_t lun;
    uint8_t command_len;//up to 16
    //any multibyte fields in here are big endian?
    uint8_t command[16];
} __attribute__((packed));

struct CommandStatusWrapper {
    uint32_t signature;//0x53425355
    uint32_t tag;
    uint32_t data_residue;//how much data wasn't written?
    uint8_t status;
} __attribute__((packed));

static void debug_ring(const struct Ring *ring) {
    //print some of the preceeding items
    for(uint64_t i=0; i<ring->idx; i++) {
        struct TRB *x = &ring->trbs[i];
        printf("TRB: p %llu s %llu type %d cycle %d\n", x->parameter.raw, x->status.raw, x->status.trb_type, x->status.cycle_bit);
    }
    printf("\n");
}

void initialise_msd(struct xHCIData *xhci, uint8_t slot_number, struct ExternConfigDesc config_descriptor) {
    printf("initialising MSD:\n");

    assert(config_descriptor.num_interfaces == 1);
    const struct ExternIfDesc if_descriptor = config_descriptor.interfaces[0];
    struct XHCIDevice *device = &xhci->slots[slot_number];

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
    printf("MSD out is endpoint %d\nMSD in is endpoint %d\n", out.endpoint_num, in.endpoint_num);
    //enable the endpoints
    int in_index = calculate_endpoint_index(in.endpoint_num, true);
    device->endpoint_rings[in_index] = create_ring();
    struct Ring *in_ring = &device->endpoint_rings[in_index];

    int out_index = calculate_endpoint_index(out.endpoint_num, false);
    device->endpoint_rings[out_index] = create_ring();
    struct Ring *out_ring = &device->endpoint_rings[out_index];
    device->input_context->add_flags = 
        1 | 
        (1 << (in_index+1)) |
        (1 << (out_index+1));//+1 to skip the slot context?
    device->input_context->device_context.endpoint_context[in_index] = (struct EndpointContext) {
        .ep_type = 6,
        .cerr = 3,
        .max_packet_size = in.max_packet_size,
        .tr_dequeue_pointer_lo = in_ring->trbs_phys >> 4,
        .tr_dequeue_pointer_hi = in_ring->trbs_phys >> 32,
        .dcs = 1,
        .average_trb_length = 8,
    };
    device->input_context->device_context.endpoint_context[out_index] = (struct EndpointContext) {
        .ep_type = 2,
        .cerr = 3,
        .max_packet_size = in.max_packet_size,
        .tr_dequeue_pointer_lo = out_ring->trbs_phys >> 4,
        .tr_dequeue_pointer_hi = out_ring->trbs_phys >> 32,
        .dcs = 1,
        .average_trb_length = 8,
        // .lsa = 1,
    };
    set_context_entries(&device->input_context->device_context);
    update_input_context(xhci, slot_number, false);
    assert(device->device_context->endpoint_context[in_index].ep_state == 1);
    assert(device->device_context->endpoint_context[in_index].dcs == 1);
    assert(device->device_context->endpoint_context[out_index].ep_state == 1);
    assert(device->device_context->endpoint_context[out_index].dcs == 1);

    //TODO page 385 requests we fetch a different device qualifier, so that we know settings for the USB drive when it is in full and high speed

    make_request(xhci, NULL, (struct RequestTemplate) {
        .slot_number = slot_number,

        .direction = HostToDevice,
        .request_type = RequestTypeStandard,
        .recipient=RecipientDevice,
        .request = SET_CONFIGURATION,
        .value = 0x0001,//presumably the config number (one based?)?
        .index=0,
        .length=0
    });

    uint8_t max_lun;
    make_request(xhci, &max_lun, (struct RequestTemplate) {
        .slot_number = slot_number,

        .direction = DeviceToHost,
        .request_type = RequestTypeClass,
        .recipient = RecipientInterface,
        .request = GET_MAX_LUN,
        .value = 0x0000,
        .index=0,
        .length=1
    });//apparently if it returns STALL, then take LUN=0 and total count=1
    if(max_lun == 0xFF) max_lun = 0;//some devices do this
    assert(max_lun <= 15);
    max_lun++;//since zero based, add one
    assert(max_lun == 1);

    printf("doing inquiry\n");

    uint64_t inquiry_phys = malloc4k_phys();
    volatile struct CommandBlockWrapper *inquiry = phys_to_hhdm(inquiry_phys);
    *inquiry = (struct CommandBlockWrapper) {
        .signature=0x43425355,
        .tag = next_free_tag++,
        .transfer_length = 0x24,
        .direction = 1,
        .lun=0,
        .command_len = 6,
        .command = {
            0x12,//inquiry
            0x00,//no vital product data
            0x00,//page code
            0x00,
            0x24,//big endian length
            0x00//control
        }
    };

    uint64_t inquiry_response_phys = malloc4k_phys();
    volatile void *inquiry_response = phys_to_hhdm(inquiry_response_phys);
    uint64_t inquiry_status_phys = malloc4k_phys();
    volatile struct CommandStatusWrapper *inquiry_status = phys_to_hhdm(inquiry_status_phys);

    //ask for an inquiry
    enqueue_ring(out_ring, (struct TRB) {
        .parameter.raw = inquiry_phys,
        .status.normal = {
            .trb_transfer_length = 31,
            .trb_type = TRB_TYPE_NORMAL
        }
    });
    //here is where to put the response
    enqueue_ring(in_ring, (struct TRB) {
        .parameter.raw = inquiry_response_phys,
        .status.normal = {
            .trb_transfer_length = 0x24,
            .trb_type = TRB_TYPE_NORMAL
        }
    });
    //here is where to put the CSW
    enqueue_ring(in_ring, (struct TRB) {
        .parameter.raw = inquiry_status_phys,
        .status.normal = {
            .trb_transfer_length = 13,
            .trb_type = TRB_TYPE_NORMAL
        }
    });

    printf("out ring\n");
    debug_ring(out_ring);
    printf("in ring\n");
    debug_ring(in_ring);

    assert(device->device_context->endpoint_context[in_index].ep_state == 1);
    assert(device->device_context->endpoint_context[in_index].dcs == 1);
    assert(device->device_context->endpoint_context[out_index].ep_state == 1);
    assert(device->device_context->endpoint_context[out_index].dcs == 1);

    ring_doorbell(xhci, slot_number, out_index);
    ring_doorbell(xhci, slot_number, in_index);
    delay();
    printf("inquiry complete\n");

    printf("inquiry status: 0x%x\n", inquiry_status->signature);
    assert(inquiry_status->signature == 0x53425355);
    assert(inquiry_status->tag == inquiry->tag);
    assert(inquiry_status->data_residue == 0);//hope
    assert(inquiry_status->status == 0);

    //394
}