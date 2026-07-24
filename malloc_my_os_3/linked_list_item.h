#ifndef LINKED_LIST_ITEM_H
#define LINKED_LIST_ITEM_H

#include "uapi/stdint.h"
#include "required.h"

//debugging stuff
#define HCF {printf("MALLOC ABORT ON LINE %d FILE %s\n", __LINE__, __FILE__); abort();}

/// Must be aligned to 16 byte, so that all mallocs are
struct LinkedListItem {
    struct LinkedListItem *next;
    bool is_free;
    uint32_t canary;
} __attribute__((aligned(16)));

void lli_new(struct LinkedListItem *next, struct LinkedListItem *output);
void lli_mark_as(struct LinkedListItem *self, bool is_free);
void* lli_region_start(const struct LinkedListItem *self);
uint64_t lli_region_size(const struct LinkedListItem *self, void* heap_end);
void lli_insert_after(struct LinkedListItem *self, void* to_insert_addr, struct LinkedListItem **last_item_ptr);
bool lli_try_merge_after(struct LinkedListItem *self, struct LinkedListItem **last_item_ptr);
bool lli_is_free(struct LinkedListItem *self);

#endif