#include "xhci_driver.h"
#include "pci.h"
#include "physical_slab_allocation.h"
#include "uapi/stdint.h"
#include "kern_libc.h"
#include "debugging.h"
#include "memory.h"
#include "xhci_msd.h"
#include "xhci_trb.h"
#include <uapi/stdbool.h>

//TODO I am missing tons of volatile in here!!!!!

//https://www.intel.com/content/dam/www/public/us/en/documents/technical-specifications/extensible-host-controler-interface-usb-xhci.pdf

struct EventRingSegmentTableEntry {
    uint64_t ring_segment_base_address;
    uint16_t ring_segment_size;
    uint16_t reserved_0[3];
};

struct ConfigurationDescriptor {
    uint8_t length;
    uint8_t type;
    uint16_t total_length;
    uint8_t num_interfaces;
    uint8_t config_val;
    uint8_t config_string;
    uint8_t attributes;
    uint8_t max_power;
} __attribute__((packed));

struct InterfaceDescriptor {
    uint8_t
        length,
        type,
        interface_num,
        alternate_set,
        num_endpoints,
        class_code,
        sub_class,
        protocol,
        interface_str;
} __attribute__((packed));
struct EndpointDescriptor {
    uint8_t
        length,
        type,
        endpoint_num: 4,
        reserved_0: 3,
        direction: 1,
        transfer_type: 2,
        reserved_1: 6;
    uint16_t max_packet_size;
    uint8_t interval;
} __attribute__((packed));

//offsets into BAR0

//capability register length (read lowest byte only)
#define CAPLENGTH_AND_VERSION_OFFSET 0
uint8_t CAPLENGTH_CAPLENGTH(uint32_t caplength_and_version) {return caplength_and_version & 0xFF;}
//structural params
#define HCSPARAMS1_OFFSET 4
uint8_t HCSPARAMS1_MAXPORTS(uint32_t hccparams1) {return (hccparams1 >> 24) & 0xFF;}
#define HCSPARAMS2_OFFSET 8
uint16_t HCSPARAMS2_required_scratchpad_buffers(uint32_t hcsparams2) {return ((hcsparams2 >> 27) & 0b11111) | ((hcsparams2 >> 21) & 0b1111100000);}
#define HCSPARAMS3_OFFSET 0xC
//capability params
#define HCCPARAMS1_OFFSET 0x10
uint32_t HCCPARAMS1_xecp(uint32_t hccparams1) {return (hccparams1 >> 16) << 2;}
bool HCCPARAMS1_64BIT(uint32_t hccparams1) {return hccparams1 & 1;}
bool HCCPARAMS1_csz(uint32_t hccparams1) {return hccparams1 & (1<<2);}
#define DBOFF 0x14
#define RTSOFF_OFFSET 0x18

//offsets into BAR0 + CAPLENGTH
#define USBCMD_OFFSET 0
#define USBSTS_OFFSET 4
bool USBSTS_hchalted(uint32_t usbsts) {return usbsts & 1;}
bool USBSTS_error(uint32_t usbsts) {return (usbsts & (1 << 2)) || (usbsts & (1 << 12));}
bool USBSTS_cnr(uint32_t usbsts) {return usbsts & (1 << 11);}
#define DNCTRL_OFFSET 0x14
#define CRCR_OFFSET 0x18
#define CONFIG_OFFSET 0x38
#define DCBAAP_OFFSET 0x30
#define PORTSC_OFFSET(port_index) (0x400 + 0x10*(port_index))
uint8_t PORTSC_speed(uint32_t portsc) {return (portsc >> 10) & 0xF;}

//offsets into BAR0 + RTSOFF
#define IR_IMAN_OFFSET(interrupt_index) (0x20 + 32*interrupt_index)
#define IR_IMOD_OFFSET(interrupt_index) (0x24 + 32*interrupt_index)
#define IR_ERSTSZ_OFFSET(interrupt_index) (0x28 + 32*interrupt_index)
//64 bit!
#define IR_ERSTBA_OFFSET(interrupt_index) (0x30 + 32*interrupt_index)
//64 bit!
#define IR_ERDP_OFFSET(interrupt_index) (0x38 + 32*interrupt_index)

