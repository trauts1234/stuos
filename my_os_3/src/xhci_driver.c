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
//toggle the internal cycle bit
#define TRB_TC_BIT (1 << 1)

struct TransferRequestBlock {
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

struct CommandRing {
    uint64_t max_trb_count;
    uint64_t enqueue_ptr;
    struct TransferRequestBlock *trbs;
    uint8_t ring_cycle_state;
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
#define HCCPARAMS1 0x10
bool HCCPARAMS1_64BIT(uint32_t hccparams1) {return hccparams1 & 1;}

//offsets into BAR0+CAPLENGTH

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


#define USBCMD_HCRST (1 << 1)
#define USBCMD_RS 1

void initialise_xhci(struct PciDevice dev, struct BarInfo bar) {
    uint8_t cap_length = CAPLENGTH_CAPLENGTH(read_bar_32(bar, CAPLENGTH_AND_VERSION_OFFSET));
    const uint32_t hccparams1 = read_bar_32(bar, HCCPARAMS1);
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

    write_bar_32(bar, dcbaa_phys >> 32, cap_length + DCBAAP_OFFSET + 4);
    write_bar_32(bar, dcbaa_phys & 0xFFFFFFFF, cap_length + DCBAAP_OFFSET);

    //set up TRB
    uint64_t trb_phys = malloc4k_phys();
    struct TransferRequestBlock *trb_virt = phys_to_hhdm(trb_phys);
    memset(trb_virt, 0, PAGE_SIZE);

    struct CommandRing ring = {
        .max_trb_count = PAGE_SIZE / sizeof(struct TransferRequestBlock),
        .enqueue_ptr = 0,
        .ring_cycle_state = 1,
        .trbs = trb_virt
    };

    //set last element to a link, which points back to the start again
    ring.trbs[ring.max_trb_count-1] = (struct TransferRequestBlock) {
        .parameter = trb_phys,
        .control = (TRB_TYPE_LINK << 10) | TRB_TC_BIT | ring.ring_cycle_state
    };

    write_bar_32(bar, trb_phys >> 32, cap_length + CRCR_OFFSET + 4);
    write_bar_32(bar, (trb_phys & 0xFFFFFFFF) | ring.ring_cycle_state, cap_length + CRCR_OFFSET);

    //start
    write_bar_32(bar, USBCMD_RS, cap_length + USBCMD_OFFSET);
    while(USBSTS_hchalted(read_bar_32(bar, cap_length + USBSTS_OFFSET)));

    for(uint8_t port = 0; port < max_ports; port++) {
        uint32_t portsc = read_bar_32(bar, cap_length + PORTSC_OFFSET(port));
        assert(PORTSC_pp(portsc));//must already be powered

        if(PORTSC_ccs(portsc)) {
            printf("device found on port %u\n", port);
        } else {
            printf("nothing on port %u\n", port);
        }
    }

    printf("xhci initialised (error flag %d)\n", USBSTS_error(read_bar_32(bar, cap_length + USBSTS_OFFSET)));
}

void enqueue_ring(struct CommandRing *ring, struct TransferRequestBlock trb) {
    trb.cycle_bit = ring->ring_cycle_state;
    ring->trbs[ring->enqueue_ptr] = trb;

    ring->enqueue_ptr++;
    if(ring->enqueue_ptr == ring->max_trb_count-1) {
        //now pointing to link element, so update the cycle bit and loop round
        ring->trbs[ring->enqueue_ptr].cycle_bit ^= 1;
        ring->enqueue_ptr = 0;
        ring->ring_cycle_state ^= 1;
    }
}