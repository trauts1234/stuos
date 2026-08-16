#include "xhci_driver.h"
#include "pci.h"
#include "physical_slab_allocation.h"
#include "uapi/stdint.h"
#include "kern_libc.h"
#include "debugging.h"
#include "memory.h"
#include <uapi/stdbool.h>

//TODO I am missing tons of volatile in here!!!!!

//store TRBs in a list after they have been dequeued
#define TRB_LIST_LEN 64

//https://www.intel.com/content/dam/www/public/us/en/documents/technical-specifications/extensible-host-controler-interface-usb-xhci.pdf

//mark as link, which points back to the start
#define TRB_TYPE_LINK 6
#define TRB_TYPE_ENABLE_SLOT 9
#define TRB_TYPE_SET_ADDRESS 11

//event ring responses types
#define TRB_TYPE_TRANSFER 32
#define TRB_TYPE_CMD_COMPLETION 33
#define TRB_TYPE_PORT_STS_CHANGE 34
//optional
#define TRB_TYPE_BANDWIDTH_REQUEST 35
//optional
#define TRB_TYPE_DOORBELL 36
#define TRB_TYPE_HOST_CONTROLLER 37
#define TRB_TYPE_DEVICE_NOTIFICATION 38
#define TRB_TYPE_MFINDEX_WRAP 39

//toggle the internal cycle bit
#define TRB_TC_BIT (1 << 1)

struct TRB {
    union {
        uint64_t raw;
    } parameter;

    union {
        uint64_t raw;
        //minimal
        struct {
            uint64_t
            reserved_0: 32,
            cycle_bit: 1,
            reserved_1: 9,
            trb_type: 6,
            reserved_2: 16;
        };

        struct LinkTRBSts {
            uint64_t
            reserved_0: 22,
            interrupter_target: 10,
            cycle_bit: 1,
            toggle_cycle: 1,
            reserved_1: 2,
            chain_bit: 1,
            interrupt_on_completion: 1,
            reserved_2: 4,
            trb_type: 6,
            reserved_3: 16;
        } link;

        struct CommandCompletionEvent {
            uint64_t
            command_completion_parameter: 24,
            completion_code: 8,
            cycle_bit: 1,
            reserved_0: 9,
            trb_type: 6,
            vf_id: 8,
            slot_id: 8;
        } command_completion;

        struct SetAddressCommand {
            uint64_t
            reserved_0: 32,
            cycle_bit: 1,
            reserved_1: 8,
            block_set_address_request: 1,//BSR
            trb_type: 6,
            reserved_2: 8,
            slot_id: 8;
        } set_address;
    } status;
};

struct EventRingSegmentTableEntry {
    uint64_t ring_segment_base_address;
    uint16_t ring_segment_size;
    uint16_t reserved_0[3];
};

struct Ring {
    uint64_t count;
    uint64_t idx;
    uint64_t trbs_phys;
    struct TRB *trbs;
    uint8_t ring_cycle_state;
};

struct xHCIData {
    struct Ring command_ring;
    struct Ring event_ring;

    struct BarInfo bar;
    uint32_t rts_offset;
    uint32_t db_offset;
    
    //indexed by port index
    struct PerPortxHCIData {
        bool is_usb3;
        struct Ring ep0_transfer;
    } dev_data[64];

    //I've taken these from the event TRBs
    //trb type 0 indicates an empty slot
    struct TRB unhandled_command_completion[TRB_LIST_LEN];
};

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
#define IR_IMOD_OFFSET(interrupt_index) (024 + 32*interrupt_index)
#define IR_ERSTSZ_OFFSET(interrupt_index) (0x28 + 32*interrupt_index)
//64 bit!
#define IR_ERSTBA_OFFSET(interrupt_index) (0x30 + 32*interrupt_index)
//64 bit!
#define IR_ERDP_OFFSET(interrupt_index) (0x38 + 32*interrupt_index)

//offsets into BAR0 + DBOFF
#define DOORBELL_OFFSET(doorbell_index) (4*doorbell_index)

#define USBSTS_EINT (1 << 3)
#define USBCMD_HCRST (1 << 1)
#define USBCMD_RS 1
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