//offsets into BAR0 + DBOFF
#define DOORBELL_OFFSET(doorbell_index) (4*doorbell_index)

#define USBSTS_EINT (1 << 3)
#define USBCMD_RS 1
#define USBCMD_HCRST (1 << 1)
#define USBCMD_IE (1 << 2)
#define PORTSC_CCS 1
#define PORTSC_PED (1 << 1)
#define PORTSC_PR (1 << 4)
#define PORTSC_PP (1 << 9)
#define PORTSC_CSC (1 << 17)
#define PORTSC_PEC (1 << 18)
#define PORTSC_WRC (1 << 19)
#define PORTSC_PRC (1 << 21)
#define PORTSC_WPR (1 << 31)
#define IMAN_INTERRUPT_ENABLE (1 << 1)
#define IMAN_INTERRUPT_PENDING 1
#define ERDP_EHB (1 << 3)

#define USBLEGSUP_BIOS_OWNED_SEMAPHORE (1 << 16)
#define USBLEGSUP_OS_OWNED_SEMAPHORE (1 << 24)

void delay() {
    __asm__ volatile("mfence" ::: "memory");
    for(uint64_t i=0;i<30000;i++) {
        __asm("nop");
    }
}

static void ack_irq(struct xHCIData *data, uint8_t interrupt_index) {
    uint32_t usbsts = read_bar_32(data->bar, data->cap_length + USBSTS_OFFSET);
    usbsts |= USBSTS_EINT;
    write_bar_32(data->bar, usbsts, data->cap_length + USBSTS_OFFSET);

    uint32_t iman = read_bar_32(data->bar, data->rts_offset + IR_IMAN_OFFSET(interrupt_index));
    iman |= IMAN_INTERRUPT_PENDING;//write 1 to reset the bit to a 0
    write_bar_32(data->bar, iman, data->rts_offset + IR_IMAN_OFFSET(interrupt_index));
}

//sets the event ring dequeue pointer to its new value, so that the xHCI controller can write new TRBs
//do this after dequeueing from the event ring
static void update_erdp(struct xHCIData *data, bool clear_ehb) {
    uint64_t erdp = (clear_ehb ? ERDP_EHB : 0ull) | (data->event_ring.trbs_phys + sizeof(struct TRB) * data->event_ring.idx);
    write_bar_32(data->bar, erdp & 0xFFFFFFFF, data->rts_offset + IR_ERDP_OFFSET(0));
    write_bar_32(data->bar, erdp >> 32, data->rts_offset + IR_ERDP_OFFSET(0) + 4);
    delay();
}

//returns 1 for endpoint 1 out, 2 for endpoint 1 in, 3 for endpoint 2 out, etc.
uint8_t calculate_endpoint_index(uint8_t endpoint_num, bool is_in) {
    assert(endpoint_num != 0);
    assert(endpoint_num <= 15);
    return 2*endpoint_num + is_in - 1;
}

void ring_doorbell(struct xHCIData *data, uint8_t port, uint8_t endpoint_index) {
    assert(endpoint_index < 31);
    write_bar_32(data->bar, endpoint_index + 1, data->db_offset + DOORBELL_OFFSET(port));//+1 since the command doorbell uses 0
    delay();
}

static void ring_command_doorbell(struct xHCIData *data) {
    write_bar_32(data->bar, 0, data->db_offset + DOORBELL_OFFSET(0));
    delay();
}

static void insert_to_list(struct TRB list[TRB_LIST_LEN], struct TRB to_insert) {
    for(int i=0;;i++) {
        assert(i < TRB_LIST_LEN)
        if(list[i].status.trb_type == 0) {
            list[i] = to_insert;
            return;
        }
    }
}
//returns {} on failure
static struct TRB extract_from_list(struct TRB list[TRB_LIST_LEN], uint8_t requested_trb_type) {
    assert(requested_trb_type != 0);
    assert(requested_trb_type <= 39);//this may exclude vendor defined messages :(
    for(int i=0; i<TRB_LIST_LEN ;i++) {
        if(list[i].status.trb_type == requested_trb_type) {
            struct TRB result = list[i];
            list[i].status.trb_type = 0;
            return result;
        }
    }
    return (struct TRB) {};
}

