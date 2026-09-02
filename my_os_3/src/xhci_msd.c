#include "xhci_msd.h"
#include "physical_slab_allocation.h"
#include "xhci_driver.h"
#include "kern_libc.h"
#include "debugging.h"
#include "xhci_trb.h"
#include "memory.h"

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

struct InquiryReturn {
    uint8_t
        //0=>direct access block device, 5=>cd-rom
        peripheral_device_type: 5,
        peripheral_qualifier: 3,
        reserved_0: 7,
        removable: 1,
        version,//0 (but it isn't in QEMU?)
        response_data_format: 4,
        hisup: 1,
        norm_aca: 1,
        reserved_1: 2,
        additional_length,//how many additional bytes are in this data block
        prot: 1,
        reserved_2: 2,
        pc_3: 1,
        tpgs: 2,
        acc: 1,
        sccs: 1,
        addr_16: 1,
        reserved_3: 3,
        multi_p: 1,
        vs_0: 1,
        enc_serv: 1,
        resv: 1,
        vs_1: 1,
        command_queue: 1,
        reserved_4: 2,
        sync: 1,
        wbus_16: 1,
        reserved_5: 2;
    uint64_t
        vendor_information,//ASCII
        product_identification[2];//ASCII
    uint32_t product_revision_level;
} __attribute__((packed));

struct ReadCapacity10Return {
    //big endian!
    uint32_t last_valid_lba;
    //big endian!
    uint32_t block_size_bytes;
};

uint32_t flip_endianness(uint32_t val) {
    uint8_t *bytes = (void*)&val;
    return
        (bytes[0] << 24) |
        (bytes[1] << 16) |
        (bytes[2] << 8) |
        bytes[3];
}

static uint32_t send_bbb(struct xHCIData *xhci, uint8_t slot_number, struct Ring *in_ring, int in_index, struct Ring *out_ring, int out_index, struct CommandBlockWrapper cbw, void* response_out, uint32_t response_len) {
    static uint32_t next_free_tag = 69;

    uint64_t command_phys = malloc4k_phys();
    volatile struct CommandBlockWrapper *command = phys_to_hhdm(command_phys);
    cbw.tag = next_free_tag++,
    *command = cbw;

    assert(response_len > 0);
    assert(response_out);
    uint64_t response_num_pages = round_up_pages(response_len);
    uint64_t response_phys = malloc_contiguous_phys(response_num_pages);
    
    uint64_t status_phys = malloc4k_phys();
    volatile struct CommandStatusWrapper *status = phys_to_hhdm(status_phys);

    //send command
    enqueue_ring(out_ring, (struct TRB) {
        .parameter.raw = command_phys,
        .status.normal = {
            .trb_transfer_length = sizeof(struct CommandBlockWrapper),
            .trb_type = TRB_TYPE_NORMAL,
            .interrupt_on_completion = 1,
        }
    });
    ring_doorbell(xhci, slot_number, out_index);
    //check that sending command was successful
    struct TRB recv = fetch_and_extract(xhci, TRB_TYPE_TRANSFER);
    assert(recv.status.type_transfer.trb_type != 0);
    assert(recv.status.type_transfer.completion_code == 1);
    assert(recv.status.type_transfer.trb_transfer_length == 0);
    assert(recv.status.type_transfer.slot_id == slot_number);

    //here is where to put the response
    enqueue_ring(in_ring, (struct TRB) {
        .parameter.raw = response_phys,
        .status.normal = {
            .trb_transfer_length = response_len,
            .trb_type = TRB_TYPE_NORMAL,
            .interrupt_on_completion = 1,
            .interrupt_on_short_packet = 1,
        }
    });
    // here is where to put the CSW
    enqueue_ring(in_ring, (struct TRB) {
        .parameter.raw = status_phys,
        .status.normal = {
            .trb_transfer_length = sizeof(struct CommandStatusWrapper),
            .trb_type = TRB_TYPE_NORMAL,
            .interrupt_on_completion = 1,
        }
    });
    ring_doorbell(xhci, slot_number, in_index);

    //read the response from the inquiry response (TODO handle a short packet gracefully since this is fine)
    recv = fetch_and_extract(xhci, TRB_TYPE_TRANSFER);
    assert(recv.status.type_transfer.trb_type != 0);
    assert(recv.status.type_transfer.completion_code == 1);
    assert(recv.status.type_transfer.trb_transfer_length == 0);
    assert(recv.status.type_transfer.slot_id == slot_number);

    recv = fetch_and_extract(xhci, TRB_TYPE_TRANSFER);
    assert(recv.status.type_transfer.trb_type != 0);
    assert(recv.status.type_transfer.completion_code == 1);
    assert(recv.status.type_transfer.trb_transfer_length == 0);
    assert(recv.status.type_transfer.slot_id == slot_number);

    assert(status->signature == 0x53425355);
    assert(status->tag == command->tag);
    assert(status->status == 0);

    memcpy(response_out, phys_to_hhdm(response_phys), response_len - status->data_residue);

    free4k_phys(command_phys);
    free_contiguous_phys(response_phys, response_num_pages);
    free4k_phys(status_phys);

    return response_len - status->data_residue;
}

