#ifndef XHCI_DRIVER_H
#define XHCI_DRIVER_H

#include "pci.h"
#include "xhci_trb.h"

//store TRBs in a list after they have been dequeued
#define TRB_LIST_LEN 64

struct xHCIData {
    struct Ring command_ring;
    struct Ring event_ring;

    struct BarInfo bar;
    uint8_t cap_length;
    uint32_t rts_offset;
    uint32_t db_offset;
    
    //indexed by port index
    struct XHCIDevice {
        bool is_usb3;
        struct Ring ep0_transfer;
    } dev_data[64];

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
        uint8_t sub_class;
        uint8_t protocol;
        struct ExternEpDesc{
            uint8_t address;
            uint8_t attributes;
            uint16_t max_packet_size;
        } endpoints[16];//TODO is 16 right?
    } interfaces[256];
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
        address,
        attributes;
    uint16_t max_packet_size;
    uint8_t interval;
} __attribute__((packed));

void initialise_xhci(struct PciDevice dev, struct PciData *dev_data);

#endif