static void xhci_handle_responses(struct xHCIData *data) {
    //try and read some data
    struct TRB recv = {};
    while(dequeue_ring(&data->event_ring, &recv) == 0) {
        update_erdp(data, true);
        switch(recv.status.trb_type) {
            case TRB_TYPE_TRANSFER:
            case TRB_TYPE_CMD_COMPLETION:
            case TRB_TYPE_PORT_STS_CHANGE:
            case TRB_TYPE_BANDWIDTH_REQUEST:
            case TRB_TYPE_DOORBELL:
            case TRB_TYPE_HOST_CONTROLLER:
            case TRB_TYPE_DEVICE_NOTIFICATION:
            case TRB_TYPE_MFINDEX_WRAP:
            break;
            
            default:
            printf("ERR: unknown trb type 0x%x\n", recv.status.trb_type);
            HCF
            break;
        }

        insert_to_list(data->unhandled_events, recv);
    }
}

struct TRB fetch_and_extract(struct xHCIData *data, uint8_t requested_trb_type) {
    struct TRB result;
    while((result = extract_from_list(data->unhandled_events, requested_trb_type)).status.trb_type == 0) {
        xhci_handle_responses(data);
    }
    return result;
}

//calls SET_ADDRESS with the block bit zeroed
static void set_input_context(struct xHCIData *xhci, uint8_t slot_id, bool block_set_address_request) {
    enqueue_ring(&xhci->command_ring, (struct TRB) {
        .parameter.raw = xhci->slots[slot_id].input_context_phys,
        .status.set_address = {
            .block_set_address_request = block_set_address_request,
            .trb_type = TRB_TYPE_SET_ADDRESS,
            .slot_id = slot_id,
        }
    });
    ring_command_doorbell(xhci);
    struct TRB recv = fetch_and_extract(xhci, TRB_TYPE_CMD_COMPLETION);
    assert(recv.status.command_completion.trb_type != 0);
    assert(recv.status.command_completion.completion_code == 1);
    assert(recv.status.command_completion.slot_id == slot_id);
}

void update_input_context(struct xHCIData *xhci, uint8_t slot_id, bool am_modifying_existing_endpoints) {
    enqueue_ring(&xhci->command_ring, (struct TRB) {
        .parameter.raw = xhci->slots[slot_id].input_context_phys,
        //use the set_address union (todo may need custom union to access the configure endpoint deconfigure bit)
        .status.set_address = {
            .trb_type = am_modifying_existing_endpoints ? TRB_TYPE_EVALUATE_CONTEXT: TRB_TYPE_CONFIGURE_ENDPOINT,
            .slot_id = slot_id,
        }
    });
    ring_command_doorbell(xhci);
    struct TRB recv = fetch_and_extract(xhci, TRB_TYPE_CMD_COMPLETION);
    assert(recv.status.command_completion.trb_type != 0);
    assert(recv.status.command_completion.completion_code == 1);
}