void initialise_msd(struct xHCIData *xhci, uint8_t slot_number, struct ExternConfigDesc config_descriptor) {
    printf("initialising MSD:\n");

    assert(config_descriptor.num_interfaces == 1);
    printf("config val %d\n", config_descriptor.configuration_value);
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

    printf("set configuration\n");
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

    printf("get max lun\n");

    uint8_t max_lun;
    make_request(xhci, &max_lun, (struct RequestTemplate) {
        .slot_number = slot_number,

        .direction = DeviceToHost,
        .request_type = RequestTypeClass,
        .recipient = RecipientInterface,
        .request = GET_MAX_LUN,
        .value = 0x0000,
        .index=0,
        .length=1,
        .setup_transfer_type = InDataStage
    });//apparently if it returns STALL, then take LUN=0 and total count=1
    if(max_lun == 0xFF) max_lun = 0;//some devices do this
    assert(max_lun <= 15);
    max_lun++;//since zero based, add one
    assert(max_lun == 1);

    printf("inquiry\n");

    struct InquiryReturn inquiry_return = {};
    uint32_t inquiry_data_read = send_bbb(
        xhci, slot_number, in_ring, in_index, out_ring, out_index,
        (struct CommandBlockWrapper) {
            .signature=0x43425355,
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
        },
        &inquiry_return, sizeof(inquiry_return)
    );
    assert(inquiry_data_read == sizeof(inquiry_return));
    assert(inquiry_return.peripheral_device_type == 0);//direct access block device
    assert(inquiry_return.response_data_format == 1 || inquiry_return.response_data_format == 2);
    char vendor_information[9] = {};
    memcpy(&vendor_information, (void*)&inquiry_return.vendor_information, 8);
    char product_identification[17] = {};
    memcpy(&product_identification, (void*)&inquiry_return.product_identification, 16);
    printf("vendor information: %s\nproduct identification: %s\n", vendor_information, product_identification);

    //403
    struct ReadCapacity10Return read_capacity_10;
    uint32_t read_capacity_read = send_bbb(
        xhci, slot_number, in_ring, in_index, out_ring, out_index,
        (struct CommandBlockWrapper) {
            .signature=0x43425355,
            .transfer_length = 0x8,
            .direction = 1,
            .lun=0,
            .command_len = 0x0A,
            .command = {
                0x25,//read capacity(10)
                0,//reserved
                0,0,0,0,//LBA 0
                0,0,0,//reserved
                0//control
            }
        },
        &read_capacity_10, sizeof(read_capacity_10)
    );
    assert(read_capacity_read == sizeof(read_capacity_10));
    assert(read_capacity_10.last_valid_lba != 0xFFFFFFFF);//32 bit is not big enough to find the capacity of the drive! (see page 105)

    uint32_t last_lba = flip_endianness(read_capacity_10.last_valid_lba);
    uint32_t block_size = flip_endianness(read_capacity_10.block_size_bytes);

    printf("last LBA: 0x%x\nblock size %u\n", last_lba, block_size);
}