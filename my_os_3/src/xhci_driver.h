#ifndef XHCI_DRIVER_H
#define XHCI_DRIVER_H

#include "pci.h"
#include "xhci_trb.h"

//store TRBs in a list after they have been dequeued
#define TRB_LIST_LEN 64

struct DeviceContext {
    //first entry is common information about the device (slot context)
    uint32_t
        route_string: 20,
        speed: 4,
        reserved_2: 1,
        mtt: 1,
        hub: 1,
        context_entries: 5;
    uint32_t
        max_exit_latency: 16,
        root_hub_port_num: 8,
        number_of_ports: 8;
    uint32_t
        tt_hub_slot_id: 8,
        tt_port_num: 8,
        ttt: 2,
        reserved_3: 4,
        interrupt_target: 10;
    uint32_t
        device_address: 8,
        reserved_4: 19,
        //0=>disabled/enabled, 1=>default, 2=>addressed, 3=>configured
        slot_state: 5;
    uint32_t reserved_5[4];

    //endpoint context
    struct EndpointContext {
        uint32_t
        //0=>disabled, 1=>running, 2=>halted, 3=>stopped, 4=>error
            ep_state: 3,
            reserved_0: 5,
            mult: 2,
            max_p_streams: 5,
            lsa: 1,
            interval: 8,
            max_esit_payload_hi: 8;
        uint32_t
            reserved_1: 1,
            cerr: 2,
            //1=>isoch out, 2=>bulk out, 3=>interrupt out, 4=>control, 5=>isoch in, 6=>bulk in, 7=>interrupt in
            ep_type: 3,
            reserved_2: 1,
            hid: 1,
            max_burst_size: 8,
            max_packet_size: 16;
        uint32_t
            dcs: 1,
            reserved_3: 3,
            tr_dequeue_pointer_lo: 28;//shift off the low 4 bits before putting in here
        uint32_t tr_dequeue_pointer_hi;
        uint32_t
            average_trb_length: 16,
            max_esit_payload_lo: 16;
        uint32_t reserved_4[3];

    } endpoint_context[31];
};

struct InputContext {
    //control context
    uint32_t drop_flags;//bottom 2 bits are reserved, 1 means disabled
    uint32_t add_flags;
    uint32_t reserved_0[5];
    uint32_t
        configuration_value: 8,
        interface_number: 8,
        alternate_setting: 8,
        reserved_1: 8;
    
    struct DeviceContext device_context;
};

struct xHCIData {
    struct Ring command_ring;
    struct Ring event_ring;

    struct BarInfo bar;
    uint8_t cap_length;
    uint32_t rts_offset;
    uint32_t db_offset;
    
    //indexed by slot number (one based)
    struct XHCIDevice {
        //indexed by calculate_endpoint_index (or 0 for endpoint 0)
        struct Ring endpoint_rings[15];
        uint64_t input_context_phys;
        struct InputContext *input_context;
        //root port number (0 as a null sentinel if the slot is unused)
        uint8_t one_based_root_port;
    } slots[256];

    //indexed by root port index (0 based)
    bool port_is_usb3[64];

    //I've taken these from the event TRBs
    //trb type 0 indicates an empty slot
    struct TRB unhandled_events[TRB_LIST_LEN];
};

struct DeviceDescriptor {
    uint8_t
    //0x12
        length,
    //0x01
        type,
    //for example 0x0200 is usb 02.00
        release_bcd_min, release_bcd_maj,
    //0x00, indicates to use the class code in the interface descriptor
        device_class,
        sub_class,
        protocol,
        max_packet_size;

    //this is after the first 8 bytes!
    uint16_t
        vendor_id,
        product_id,
        device_release;
    uint8_t
        manufacturer,
        //index of string descriptor
        product,
        //index of serial number descriptor
        serial_num,
        configurations;
};

struct ExternConfigDesc {
    uint8_t num_interfaces;
    struct ExternIfDesc{
        uint8_t num_endpoints;
        enum {ExternIfClassMSD=0x08} class_code;
        enum {ExternIfSubClassSCSI=0x06} sub_class;
        enum {ExternIfProtocolBulkOnly=0x50} protocol;
        struct ExternEpDesc{
            uint8_t endpoint_num;
            bool is_in;//1=>in, 0=>out
            enum {EPTransferControl=0b00, EpTransferIsochronous=0b01, EpTransferBulk=0b10, EpTransferInterrupt=0b11} transfer_type;
            uint16_t max_packet_size;
        } endpoints[16];//TODO is 16 right?
    } interfaces[256];
};

struct RequestTemplate {
    uint8_t slot_number;

    enum {HostToDevice=0,DeviceToHost=1} direction;
    enum {RequestTypeStandard=0, RequestTypeClass=1, RequestTypeVendor=2} request_type;
    enum {RecipientDevice=0, RecipientInterface=1, RecipientEndpoint=2, RecipientOther=3} recipient;
    enum {GET_DESCRIPTOR=6, SET_CONFIGURATION=9, GET_MAX_LUN=0xFE} request;
    union {
        uint16_t value;
        struct {
            uint8_t
                descriptor_index,
            //1=>DEVICE, 2=>configuration, 3=>string, 4=>interface, 5=>endpoint, 6=>device qualifier, 7=>other speed configuration, 8=>interface power
                descriptor_type;
        };
    };
    uint16_t index;
    uint16_t length;
};

void make_request(struct xHCIData *xhci, void *output, struct RequestTemplate request);
//if a slot's input context struct has been changed, then this will send the change to the controller
void update_input_context(struct xHCIData *xhci, uint8_t slot_id, bool am_modifying_existing_endpoints);
uint8_t calculate_endpoint_index(uint8_t endpoint_num, bool is_in);
// sets the context_entries field correctly
void set_context_entries(struct DeviceContext *device_context);
// endpoint index as a return value from calculate_endpoint_index, or 0 for the control doorbell
//
// port is one-based
void ring_doorbell(struct xHCIData *data, uint8_t port, uint8_t endpoint_index);
//memory fence + nops
void delay();

void initialise_xhci(struct PciDevice dev, struct PciData *dev_data);

#endif