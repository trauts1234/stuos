#include "uapi/stdbool.h"
#include "uapi/stdint.h"
#include "required.h"
#include "linked_list_item.h"

#define LL_CANARY 0xDEADBEEFu

// struct LinkedListCursor {
//     /// Pointer to the `next` field that points at the next item
//     struct LinkedListItem **next_pointer;
// };

static void assert_invariant(const struct LinkedListItem *const self) {
    if(self->canary != LL_CANARY) {
        printf("ERROR WITH MALLOC CANARY!\ncanary: %X", self->canary);
        abort();
    }
}
static struct LinkedListItem *get_next(struct LinkedListItem *self) {
    assert_invariant(self);
    return self->next;
}

/// Generates a `LinkedListItem`, that is free and represents all the memory after `self`, up to `next`, or the end of the heap if `next` is null
void lli_new(struct LinkedListItem *next, struct LinkedListItem *output) {
    *output = (struct LinkedListItem){ .next=next, .canary = LL_CANARY, .is_free = true };
}

static void destroy(struct LinkedListItem *self) {
    self->canary = 0xABABABABu;
    self->next = (void*)0xCAFECAFECAFECAFEULL;
}

bool lli_is_free(struct LinkedListItem *self) {
    assert_invariant(self);
    return self->is_free;
}
void lli_mark_as(struct LinkedListItem *self, bool is_free) {
    assert_invariant(self);
    self->is_free = is_free;
}
void* lli_region_start(const struct LinkedListItem *self) {
    assert_invariant(self);
    return (void*)(self+1);
}
uint64_t lli_region_size(const struct LinkedListItem *self, void* heap_end) {
    assert_invariant(self);
    void* region_start_ptr = lli_region_start(self);
    void* region_end_ptr = self->next ? self->next : heap_end;

    int64_t region_size_bytes = region_end_ptr - region_start_ptr;
    if(region_size_bytes < 0) {
        abort();
    }
    return region_size_bytes;
}

/// Initialises and inserts a blank segment after self at address `to_insert_addr`
/// 
/// Also updates `last_item_ptr` if required
void lli_insert_after(struct LinkedListItem *self, void* to_insert_addr, struct LinkedListItem **last_item_ptr) {
    // let's call the nodes self -> to_insert -> after
    struct LinkedListItem *after_addr = get_next(self);
    if(to_insert_addr == 0) abort();
    //inserted item must be after me, either in my region (as I am being split), or at the start of a newly brk'd region
    if(lli_region_start(self) > to_insert_addr) abort();

    //insert
    self->next = to_insert_addr;
    lli_new(after_addr, to_insert_addr);
    if(after_addr == 0) {
        //as I added a new last item, update
        *last_item_ptr = to_insert_addr;
    } else {
        //next item must be after inserted item
        if(lli_region_start(to_insert_addr) > (void*)after_addr) abort();
    }
}

/// Removes the item after myself, if both myself and next are free
/// 
/// ### Returns
/// True if a merge was successful
bool lli_try_merge_after(struct LinkedListItem *self, struct LinkedListItem **last_item_ptr) {
    //find the next item after self
    struct LinkedListItem *to_destroy = get_next(self);
    if(to_destroy == 0) return false;//I am last item, can't possible remove after

    if(!self->is_free || !to_destroy->is_free) return false;//can only merge free regions

    //make my pointer skip the deleted item
    struct LinkedListItem *after_removed_addr = get_next(to_destroy);
    self->next = after_removed_addr;

    //if I removed the last item, update
    if(to_destroy == *last_item_ptr) {
        *last_item_ptr = self;
    }

    destroy(to_destroy);
    return true;
}