static void ack_irq(struct BarInfo bar, uint32_t cap_length, uint32_t rts_offset, uint8_t interrupt_index) {
    uint32_t usbsts = read_bar_32(bar, cap_length + USBSTS_OFFSET);
    usbsts |= USBSTS_EINT;
    write_bar_32(bar, usbsts, cap_length + USBSTS_OFFSET);

    uint32_t iman = read_bar_32(bar, rts_offset + IR_IMAN_OFFSET(interrupt_index));
    iman |= IMAN_INTERRUPT_PENDING;//write 1 to reset the bit to a 0
    write_bar_32(bar, iman, rts_offset + IR_IMAN_OFFSET(interrupt_index));
}

static void enqueue_ring(struct Ring *ring, struct TRB trb) {
    trb.status.cycle_bit = ring->ring_cycle_state;
    ring->trbs[ring->idx] = trb;

    ring->idx++;
    if(ring->idx == ring->count-1) {
        //now pointing to link element, so update the cycle bit and loop round
        ring->trbs[ring->idx].status.cycle_bit ^= 1;
        ring->idx = 0;
        ring->ring_cycle_state ^= 1;
    }
}

//returns 0 on success, -1 if there was nothing to get (trb_out unaffected)
static int dequeue_ring(struct Ring *ring, struct TRB *trb_out) {
    if(ring->trbs[ring->idx].status.cycle_bit != ring->ring_cycle_state) {
        return -1;
    }

    *trb_out = ring->trbs[ring->idx];
    ring->idx++;
    if(ring->idx == ring->count) {
        ring->idx = 0;
        ring->ring_cycle_state ^= 1;
    }

    return 0;
}

//sets the event ring dequeue pointer to its new value, so that the xHCI controller can write new TRBs
//do this after dequeueing from the event ring
static void update_erdp(struct xHCIData *data) {
    uint64_t erdp = ERDP_EHB | (data->event_ring.trbs_phys + sizeof(struct TRB) * data->event_ring.idx);
    write_bar_32(data->bar, erdp & 0xFFFFFFFF, data->rts_offset + IR_ERDP_OFFSET(0));
    write_bar_32(data->bar, erdp >> 32, data->rts_offset + IR_ERDP_OFFSET(0) + 4);
}

