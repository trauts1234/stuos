#include "xhci_keyboard.h"
#include "physical_slab_allocation.h"
#include "xhci_driver.h"
#include "debugging.h"
#include <uapi/stdbool.h>
#include <uapi/stddef.h>
#include "memory.h"
#include "kern_libc.h"
#include "xhci_trb.h"
#include "tty.h"

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

struct PacketRecvState {
    uint64_t rep_phys;
    int in_index;

    struct InputReport previous_report;
    bool shift_currently_pressed;
    bool capslock_on;
};

static const char key_map[256][2] = {
    [0x04] = { 'a', 'A' },
    [0x05] = { 'b', 'B' },
    [0x06] = { 'c', 'C' },
    [0x07] = { 'd', 'D' },
    [0x08] = { 'e', 'E' },
    [0x09] = { 'f', 'F' },
    [0x0A] = { 'g', 'G' },
    [0x0B] = { 'h', 'H' },
    [0x0C] = { 'i', 'I' },
    [0x0D] = { 'j', 'J' },
    [0x0E] = { 'k', 'K' },
    [0x0F] = { 'l', 'L' },
    [0x10] = { 'm', 'M' },
    [0x11] = { 'n', 'N' },
    [0x12] = { 'o', 'O' },
    [0x13] = { 'p', 'P' },
    [0x14] = { 'q', 'Q' },
    [0x15] = { 'r', 'R' },
    [0x16] = { 's', 'S' },
    [0x17] = { 't', 'T' },
    [0x18] = { 'u', 'U' },
    [0x19] = { 'v', 'V' },
    [0x1A] = { 'w', 'W' },
    [0x1B] = { 'x', 'X' },
    [0x1C] = { 'y', 'Y' },
    [0x1D] = { 'z', 'Z' },
    [0x1E] = { '1', '!' },
    [0x1F] = { '2', '@' },
    [0x20] = { '3', '#' },
    [0x21] = { '4', '$' },
    [0x22] = { '5', '%' },
    [0x23] = { '6', '^' },
    [0x24] = { '7', '&' },
    [0x25] = { '8', '*' },
    [0x26] = { '9', '(' },
    [0x27] = { '0', ')' },
    [0x28] = {'\n', '\n'},
    [0x2A] = {'\b', '\b'},
    [0x2B] = {'\t', '\t'},
    [0x2C] = { ' ', ' ' },
    [0x2D] = { '-', '_' },
    [0x2E] = { '=', '+' },
    [0x2F] = { '[', '{' },
    [0x30] = { ']', '}' },
    [0x31] = {'\\', '|' },
    [0x32] = { '#', '~' },
    [0x33] = { ';', ':' },
    [0x34] = {'\'', '"' },
    [0x35] = { '`', '~' },
    [0x36] = { ',', '<' },
    [0x37] = { '.', '>' },
    [0x38] = { '/', '?' },
};

static char handle_keypress(uint8_t keycode, bool is_break, struct PacketRecvState *state)
{
    if(keycode == 0xE1 || keycode == 0xE5) {
        state->shift_currently_pressed = !is_break;
    }
    if(keycode == 0x39) {
        state->capslock_on = !is_break;
    }
    if(is_break) return 0;

    char character = key_map[keycode][state->shift_currently_pressed];
    if (state->capslock_on) {
        character = toupper(character);
    }

    return character;

}

static void packet_recv(struct xHCIData *xhci, struct TRB recv) {
    assert(recv.status.type_transfer.trb_type == TRB_TYPE_TRANSFER);
    assert(recv.status.type_transfer.completion_code == 1);
    assert(recv.status.type_transfer.trb_transfer_length == 0);

    uint8_t slot_number = recv.status.type_transfer.slot_id;
    struct XHCIDevice *device = &xhci->slots[slot_number];
    struct PacketRecvState *state = device->interrupt_handler_data;

    volatile struct InputReport *rep = phys_to_hhdm(state->rep_phys);

    uint8_t codes_sparse[256] = {};
    //fill in new keys with 1
    for(int i=0; i<6; i++) {
        uint8_t keycode = rep->keycodes[i];
        if(keycode == 0) continue;
        codes_sparse[keycode] = 1;
    }
    for(int i=0; i<6; i++) {
        uint8_t old_keycode = state->previous_report.keycodes[i];
        if(old_keycode == 0) continue;
        if(codes_sparse[old_keycode] == 0) {
            //currently not pressed, used to be pressed
            tty_provide_stdin(handle_keypress(old_keycode, true, state));
        }
        codes_sparse[old_keycode] = 2;
    }
    for(int i=0; i<6; i++) {
        uint8_t keycode = rep->keycodes[i];
        if(keycode == 0) continue;
        if(codes_sparse[keycode] == 1) {
            //used to be unpressed, is currently pressed
            tty_provide_stdin(handle_keypress(keycode, false, state));
        }
    }
    state->previous_report = *rep;

    struct Ring *in_ring = &xhci->slots[slot_number].endpoint_rings[state->in_index];
    enqueue_ring(in_ring, (struct TRB) {
        .parameter.raw = state->rep_phys,
        .status.normal = {
            .trb_transfer_length = sizeof(struct InputReport),
            .trb_type = TRB_TYPE_NORMAL,
            .interrupt_on_completion = 1,
        }
    });
    ring_doorbell(xhci, slot_number, state->in_index);
}

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

    struct PacketRecvState *state = malloc(sizeof(struct PacketRecvState));
    *state = (struct PacketRecvState) {
        .rep_phys = malloc4k_phys(),
        .in_index = in_index
    };
    device->interrupt_handler_data = state;
    device->interrupt_trb_handler = packet_recv;

    struct Ring *in_ring = &xhci->slots[slot_number].endpoint_rings[in_index];
    enqueue_ring(in_ring, (struct TRB) {
        .parameter.raw = state->rep_phys,
        .status.normal = {
            .trb_transfer_length = sizeof(struct InputReport),
            .trb_type = TRB_TYPE_NORMAL,
            .interrupt_on_completion = 1,
        }
    });
    ring_doorbell(xhci, slot_number, in_index);
    
}