void make_request(struct xHCIData *xhci, void *output, struct RequestTemplate request) {
    struct Ring *ep0_transfer = &xhci->slots[request.slot_number].endpoint_rings[0];
    enqueue_ring(ep0_transfer, (struct TRB) {
        .parameter.device_request = {
            .recipient = request.recipient,
            .type = request.request_type,
            .direction = request.direction,
            .request = request.request,
            .value = request.value,
            .index = request.length,
            .length = request.length
        },
        .status.device_request = {
            .trb_transfer_length = 8,
            .interrupter_target = 0,
            .interrupt_on_completion = 0,
            .immediate_data = 1,
            .trb_type = TRB_TYPE_SETUP_STAGE,
            .transfer_type = 3,
        }
    });
    //data stage
    uint64_t num_pages = round_up_pages(request.length);
    uint64_t data_buffer_phys = malloc_contiguous_phys(num_pages);
    void *data_buffer_virt = phys_to_hhdm(data_buffer_phys);
    memset(data_buffer_virt, 0, PAGE_SIZE);//todo * num_pages ?
    enqueue_ring(ep0_transfer, (struct TRB) {
        .parameter.raw = data_buffer_phys,
        .status.data_stage = {
            .trb_transfer_length = request.length,
            .td_size = 0,
            .interrupter_target = 0,//this is an IR_IMOD index, not an IDT index
            .evaluate_next_trb = 1,
            .chain_bit = 1,
            .trb_type = TRB_TYPE_DATA_STAGE,
            .direction = 1,
        }
    });

    enqueue_ring(ep0_transfer, (struct TRB) {
        .parameter.raw = 0,
        .status.status_stage = {
            .interrupter_target = 0,
            .chain_bit = 1,
            .trb_type = TRB_TYPE_STATUS_STAGE,
        }
    });
    enqueue_ring(ep0_transfer, (struct TRB) {
        .parameter.raw = 0xBEEFBEEFBEEFBEEF,
        .status.event_data = {
            .interrupt_target = 0,
            .interrupt_on_completion = 1,
            .trb_type = TRB_TYPE_EVENT_DATA
        }
    });
    ring_doorbell(xhci, request.slot_number, 0);
    delay();
    struct TRB recv = fetch_and_extract(xhci, TRB_TYPE_TRANSFER);
    assert(recv.status.type_transfer.trb_type != 0);
    // assert(recv.status.type_transfer.trb_transfer_length == num_bytes);
    assert(recv.status.type_transfer.completion_code == 1);
    assert(recv.status.type_transfer.slot_id == request.slot_number);
    assert(recv.status.type_transfer.event_data);//ensures that paramater contains raw data
    assert(recv.parameter.raw == 0xBEEFBEEFBEEFBEEF);

    memcpy(output, data_buffer_virt, request.length);
    free_contiguous_phys(data_buffer_phys, num_pages);
}

void set_context_entries(struct DeviceContext *device_context) {
    //count down based on the number of unused trailing entries
    uint8_t index_of_last_valid_entry = 30;
    while(device_context->endpoint_context[index_of_last_valid_entry].max_packet_size == 0) {
        assert(index_of_last_valid_entry != 0);
        index_of_last_valid_entry--;//found an empty slot
    }

    device_context->context_entries = index_of_last_valid_entry;
    printf("set context entries %d\n", index_of_last_valid_entry);
}

static void send_control_transfer(struct xHCIData *xhci, uint8_t slot_number, uint8_t descriptor_type, uint8_t descriptor_index, void *output, uint64_t num_bytes) {
    struct RequestTemplate rq = {
        .slot_number = slot_number,

        .direction = 1,
        .request_type = RequestTypeStandard,
        .recipient = RecipientDevice,
        .request = GET_DESCRIPTOR,
        .descriptor_index = descriptor_index,
        .descriptor_type = descriptor_type,
        .index = 0,
        .length = num_bytes
    };

    make_request(xhci, output, rq);
}

static void read_string_descriptor(struct xHCIData *xhci, uint8_t slot_id, uint8_t string_index) {
    assert(string_index != 0);//0 is the language descriptor
    #define MAX_STRING_DESCRIPTOR_LENGTH ((UINT8_MAX - 2)/2)
    struct String {
        //how many bytes the struct is (includes length and type)
        uint8_t length;
        uint8_t type;
        uint16_t unicode[MAX_STRING_DESCRIPTOR_LENGTH];
    } str;

    //read length and type
    send_control_transfer(xhci, slot_id, DESCRIPTOR_TYPE_STRING, string_index, &str, 2);
    assert(str.length % 2 == 0);
    assert(str.length >= 2);
    assert(str.type == DESCRIPTOR_TYPE_STRING);
    //read whole string
    send_control_transfer(xhci, slot_id, DESCRIPTOR_TYPE_STRING, string_index, &str, str.length);

    uint64_t num_chars = (str.length-2)/2;
    char ascii[MAX_STRING_DESCRIPTOR_LENGTH+1] = {};
    for(uint64_t i=0; i<num_chars; i++) {
        assert((str.unicode[i] & 0xFF00) == 0);
        ascii[i] = str.unicode[i];
    }
    printf("string descriptor %d: %s\n", string_index, ascii);
}
static struct ExternConfigDesc read_configuration_descriptor(struct xHCIData *xhci, uint8_t slot_id, uint8_t configuration_index) {
    struct ExternConfigDesc result = {};
    
