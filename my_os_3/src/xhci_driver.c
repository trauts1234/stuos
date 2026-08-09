#include "xhci_driver.h"
#include "pci.h"
#include "physical_slab_allocation.h"
#include "uapi/stdint.h"
#include "kern_libc.h"
#include "debugging.h"
#include "memory.h"
#include <uapi/stdbool.h>

//https://www.intel.com/content/dam/www/public/us/en/documents/technical-specifications/extensible-host-controler-interface-usb-xhci.pdf

//mark as link, which points back to the start
#define TRB_TYPE_LINK 6
#define TRB_TYPE_ENABLE_SLOT 9

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

union TransferRequestBlock {
    //basic params
    struct {
        uint64_t parameter;
        uint32_t status;
        union {
            struct {
                uint32_t
                cycle_bit: 1,
                reserved_0: 9,
                trb_type: 6,
                reserved_1: 16;
            };
            uint32_t control;
        };
    };

    struct CommandCompletionRequestBlock {
        uint64_t command_trb_pointer;
        uint32_t 
        reserved_0: 24,
        completion_code: 8;
        uint32_t
        cycle_bit: 1,
        reserved_1: 9,
        trb_type: 6,
        vfid: 8,
        slot_id: 8;
    } command_completion_request_block;
};

struct EventRingSegmentTableEntry {
    uint64_t ring_segment_base_address;
    uint16_t ring_segment_size;
    uint16_t reserved_0[3];
};

struct xHCIData {
    uint64_t command_trb_count;
    uint64_t command_enqueue_ptr;
    union TransferRequestBlock *command_trbs;
    uint8_t command_ring_cycle_state;

    uint64_t event_trb_count;
    uint64_t event_dequeue_idx;
    uint64_t event_trbs_phys;
    union TransferRequestBlock *event_trbs;
    uint8_t event_ring_cycle_state;