static void ring_doorbell(struct xHCIData *ring, uint8_t doorbell, uint8_t target) {
    write_bar_32(ring->bar, target, ring->db_offset + DOORBELL_OFFSET(doorbell));
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
static struct TRB extract_from_list(struct TRB list[TRB_LIST_LEN]) {
    for(int i=0; i<TRB_LIST_LEN ;i++) {
        if(list[i].status.trb_type != 0) {
            struct TRB result = list[i];
            list[i].status.trb_type = 0;
            return result;
        }
    }
    return (struct TRB) {};
}

static struct Ring create_ring() {
    const uint64_t count = PAGE_SIZE / sizeof(struct TRB);
    uint64_t trb_phys = malloc4k_phys();
    struct TRB *trb_virt = phys_to_hhdm(trb_phys);
    memset(trb_virt, 0, PAGE_SIZE);

    //set last element to a link, which points back to the start again
    trb_virt[count-1] = (struct TRB) {
        .parameter = trb_phys,
        .status.link = {
            .cycle_bit = 1,
            .trb_type = TRB_TYPE_LINK,
            .toggle_cycle = 1
        }
    };

    return (struct Ring) {
        .count = count,
        .idx = 0,
        .trbs_phys = trb_phys,
        .trbs = trb_virt,
        .ring_cycle_state = 1
    };
}

static void debug_ring(const struct Ring *ring) {
    //print some of the preceeding items
    for(uint64_t i=0; i<ring->idx; i++) {
        struct TRB *x = &ring->trbs[i];
        printf("TRB: p %llu s%llu type %d cycle %d\n", x->parameter.raw, x->status.raw, x->status.trb_type, x->status.cycle_bit);
    }
    printf("\n");
}

void xhci_handle_responses(struct xHCIData *data) {
    //try and read some data
    struct TRB recv = {};
    while(dequeue_ring(&data->event_ring, &recv) == 0) {
        update_erdp(data);
        switch(recv.status.trb_type) {
            case TRB_TYPE_TRANSFER:
            printf("type transfer event\n");break;

            case TRB_TYPE_CMD_COMPLETION:
            insert_to_list(data->unhandled_command_completion, recv);
            break;

            case TRB_TYPE_PORT_STS_CHANGE:
            printf("port status event\n");break;
            case TRB_TYPE_BANDWIDTH_REQUEST:
            break;//optional
            case TRB_TYPE_DOORBELL:
            printf("doorbell event\n");break;
            case TRB_TYPE_HOST_CONTROLLER:
            printf("host controller event\n");break;
            case TRB_TYPE_DEVICE_NOTIFICATION:
            printf("device notification event\n");break;
            case TRB_TYPE_MFINDEX_WRAP:
            printf("mfindex wrap event\n");break;
            
            default:
            printf("WARN: unknown trb type 0x%x\n", recv.status.trb_type);
            break;
        }
    }
}

void initialise_xhci(struct PciDevice dev, struct BarInfo bar) {
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
            printf("found range of usb%d.%d ports\n", usb_maj, usb_min);
            for(uint8_t i=port_index; i<port_index+port_count; i++) {
                xhci.dev_data[i].is_usb3 = (usb_maj == 3);
            }

            default:
            break;
        }

        xecp = ((data >> 8) & 0xFF) << 2;
    }

    uint8_t pci_config[256];
    read_header(dev, pci_config);

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
    write_bar_32(bar, 0xFFFF, cap_length + DNCTRL_OFFSET);
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

    update_erdp(&xhci);//TODO original implementation didn't add ERDP_EHB

    //point to the ERST
    write_bar_32(bar, event_ring_table_phys & 0xFFFFFFFF, IR_ERSTBA_OFFSET(0) + rts_offset);
    write_bar_32(bar, event_ring_table_phys >> 32, IR_ERSTBA_OFFSET(0) + rts_offset + 4);

    //clear prior interrupts
    ack_irq(bar, cap_length, rts_offset, 0);


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
    write_bar_32(bar, USBCMD_RS, cap_length + USBCMD_OFFSET);
    while(1) {
        uint32_t usb_sts = read_bar_32(bar, cap_length + USBSTS_OFFSET);
        if(!USBSTS_hchalted(usb_sts) && !USBSTS_cnr(usb_sts)) {
            //not halted and is ready
            break;
        }
    }

    for(uint64_t i=0;i<3000;i++) {
        __asm("nop");
    }

    printf("scanning %d ports\n", max_ports);
    for(uint8_t port_idx = 0; port_idx < max_ports; port_idx++) {
        uint32_t portsc = read_bar_32(bar, cap_length + PORTSC_OFFSET(port_idx));
        // printf("port %u PORTSC = %08x\n", port + 1, portsc);
        assert(portsc & PORTSC_PP);//TODO power on the port if it isn't already

        if((portsc & PORTSC_CCS) && (portsc & PORTSC_CSC)) {
            printf("device found on port index %u\n", port_idx); 
            bool is_usb3 = xhci.dev_data[port_idx].is_usb3;
            //write to clear some status bits?
            portsc |= PORTSC_CSC | PORTSC_PEC | PORTSC_PRC;
            //reset or warm port reset
            portsc |= is_usb3 ? PORTSC_WPR : PORTSC_PR;
            write_bar_32(bar, portsc, cap_length + PORTSC_OFFSET(port_idx));

            for(uint64_t i=0;i<3000;i++) {
                __asm("nop");
            }
            portsc = read_bar_32(bar, cap_length + PORTSC_OFFSET(port_idx));
            //wait for reset completion
            while((is_usb3 && !(portsc & PORTSC_WRC)) || (!is_usb3 && !(portsc & PORTSC_PRC))) {
                portsc = read_bar_32(bar, cap_length + PORTSC_OFFSET(port_idx));
            }

            for(uint64_t i=0;i<3000;i++) {
                __asm("nop");
            }

            //write to reset some flags
            portsc |= PORTSC_PRC | PORTSC_WRC | PORTSC_CSC | PORTSC_PEC;
            //writing 1 would clear this flag, so we write 0
            portsc &= ~(uint32_t)PORTSC_PED;
            write_bar_32(bar, portsc, cap_length + PORTSC_OFFSET(port_idx));

            for(uint64_t i=0;i<3000;i++) {
                __asm("nop");
            }

            portsc = read_bar_32(bar, cap_length + PORTSC_OFFSET(port_idx));
            assert(portsc & PORTSC_PED);

            //get a device slot
            struct TRB send = {
                .status.trb_type = TRB_TYPE_ENABLE_SLOT
            };
            enqueue_ring(&xhci.command_ring, send);
            ring_doorbell(&xhci, 0, 0);//ring command doorbell
            xhci_handle_responses(&xhci);
            struct TRB recv = extract_from_list(xhci.unhandled_command_completion);
            assert(recv.status.command_completion.trb_type == TRB_TYPE_CMD_COMPLETION);
            assert(recv.status.command_completion.completion_code == 1);
            const uint8_t slot_id = recv.status.command_completion.slot_id;
            assert(slot_id != 0);

            const char* speed_str = 
                (const char*[]){
                    "invalid",
                    "full speed usb 2",
                    "low speed usb 2",
                    "high speed usb 2",
                    "super speed usb 3",
                    "super speed plus usb 3.1"
                }
                [PORTSC_speed(portsc)];
            printf("speed: %s\n", speed_str);

            //create a ring for this port's endpoint 0
            xhci.dev_data[port_idx].ep0_transfer = create_ring();
            struct Ring *transfer_ring = &xhci.dev_data[port_idx].ep0_transfer;
            //create device context
            uint64_t device_context_phys = malloc4k_phys();
            struct DeviceContext *device_context = phys_to_hhdm(device_context_phys);
            memset(device_context, 0, sizeof(struct DeviceContext));
            dcbaa_virt[slot_id] = device_context_phys;

            //create input context, which is a second device context plus some extra
            uint64_t input_context_phys = malloc4k_phys();
            struct InputContext *input_context = phys_to_hhdm(input_context_phys);
            memset(input_context, 0, sizeof(struct InputContext));

            input_context->add_flags |= 0b11;//enable the slot context and control EP0
            input_context->device_context.context_entries = 1;
            input_context->device_context.speed = PORTSC_speed(portsc);
            input_context->device_context.root_hub_port_num = port_idx+1;
            input_context->device_context.interrupt_target = 0;//?
            //set up the mandatory endpoint context zero
            input_context->device_context.endpoint_context[0] = (struct EndpointContext) {
                .ep_type = 4,
                .cerr = 3,
                .max_packet_size = (const uint16_t[]) {8,64,8, 64,512,512}[PORTSC_speed(portsc)],
                .tr_dequeue_pointer_lo = transfer_ring->trbs_phys >> 4,
                .tr_dequeue_pointer_hi = transfer_ring->trbs_phys >> 32,
                .dcs = 1,//should it be 1???
                .average_trb_length = 8,
                .max_esit_payload_lo = 0,
                .max_esit_payload_hi = 0,

            };
            //initialise something
            send = (struct TRB) {
                .parameter.raw = input_context_phys,
                .status.set_address = {
                    .block_set_address_request = 1,//block the action, since this TRB can tell old usb devices to get set up properly?
                    .trb_type = TRB_TYPE_SET_ADDRESS,
                    .slot_id = slot_id
                }
            };
            enqueue_ring(&xhci.command_ring, send);
            ring_doorbell(&xhci, 0, 0);
            for(uint64_t i=0;i<3000;i++) {
                __asm("nop");
            }
            xhci_handle_responses(&xhci);
            recv = extract_from_list(xhci.unhandled_command_completion);
            assert(recv.status.command_completion.trb_type == TRB_TYPE_CMD_COMPLETION);
            // printf("cmd ring\n");
            // debug_ring(&xhci.command_ring);
            // printf("event ring\n");
            // debug_ring(&xhci.event_ring);
            assert(recv.status.command_completion.completion_code == 1);
            assert(recv.status.command_completion.slot_id == slot_id);

            assert(device_context->slot_state == 1);
            assert(device_context->device_address == 0);
            assert(device_context->endpoint_context[0].ep_state == 1);

            free4k_phys(input_context_phys);
            //page 332
        }

    }

}