    struct ConfigurationDescriptor initial_config;
    //read initial fields (9 bytes)
    send_control_transfer(xhci, slot_id, DESCRIPTOR_TYPE_CONFIGURATION, configuration_index, &initial_config, sizeof(initial_config));
    assert(initial_config.length == sizeof(initial_config));
    assert(initial_config.type == DESCRIPTOR_TYPE_CONFIGURATION);
    assert(initial_config.total_length >= initial_config.length);
    result.num_interfaces = initial_config.num_interfaces;

    void *data = malloc(initial_config.total_length);
    const void *end = data + initial_config.total_length;
    //read the rest
    send_control_transfer(xhci, slot_id, DESCRIPTOR_TYPE_CONFIGURATION, configuration_index, data, initial_config.total_length);
    struct InterfaceDescriptor *curr_interface = data + initial_config.length;//this pointer is always valid
    while((void*)curr_interface + 2 < end) {
        assert(curr_interface->type == DESCRIPTOR_TYPE_INTERFACE);
        assert(curr_interface->num_endpoints <= 16);
        result.interfaces[curr_interface->interface_num] = (struct ExternIfDesc) {
            .num_endpoints = curr_interface->num_endpoints,
            .class_code = curr_interface->class_code,
            .sub_class = curr_interface->sub_class,
            .protocol = curr_interface->protocol
        };

        uint8_t endpoint_index = 0;
        struct EndpointDescriptor *curr_endpoint = (void*)curr_interface + curr_interface->length;//immediately after
        while((void*)curr_endpoint + 2 < end && curr_endpoint->type != DESCRIPTOR_TYPE_INTERFACE) {
            if(curr_endpoint->type == DESCRIPTOR_TYPE_ENDPOINT) {
                assert(curr_endpoint->interval == 0);//means no polling needed
                printf("endpoint index %d of %d\n", endpoint_index, curr_interface->num_endpoints);
                assert(endpoint_index < curr_interface->num_endpoints);
                result.interfaces[curr_interface->interface_num].endpoints[endpoint_index++] = (struct ExternEpDesc) {
                    .endpoint_num = curr_endpoint->endpoint_num,
                    .is_in = curr_endpoint->direction,//1 means in
                    .transfer_type = curr_endpoint->transfer_type,
                    .max_packet_size = curr_endpoint->max_packet_size
                };
            }

            curr_endpoint = (void*)curr_endpoint + curr_endpoint->length;
        }

        curr_interface = (void*)curr_endpoint;
    }
    free(data);

    return result;
}