    struct BarInfo bar;
    uint32_t rts_offset;
    uint32_t db_offset;
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
bool HCCPARAMS1_64BIT(uint32_t hccparams1) {return hccparams1 & 1;}
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
bool PORTSC_ccs(uint32_t portsc) {return portsc & 1;}
bool PORTSC_pp(uint32_t portsc) {return portsc & (1 << 9);}

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
//TODO port reset and warm port reset
#define PORTSC_PR (1 << 4)
#define PORTSC_CSC (1 << 17)
#define PORTSC_PEC (1 << 18)
#define PORTSC_PRC (1 << 21)
#define PORTSC_WPR (1 << 31)
#define IMAN_INTERRUPT_ENABLE (1 << 1)
#define IMAN_INTERRUPT_PENDING 1
#define ERDP_EHB (1 << 3)

static void ack_irq(struct BarInfo bar, uint32_t cap_length, uint32_t rts_offset, uint8_t interrupt_index) {
    uint32_t usbsts = read_bar_32(bar, cap_length + USBSTS_OFFSET);
    usbsts |= USBSTS_EINT;
    write_bar_32(bar, usbsts, cap_length + USBSTS_OFFSET);

    uint32_t iman = read_bar_32(bar, rts_offset + IR_IMAN_OFFSET(interrupt_index));
    iman |= IMAN_INTERRUPT_PENDING;//write 1 to reset the bit to a 0
    write_bar_32(bar, iman, rts_offset + IR_IMAN_OFFSET(interrupt_index));
}

static void enqueue_ring(struct xHCIData *ring, union TransferRequestBlock trb) {
    trb.cycle_bit = ring->command_ring_cycle_state;
    ring->command_trbs[ring->command_enqueue_ptr] = trb;

    ring->command_enqueue_ptr++;
    if(ring->command_enqueue_ptr == ring->command_trb_count-1) {
        //now pointing to link element, so update the cycle bit and loop round
        ring->command_trbs[ring->command_enqueue_ptr].cycle_bit ^= 1;
        ring->command_enqueue_ptr = 0;
        ring->command_ring_cycle_state ^= 1;
    }
}

//returns 0 on success, -1 if there was nothing to get (trb_out unaffected)
static int dequeue_ring(struct xHCIData *ring, union TransferRequestBlock *trb_out) {
    if(ring->event_trbs[ring->event_dequeue_idx].cycle_bit != ring->event_ring_cycle_state) {
        return -1;
    }

    *trb_out = ring->event_trbs[ring->event_dequeue_idx];
    ring->event_dequeue_idx++;
    if(ring->event_dequeue_idx == ring->event_trb_count) {
        ring->event_dequeue_idx = 0;
        ring->event_ring_cycle_state ^= 1;
    }

    uint64_t erdp = ERDP_EHB | (ring->event_trbs_phys + sizeof(union TransferRequestBlock) * ring->event_dequeue_idx);
    write_bar_32(ring->bar, erdp >> 32, ring->rts_offset + IR_ERDP_OFFSET(0) + 4);
    write_bar_32(ring->bar, erdp & 0xFFFFFFFF, ring->rts_offset + IR_ERDP_OFFSET(0));

    return 0;
}

static void ring_doorbell(struct xHCIData *ring, uint8_t doorbell, uint8_t target) {
    write_bar_32(ring->bar, target, ring->db_offset + DOORBELL_OFFSET(doorbell));
}

void xhci_handle_responses(struct xHCIData *data) {
    //try and read some data
    union TransferRequestBlock recv = {};
    while(dequeue_ring(data, &recv) == 0) {
        switch(recv.trb_type) {
            case TRB_TYPE_TRANSFER:
            printf("type transfer event\n");break;

            case TRB_TYPE_CMD_COMPLETION:
            // printf("completion event: code 0x%x slot %u\n", recv.command_completion_request_block.completion_code, recv.command_completion_request_block.slot_id);
            break;

            case TRB_TYPE_PORT_STS_CHANGE:
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
            printf("WARN: unknown trb type 0x%x\n", recv.trb_type);
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

    uint8_t pci_config[256];
    read_header(dev, pci_config);

    //only got code for 64 bit
    assert(HCCPARAMS1_64BIT(hccparams1));

    printf("got %d max ports\nneed %d scratchpad buffers\ncap length %d\n", max_ports, HCSPARAMS2_required_scratchpad_buffers(hcsparams2), cap_length);

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
    uint64_t trb_phys = malloc4k_phys();
    union TransferRequestBlock *trb_virt = phys_to_hhdm(trb_phys);
    memset(trb_virt, 0, PAGE_SIZE);

    struct xHCIData ring = {
        .command_trb_count = PAGE_SIZE / sizeof(union TransferRequestBlock),
        .command_enqueue_ptr = 0,
        .command_ring_cycle_state = 1,
        .command_trbs = trb_virt,

        .bar = bar,
        .rts_offset = rts_offset,
        .db_offset = db_offset
    };

    //set last element to a link, which points back to the start again
    ring.command_trbs[ring.command_trb_count-1] = (union TransferRequestBlock) {
        .parameter = trb_phys,
        .control = (TRB_TYPE_LINK << 10) | TRB_TC_BIT | ring.command_ring_cycle_state
    };

    write_bar_32(bar, (trb_phys & 0xFFFFFFFF) | ring.command_ring_cycle_state, cap_length + CRCR_OFFSET);
    write_bar_32(bar, trb_phys >> 32, cap_length + CRCR_OFFSET + 4);

    //set up runtime registers
    //enable interrupts
    uint32_t iman = read_bar_32(bar, IR_IMAN_OFFSET(0) + rts_offset);
    iman |= IMAN_INTERRUPT_ENABLE;
    write_bar_32(bar, iman, IR_IMAN_OFFSET(0) + rts_offset);

    //set up event ring
    uint64_t event_ring_trb_phys = malloc4k_phys();//this is the actual queue
    union TransferRequestBlock *event_ring_trb_virt = phys_to_hhdm(event_ring_trb_phys);
    uint64_t event_ring_table_phys = malloc4k_phys();//this contains fat pointers to several event rings (in our case, one)
    struct EventRingSegmentTableEntry *event_ring_table_virt = phys_to_hhdm(event_ring_table_phys);

    uint16_t trb_count = PAGE_SIZE / sizeof(union TransferRequestBlock);
    event_ring_table_virt[0] = (struct EventRingSegmentTableEntry) {
        .ring_segment_base_address = event_ring_trb_phys,
        .ring_segment_size = trb_count,
    };

    //add event ring info to ring
    ring.event_ring_cycle_state = 1;
    ring.event_dequeue_idx = 0;
    ring.event_trb_count = trb_count;
    ring.event_trbs_phys = event_ring_trb_phys;
    ring.event_trbs = event_ring_trb_virt;

    //set ERST size
    write_bar_32(bar, 1, IR_ERSTSZ_OFFSET(0) + rts_offset);

    //update the ERDP
    uint64_t dequeue_addr = event_ring_trb_phys + sizeof(union TransferRequestBlock) * ring.event_dequeue_idx;
    write_bar_32(bar, dequeue_addr & 0xFFFFFFFF, rts_offset + IR_ERDP_OFFSET(0));
    write_bar_32(bar, dequeue_addr >> 32, rts_offset + IR_ERDP_OFFSET(0) + 4);

    //point to the ERST
    write_bar_32(bar, event_ring_table_phys & 0xFFFFFFFF, IR_ERSTBA_OFFSET(0) + rts_offset);
    write_bar_32(bar, event_ring_table_phys >> 32, IR_ERSTBA_OFFSET(0) + rts_offset + 4);

    //clear prior interrupts
    ack_irq(bar, cap_length, rts_offset, 0);

    //start
    write_bar_32(bar, USBCMD_RS, cap_length + USBCMD_OFFSET);
    while(1) {
        uint32_t usb_sts = read_bar_32(bar, cap_length + USBSTS_OFFSET);
        if(!USBSTS_hchalted(usb_sts) && !USBSTS_cnr(usb_sts)) {
            //not halted and is ready
            break;
        }
    }

    printf("xhci initialised (error flag %d)\n", USBSTS_error(read_bar_32(bar, cap_length + USBSTS_OFFSET)));

    for(uint64_t i=0;i<3000;i++) {
        __asm("nop");
    }

    for(int i=0; i<max_ports; i++) {
        union TransferRequestBlock send = {};
        send.trb_type = TRB_TYPE_ENABLE_SLOT;
        enqueue_ring(&ring, send);
        for(uint64_t i=0;i<3000;i++) {
            __asm("nop");
        }
        ring_doorbell(&ring, 0, 0);//ring command doorbell
        for(uint64_t i=0;i<3000;i++) {
            __asm("nop");
        }
    }
    
    for(uint64_t i=0;i<3000;i++) {
        __asm("nop");
    }
    
    xhci_handle_responses(&ring);

    printf("scanning %d ports\n", max_ports);
    for(uint8_t port = 0; port < max_ports; port++) {
        uint32_t portsc = read_bar_32(bar, cap_length + PORTSC_OFFSET(port));
        assert(PORTSC_pp(portsc));//TODO power on the port if it isn't already

        //write to clear some status bits
        portsc |= PORTSC_CSC | PORTSC_PEC | PORTSC_PRC;
        write_bar_32(bar, portsc, cap_length + PORTSC_OFFSET(port));
        for(uint64_t i=0;i<1000;i++) {
            __asm("nop");
        }
        portsc = read_bar_32(bar, cap_length + PORTSC_OFFSET(port));

        if(PORTSC_ccs(portsc)) {
            printf("(ccs) device found on port %u\n", port);
        }
        if(portsc & PORTSC_CSC) {
            printf("(csc) device found on port %u\n", port);
        }

    }

}