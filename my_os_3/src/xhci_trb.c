#include "xhci_trb.h"
#include "physical_slab_allocation.h"
#include "memory.h"
#include "kern_libc.h"
#include "debugging.h"

struct Ring create_ring() {
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

void enqueue_ring(struct Ring *ring, struct TRB trb) {
    assert(ring->trbs);
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

int dequeue_ring(struct Ring *ring, struct TRB *trb_out) {
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