//does nothing if port is empty
static void initialise_port(struct xHCIData *xhci, uint64_t *dcbaa_virt, uint8_t port_idx) {
    uint32_t portsc = read_bar_32(xhci->bar, xhci->cap_length + PORTSC_OFFSET(port_idx));
    assert(portsc & PORTSC_PP);//TODO power on the port if it isn't already

    if(!((portsc & PORTSC_CCS) && (portsc & PORTSC_CSC))) return;//port is empty

    printf("device found on port index %u\n", port_idx); 
    bool is_usb3 = xhci->port_is_usb3[port_idx];
    //write to clear some status bits?
    portsc |= PORTSC_CSC | PORTSC_PEC | PORTSC_PRC;
    //reset or warm port reset
    portsc |= is_usb3 ? PORTSC_WPR : PORTSC_PR;
    write_bar_32(xhci->bar, portsc, xhci->cap_length + PORTSC_OFFSET(port_idx));

    delay();
    portsc = read_bar_32(xhci->bar, xhci->cap_length + PORTSC_OFFSET(port_idx));
    //wait for reset completion
    while((is_usb3 && !(portsc & PORTSC_WRC)) || (!is_usb3 && !(portsc & PORTSC_PRC))) {
        portsc = read_bar_32(xhci->bar, xhci->cap_length + PORTSC_OFFSET(port_idx));
    }
    delay();

    //write to reset some flags
    portsc |= PORTSC_PRC | PORTSC_WRC | PORTSC_CSC | PORTSC_PEC;
    //writing 1 would clear this flag, so we write 0
    portsc &= ~(uint32_t)PORTSC_PED;
    write_bar_32(xhci->bar, portsc, xhci->cap_length + PORTSC_OFFSET(port_idx));
    delay();

    portsc = read_bar_32(xhci->bar, xhci->cap_length + PORTSC_OFFSET(port_idx));
    assert(portsc & PORTSC_PED);

    //get a device slot
    enqueue_ring(&xhci->command_ring, (struct TRB) {
        .status.trb_type = TRB_TYPE_ENABLE_SLOT
    });
    ring_command_doorbell(xhci);
    struct TRB recv = fetch_and_extract(xhci, TRB_TYPE_CMD_COMPLETION);
    assert(recv.status.command_completion.trb_type != 0);
    assert(recv.status.command_completion.completion_code == 1);
    const uint8_t slot_number = recv.status.command_completion.slot_id;
    assert(slot_number != 0);

    struct XHCIDevice *curr_device = &xhci->slots[slot_number];
    assert(xhci->slots[slot_number].one_based_root_port == 0);//slot must be unused
    curr_device->one_based_root_port = port_idx;
    //create a ring for this port's endpoint 0
    curr_device->endpoint_rings[0] = create_ring();

    //create device context
    uint64_t device_context_phys = malloc4k_phys();
    struct DeviceContext *device_context = phys_to_hhdm(device_context_phys);
    curr_device->device_context = device_context;
    memset(device_context, 0, sizeof(struct DeviceContext));
    dcbaa_virt[slot_number] = device_context_phys;

    //create input context, which is a second device context plus some extra
    uint64_t input_context_phys = malloc4k_phys();
    struct InputContext *input_context = phys_to_hhdm(input_context_phys);
    memset(input_context, 0, sizeof(struct InputContext));
    curr_device->input_context = input_context;
    curr_device->input_context_phys = input_context_phys;

    input_context->add_flags |= 0b11;//enable the slot context and control EP0
    input_context->device_context.speed = PORTSC_speed(portsc);
    input_context->device_context.root_hub_port_num = port_idx+1;
    input_context->device_context.interrupt_target = 0;//interrupt IR_IMOD index zero
    //set up the mandatory endpoint context zero
    input_context->device_context.endpoint_context[0] = (struct EndpointContext) {
        .ep_type = 4,
        .cerr = 3,
        .max_packet_size = (const uint16_t[]) {8,64,8, 64,512,512}[PORTSC_speed(portsc)],
        .tr_dequeue_pointer_lo = curr_device->endpoint_rings[0].trbs_phys >> 4,
        .tr_dequeue_pointer_hi = curr_device->endpoint_rings[0].trbs_phys >> 32,
        .dcs = 1,
        .average_trb_length = 8,
    };
    set_context_entries(&input_context->device_context);
    //initialise something
    set_input_context(xhci, slot_number, true);
    assert(device_context->slot_state == 1);
    assert(device_context->device_address == 0);
    assert(device_context->endpoint_context[0].ep_state == 1);

    set_input_context(xhci, slot_number, false);
    assert(device_context->slot_state == 2);

    struct DeviceDescriptor device_descriptor;
    send_control_transfer(xhci, slot_number, 1, 0, &device_descriptor, 8);
    input_context->device_context.endpoint_context[0] = device_context->endpoint_context[0];
    input_context->add_flags = 0b10;
    input_context->device_context.endpoint_context[0].max_packet_size = (device_descriptor.release_bcd_maj >= 0x03) ? (1<<device_descriptor.max_packet_size) : device_descriptor.max_packet_size;//usb 3 decided to be special and think that 9 equals 512
    update_input_context(xhci, slot_number, true);
    //read the full data buffer
    assert(device_descriptor.length == 18);
    send_control_transfer(xhci, slot_number, 1, 0, &device_descriptor, 18);

    assert(device_descriptor.device_class == 0);//unknown type
    
    if(device_descriptor.product) {
        read_string_descriptor(xhci, slot_number, device_descriptor.product);
    }
    if(device_descriptor.serial_num) {
        read_string_descriptor(xhci, slot_number, device_descriptor.serial_num);
    }

    //337, 383

    assert(device_descriptor.configurations == 1);// only handle situations with one configuration for now
    const struct ExternConfigDesc config_descriptor = read_configuration_descriptor(xhci, slot_number, 0);

    if (
        config_descriptor.num_interfaces == 1
    ) {
        const struct ExternIfDesc desc = config_descriptor.interfaces[0];

        if(desc.protocol == ExternIfProtocolBulkOnly && desc.sub_class == ExternIfSubClassSCSI) {
            initialise_msd(xhci, slot_number, config_descriptor);
        }
    }
}

