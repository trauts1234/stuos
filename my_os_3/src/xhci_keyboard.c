#include "xhci_keyboard.h"
#include "physical_slab_allocation.h"
#include "xhci_driver.h"
#include "debugging.h"
#include <uapi/stddef.h>
#include "memory.h"
#include "kern_libc.h"


struct InputReport {
    uint8_t modifier_keys;
    uint8_t reserved_0;
    uint8_t keycodes[6];
};
struct OutputReport {
    uint8_t
        num_lock:1,
        caps_lock:1,
        scroll_lock:1,
        compose:1,
        kana:1,
        constant:3;
};

void initialise_keyboard(struct xHCIData *xhci, uint8_t slot_number, struct ExternConfigDesc config_descriptor, uint8_t interface_num) {
    const struct ExternIfDesc if_descriptor = config_descriptor.interfaces[interface_num];
    struct XHCIDevice *device = &xhci->slots[slot_number];
    
    make_request(xhci, NULL, (struct RequestTemplate) {
        .slot_number = slot_number,

        .direction = HostToDevice,
        .request_type = RequestTypeStandard,
        .recipient = RecipientDevice,
        .request = SET_CONFIGURATION,
        .value = config_descriptor.configuration_value,//lower byte is the configuration number
        .index = 0,
        .length = 0,
        .setup_transfer_type = NoDataStage
    });
    make_request(xhci, NULL, (struct RequestTemplate) {
        .slot_number = slot_number,
        .direction = HostToDevice,
        .request_type = RequestTypeClass,
        .recipient = RecipientInterface,
        .request = SET_IDLE,
        .value = 0,
        .index=interface_num,
        .length=0
    });
    make_request(xhci, NULL, (struct RequestTemplate) {
        .slot_number = slot_number,
        .direction = HostToDevice,
        .request_type = RequestTypeClass,
        .recipient = RecipientInterface,
        .request = SET_PROTOCOL,
        .value = 0,//0=>boot protocol, 1=>report protocol
        .index=interface_num,//interface number
        .length=0
    });

    assert(if_descriptor.num_endpoints == 1);
    struct ExternEpDesc in = if_descriptor.endpoints[0];
    int in_index = calculate_endpoint_index(in.endpoint_num, true);
    device->endpoint_rings[in_index] = create_ring();

    device->input_context->add_flags = 
        1 | 
        (1 << (in_index+1));//+1 to skip the slot context?
    device->input_context->device_context.endpoint_context[in_index] = (struct EndpointContext) {
        .ep_type = 6,
        .cerr = 3,
        .max_packet_size = in.max_packet_size,
        .tr_dequeue_pointer_lo = device->endpoint_rings[in_index].trbs_phys >> 4,
        .tr_dequeue_pointer_hi = device->endpoint_rings[in_index].trbs_phys >> 32,
        .dcs = 1,
        .average_trb_length = 8,
    };
    set_context_entries(&device->input_context->device_context);
    update_input_context(xhci, slot_number, false);
    assert(device->device_context->endpoint_context[in_index].ep_state == 1);
    assert(device->device_context->endpoint_context[in_index].dcs == 1);

    uint64_t rep_phys = malloc4k_phys();
    volatile struct InputReport *rep = phys_to_hhdm(rep_phys);
    while(1) {
        struct Ring *in_ring = &xhci->slots[slot_number].endpoint_rings[in_index];
        enqueue_ring(in_ring, (struct TRB) {
            .parameter.raw = rep_phys,
            .status.normal = {
                .trb_transfer_length = sizeof(struct InputReport),
                .trb_type = TRB_TYPE_NORMAL,
                .interrupt_on_completion = 1,
            }
        });
        ring_doorbell(xhci, slot_number, in_index);
        //check that sending command was successful
        struct TRB recv = fetch_and_extract(xhci, TRB_TYPE_TRANSFER);
        assert(recv.status.type_transfer.trb_type != 0);
        assert(recv.status.type_transfer.completion_code == 1);
        assert(recv.status.type_transfer.trb_transfer_length == 0);
        assert(recv.status.type_transfer.slot_id == slot_number);

        printf("got %x %x %x %x %x %x %x\n", rep->modifier_keys, rep->keycodes[0], rep->keycodes[1], rep->keycodes[2], rep->keycodes[3], rep->keycodes[4], rep->keycodes[5]);
    }
    
}