void initialise_xhci(struct PciDevice dev, struct PciData *dev_data) {
    struct BarInfo bar = dev_data->bar_list[0];
    uint8_t cap_length = CAPLENGTH_CAPLENGTH(read_bar_32(bar, CAPLENGTH_AND_VERSION_OFFSET));
    uint32_t rts_offset = read_bar_32(bar, RTSOFF_OFFSET);
    uint32_t db_offset = read_bar_32(bar, DBOFF);
    const uint32_t hccparams1 = read_bar_32(bar, HCCPARAMS1_OFFSET);
    const uint32_t hcsparams1 = read_bar_32(bar, HCSPARAMS1_OFFSET);
    const uint32_t hcsparams2 = read_bar_32(bar, HCSPARAMS2_OFFSET);
    const uint8_t max_ports = HCSPARAMS1_MAXPORTS(hcsparams1);
    const uint16_t scratchpad_required = HCSPARAMS2_required_scratchpad_buffers(hcsparams2);

    struct xHCIData xhci = {
        .bar = bar,
        .cap_length = cap_length,
        .rts_offset = rts_offset,
        .db_offset = db_offset
    };

    for(uint32_t xecp = HCCPARAMS1_xecp(hccparams1); xecp;) {
        uint32_t data = read_bar_32(bar, xecp);
        switch(data & 0xFF) {
            
            case 1://USBLEGSUP
            uint32_t usblegsup = data;
            if(usblegsup & USBLEGSUP_BIOS_OWNED_SEMAPHORE) {
                printf("BIOS OWNED!!!\n");
                usblegsup |= USBLEGSUP_OS_OWNED_SEMAPHORE;
                write_bar_32(bar, usblegsup, xecp);
                while(read_bar_32(bar, xecp) & USBLEGSUP_BIOS_OWNED_SEMAPHORE);
            }
            break;

            case 2:
            const uint32_t second_dword = read_bar_32(bar, xecp+4);
            const uint32_t third_dword = read_bar_32(bar, xecp+8);

            assert(memcmp(&second_dword, "USB ", 4) == 0);
            uint8_t usb_maj = data >> 24;
            uint8_t usb_min = (data >> 16) & 0xFF;
            uint8_t port_index = (third_dword & 0xFF)-1;//port offset starts at 1 for some reason
            uint8_t port_count = (third_dword >> 8) & 0xFF;
            printf("found %d usb %d.%d slots at index %d and onward\n", port_count, usb_maj, usb_min, port_index);
            for(uint8_t i=port_index; i<port_index+port_count; i++) {
                xhci.port_is_usb3[i] = (usb_maj == 3);
            }

            default:
            break;
        }

        xecp = ((data >> 8) & 0xFF) << 2;
    }

    //only got code for 64 bit
    assert(HCCPARAMS1_64BIT(hccparams1));
    //only got code for 32 byte context data structs
    assert(!HCCPARAMS1_csz(hccparams1));

    //halt the xhci chip
    write_bar_32(bar, 0, cap_length + USBCMD_OFFSET);
    while(!USBSTS_hchalted(read_bar_32(bar, cap_length + USBSTS_OFFSET)));

    //reset the xhci chip
    write_bar_32(bar, USBCMD_HCRST, cap_length + USBCMD_OFFSET);
    while(read_bar_32(bar, cap_length + USBCMD_OFFSET) & USBCMD_HCRST);

    //enable all notifications
    write_bar_32(bar, 0x0002, cap_length + DNCTRL_OFFSET);//some suggest 0xFFFF, but standard says 0x2
    //set max ports to the max number of ports?
    write_bar_32(bar, max_ports, cap_length + CONFIG_OFFSET);

    assert(max_ports*sizeof(uint64_t) < PAGE_SIZE);
    uint64_t dcbaa_phys = malloc4k_phys();
    //array of pointers TODO should I allocate some DCBA structs and point to them?
    uint64_t *dcbaa_virt = phys_to_hhdm(dcbaa_phys);
    memset(dcbaa_virt, 0, PAGE_SIZE);

    if(scratchpad_required) {
        //create array of phys pointers to scratchpads
        assert(scratchpad_required*sizeof(uint64_t) < PAGE_SIZE);
        uint64_t scratchpad_pointers_phys = malloc4k_phys();
        uint64_t *scratchpad_pointers_virt = phys_to_hhdm(scratchpad_pointers_phys);
        memset(scratchpad_pointers_virt, 0, PAGE_SIZE);
        
        //allocate scratchpads
        for(uint16_t i=0; i<scratchpad_required; i++) {
            uint64_t scratchpad = malloc4k_phys();
            memset(phys_to_hhdm(scratchpad), 0, PAGE_SIZE);
            scratchpad_pointers_virt[i] = scratchpad;
        }
        //point to scratchpad pointers in the DCBAA
        dcbaa_virt[0] = scratchpad_pointers_phys;
    } else {
        dcbaa_virt[0] = 0;
    }

    write_bar_32(bar, dcbaa_phys & 0xFFFFFFFF, cap_length + DCBAAP_OFFSET);
    write_bar_32(bar, dcbaa_phys >> 32, cap_length + DCBAAP_OFFSET + 4);

    //set up command ring
    xhci.command_ring = create_ring();
    write_bar_32(bar, (xhci.command_ring.trbs_phys & 0xFFFFFFFF) | xhci.command_ring.ring_cycle_state, cap_length + CRCR_OFFSET);
    write_bar_32(bar, xhci.command_ring.trbs_phys >> 32, cap_length + CRCR_OFFSET + 4);

    //set up runtime registers
    //enable interrupts
    uint32_t iman = read_bar_32(bar, IR_IMAN_OFFSET(0) + rts_offset);
    iman |= IMAN_INTERRUPT_ENABLE;
    write_bar_32(bar, iman, IR_IMAN_OFFSET(0) + rts_offset);
    write_bar_32(bar, 0, IR_IMOD_OFFSET(0) + rts_offset);

    //set up event ring
    xhci.event_ring = create_ring();

    uint64_t event_ring_table_phys = malloc4k_phys();//this contains fat pointers to several event rings (in our case, one)
    struct EventRingSegmentTableEntry *event_ring_table_virt = phys_to_hhdm(event_ring_table_phys);
    memset(event_ring_table_virt, 0, PAGE_SIZE);
    event_ring_table_virt[0] = (struct EventRingSegmentTableEntry) {
        .ring_segment_base_address = xhci.event_ring.trbs_phys,
        .ring_segment_size = xhci.event_ring.count,
    };

    //set ERST size
    write_bar_32(bar, 1, IR_ERSTSZ_OFFSET(0) + rts_offset);

    update_erdp(&xhci, false);//TODO original implementation didn't add ERDP_EHB

    //point to the ERST
    write_bar_32(bar, event_ring_table_phys & 0xFFFFFFFF, IR_ERSTBA_OFFSET(0) + rts_offset);
    write_bar_32(bar, event_ring_table_phys >> 32, IR_ERSTBA_OFFSET(0) + rts_offset + 4);

    //clear prior interrupts
    ack_irq(&xhci, 0);

    //intel 7 series C210 series chipset magic?
    union ConfigAddress addr = {
        .register_offset = 0xD0,
        .device = dev,
        .reserved = 0,
        .enable_bit = 1,
    };
    config_write(addr, 0xFFFFFFFF);
    addr.register_offset = 0xD8;
    config_write(addr, 0xFFFFFFFF);

    //start
    write_bar_32(bar, USBCMD_RS | USBCMD_IE, cap_length + USBCMD_OFFSET);
    while(1) {
        uint32_t usb_sts = read_bar_32(bar, cap_length + USBSTS_OFFSET);
        if(!USBSTS_hchalted(usb_sts) && !USBSTS_cnr(usb_sts)) {
            //not halted and is ready
            break;
        }
    }

    delay();

    printf("scanning %d ports\n", max_ports);
    for(uint8_t port_idx = 0; port_idx < max_ports; port_idx++) {
        initialise_port(&xhci, dcbaa_virt, port_idx);
    }

}