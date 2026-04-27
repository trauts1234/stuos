#include "uapi/stdint.h"
#include <stddef.h>
#include <stdbool.h>
#include "interrupts.h"
#include "kern_libc.h"
#include "memory.h"
#include "limine.h"
#include "debugging.h"
#include "physical_slab_allocation.h"

extern void invalidate_page(void* ptr);

union PointerBitmap {
    void* raw_ptr;
    struct {
        uint64_t page_offset : 12;
        uint64_t pt_index : 9;
        uint64_t pd_index : 9;
        uint64_t pdpt_index : 9;
        uint64_t pml4_index : 9;
        uint64_t sign_extension : 16;
    };
};

struct PageTableEntry {
    uint64_t present    : 1;   // bit 0
    uint64_t read_write   : 1;   // bit 1
    uint64_t user       : 1;   // bit 2
    uint64_t pwt        : 1;   // write-through
    uint64_t pcd        : 1;   // cache-disable
    uint64_t accessed   : 1;   // accessed
    uint64_t dirty      : 1;   // dirty (PT only)
    uint64_t huge       : 1;   // page size (PD/PT)
    uint64_t global     : 1;   // global
    uint64_t ignored1   : 3;
    uint64_t addr       : 40;  // physical frame addr >> 12
    uint64_t ignored2   : 11;
    uint64_t nx         : 1;   // no-execute (if supported)
};

struct __attribute__((packed)) GdtEntry {
    unsigned int limit_low              : 16; //base and limit are unused, as the flags control all accessable ram, not [base..base+limit] bytes in long mode
    unsigned int base_low               : 24;
    unsigned int accessed               :  1; // CPU sets this bit when a read happens to this segment
    unsigned int read_write             :  1; // true in code means R-X enables reading, true in data means RW- enables writing
    unsigned int conforming_expand_down :  1; // set to 0, otherwise lower privilege levels can access this?
    unsigned int code                   :  1; // 1 for code, 0 for data
    unsigned int code_data_segment      :  1; // should be 1 for everything but TSS and LDT
    unsigned int DPL                    :  2; // privilege level
    unsigned int present                :  1; // always 1
    unsigned int limit_high             :  4;
    unsigned int available              :  1; // always 0
    unsigned int long_mode              :  1; // same as code, (x64 instructions ONLY - 64 bit data must have this set to 0)
    unsigned int big                    :  1; // complicated - long mode code should set 0, data should be 1
    unsigned int gran                   :  1; // 1, unused as relates to limit
    unsigned int base_high              :  8;
};

struct __attribute__((packed)) TssEntry {
    uint32_t reserved0;
    uint64_t rsp0;   // Stack pointer for ring 0
    uint64_t rsp1;   // (optional, unused in most kernels)
    uint64_t rsp2;   // (optional)
    uint64_t reserved1;
    uint64_t ist1;   // Interrupt Stack Table entries
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base; // offset to I/O permission bitmap
};

struct __attribute__((packed)) TssDescriptor {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  type : 4;
    uint8_t  zero : 1;
    uint8_t  dpl : 2;
    uint8_t  present : 1;
    uint8_t  limit_high : 4;
    uint8_t  available : 1;
    uint8_t  zero2 : 2;
    uint8_t  granularity : 1;
    uint8_t  base_high;
    uint32_t base_upper; // upper 32 bits of 64-bit base
    uint32_t reserved;   // must be zero
};

struct __attribute__((packed)) GdtTable {
    uint64_t blank;
    struct GdtEntry kern_code;
    struct GdtEntry kern_data;
    struct GdtEntry user_code;
    struct GdtEntry user_data;
    struct TssDescriptor tss_descriptor;//points to tss_entry
};

static struct PageTableEntry* original_kernel_page_table;//on init, this was the page table
static struct GdtTable gdt_table;
static struct TssEntry tss_entry;
static struct __attribute__((packed)) GdtTablePtr {uint16_t limit; void* base;} gdt_table_ptr = {.limit = sizeof(struct GdtTable) - 1, .base = &gdt_table};

#define BIG_AMOUNT_OF_RAM (1ul << 38)

//how much to add to physical addresses to get virtual addresses
static uint64_t hhdm_offset;

void *mmio_start;

struct PageTableEntry *get_pml4_virt() {
    return (struct PageTableEntry*)(get_pml4_phys() + hhdm_offset);
}

//gets the n'th child of parent - parent can't be a pt as that points to allocated memory, not a list of children
struct PageTableEntry *get_page_table_child(struct PageTableEntry* parent, int n) {
    uint64_t child_arr_phys = parent->addr << 12;
    if(!parent->present) HCF
    if(n < 0 || n >= 512) HCF
    struct PageTableEntry* child_arr = (struct PageTableEntry*)(child_arr_phys + hhdm_offset);

    return child_arr + n;
}

uint64_t generate_clean_virtual_addressing() {
    //allocate memory for pml4 
    uint64_t pml4_phys = malloc4k_phys();
    struct PageTableEntry* pml4_hhdm = (struct PageTableEntry*)(pml4_phys + hhdm_offset);

    //copy the original kernel page table
    memcpy(pml4_hhdm, original_kernel_page_table, PAGE_SIZE);

    for(unsigned int i=0; i<256; i++) {
        if(pml4_hhdm->present) {
            HCF//how is a userland page already allocated here?
        }
    }
    return pml4_phys;
}

//for each virtual page allocated, calls leaf_callback(virtual page start, physical location of page) then once each page at each level is exhausted, calls table_entry_callback(physical location of start of array)
static void walk_virtual_tree(uint64_t page_table_phys, void (*leaf_callback)(void* virt, uint64_t phys), void (*table_entry_callback)(uint64_t table_start_phys)) {
    struct PageTableEntry* pml4_table = (struct PageTableEntry*)(page_table_phys + hhdm_offset);

    for(int pml4_idx=0; pml4_idx < 256; pml4_idx++) {//up to 255 so no walking kernel pages
        struct PageTableEntry* pml4 = pml4_table + pml4_idx;
        if(!pml4->present) continue;

        for(int pdpt_idx=0; pdpt_idx < 512; pdpt_idx++) {
            struct PageTableEntry* pdpt = get_page_table_child(pml4, pdpt_idx);
            if(!pdpt->present) continue;

            for(int pd_idx=0; pd_idx < 512; pd_idx++) {
                struct PageTableEntry* pd = get_page_table_child(pdpt, pd_idx);
                if(!pd->present) continue;

                for(int pt_idx=0; pt_idx < 512; pt_idx++) {
                    struct PageTableEntry* pt = get_page_table_child(pd, pt_idx);
                    if(!pt->present) continue;

                    uint64_t phys_ptr = pt->addr << 12;

                    union PointerBitmap virt_ptr = {
                        .sign_extension = 0,//no walking kernel pages
                        .pml4_index = pml4_idx,
                        .pdpt_index = pdpt_idx,
                        .pd_index = pd_idx,
                        .pt_index = pt_idx,
                        .page_offset = 0,
                    };
                    leaf_callback(virt_ptr.raw_ptr, phys_ptr);
                }

                table_entry_callback(pd->addr << 12);
            }
            table_entry_callback(pdpt->addr << 12);
        }
        table_entry_callback(pml4->addr << 12);
    }
    table_entry_callback(page_table_phys);
}

//a leaf callback, that frees the physical page
static void leaf_callback_remove_page(void*, uint64_t phys) {
    free4k_phys(phys);
}
//a table entry callback, that frees the physical page
static void table_entry_callback_remove_page(uint64_t phys) {
    free4k_phys(phys);
}

void remove_virtual_addressing() {
    uint64_t tree_root_phys = get_pml4_phys();
    set_pml4_phys((uint64_t)original_kernel_page_table - hhdm_offset);//reset CR3 to default
    walk_virtual_tree(tree_root_phys, leaf_callback_remove_page, table_entry_callback_remove_page);
}

static void leaf_callback_clone_page(void* virt, uint64_t phys) {
    allocate_ram_page(virt);
    void* phys_hhdm = (void*)(phys + hhdm_offset);
    memcpy(virt, phys_hhdm, PAGE_SIZE);
}

static void table_entry_callback_do_nothing(uint64_t) {}

uint64_t clone_virtual_addressing(uint64_t original_root_phys) {
    uint64_t result = generate_clean_virtual_addressing();
    set_pml4_phys(result);//temporarily use new virtual addressing to populate it
    walk_virtual_tree(original_root_phys, leaf_callback_clone_page, table_entry_callback_do_nothing);
    set_pml4_phys(original_root_phys);//set it back
    return result;
}

static void setup_page_table_entry(struct PageTableEntry* parent) {
    if(parent->present) {HCF}//should only operate on parent with no child

    uint64_t new_entry_phys = malloc4k_phys();
    struct PageTableEntry* new_entry_hhdm = (struct PageTableEntry*)(new_entry_phys + hhdm_offset);

    for(int i=0; i<512; i++) {
        new_entry_hhdm[i].present = false;
        new_entry_hhdm[i].addr = 0;
    }

    parent->global = false;//when cr3 is overwritten, clear this from cache
    parent->addr = new_entry_phys >> 12;//shift by 12 as it is 4k aligned, so they should be zeroes anyways
    parent->huge = 0;//4k page please
    parent->pcd = false;//don't disable caches
    parent->pwt = true;//some cache thing
    parent->user = true;//user mode
    parent->read_write = true;
    parent->present = true;//since it is in memory now
    parent->nx = false;//don't want execute disable
}

/// Generate a PML4 only for kernel addresses, but doesn't allocate any pages, just sets up the first branches of the tree
static void fill_kernel_pml4() {
    const int kern_pml4_start = 1 << 8;
    const int kern_pml4_end = (1 << 9) - 1;

    struct PageTableEntry*const pml4_base = (struct PageTableEntry*)(get_pml4_phys() + hhdm_offset);
    for(int i=kern_pml4_start; i <= kern_pml4_end; i++) {
        struct PageTableEntry* my_pml4 = &pml4_base[i];//get destination of pml4 entry
        if(my_pml4->present) continue;//already allocated
        setup_page_table_entry(my_pml4);
    }
}

void map_page(uint64_t physical_addr, void* virtual_addr) {
    union PointerBitmap virt_addr_bitmap;
    virt_addr_bitmap.raw_ptr = virtual_addr;

    if(virt_addr_bitmap.page_offset != 0) {
        HCF//pointer to the middle of a page???
    }

    struct PageTableEntry* pml4_base = (struct PageTableEntry*)(get_pml4_phys() + hhdm_offset);
    struct PageTableEntry* my_pml4 = &pml4_base[virt_addr_bitmap.pml4_index];//get *my* pml4 entry

    if(!my_pml4->present) {
        setup_page_table_entry(my_pml4);
    }

    struct PageTableEntry* pdpt_base = (struct PageTableEntry*)((my_pml4->addr << 12) + hhdm_offset);
    struct PageTableEntry* my_pdpt = &pdpt_base[virt_addr_bitmap.pdpt_index];//get *my* pdpt entry

    if(!my_pdpt->present) {
        setup_page_table_entry(my_pdpt);
    }

    struct PageTableEntry* pd_base = (struct PageTableEntry*)((my_pdpt->addr << 12) + hhdm_offset);
    struct PageTableEntry* my_pd = &pd_base[virt_addr_bitmap.pd_index];

    if(!my_pd->present) {
        setup_page_table_entry(my_pd);
    }

    struct PageTableEntry* pt_base = (struct PageTableEntry*)((my_pd->addr << 12) + hhdm_offset);
    struct PageTableEntry* my_pt = &pt_base[virt_addr_bitmap.pt_index];

    if(my_pt->present) {
        HCF
    }//virtual address already allocated!

    if(physical_addr & 0xFFF) HCF

    my_pt->global = false;//when cr3 is overwritten, clear this from cache
    my_pt->addr = physical_addr >> 12;//shift by 12 as it is 4k aligned, so they should be zeroes anyways
    my_pt->huge = 0;//4k page please
    my_pt->pcd = false;//don't disable caches
    my_pt->pwt = true;//some cache thing
    my_pt->user = true;//user mode
    my_pt->read_write = true;
    my_pt->present = true;//since it is in memory now
    my_pt->nx = false;//don't want execute disable

    //refresh the cache
    invalidate_page(virtual_addr);
}

void allocate_ram_page(void* virtual_addr) {
    uint64_t new_page_phys = malloc4k_phys();
    map_page(new_page_phys, virtual_addr);
}

void deallocate_page(void* virtual_addr) {
    union PointerBitmap virt_addr_bitmap;
    virt_addr_bitmap.raw_ptr = virtual_addr;

    if(virt_addr_bitmap.page_offset != 0) {
        HCF//pointer to the middle of a page???
    }

    struct PageTableEntry* pml4_base = (struct PageTableEntry*)(get_pml4_phys() + hhdm_offset);
    struct PageTableEntry* my_pml4 = &pml4_base[virt_addr_bitmap.pml4_index];//get *my* pml4 entry

    if(!my_pml4->present) {
        HCF //double free? how did I free a non-allocated address?
    }

    struct PageTableEntry* pdpt_base = (struct PageTableEntry*)((my_pml4->addr << 12) + hhdm_offset);
    struct PageTableEntry* my_pdpt = &pdpt_base[virt_addr_bitmap.pdpt_index];//get *my* pdpt entry

    if(!my_pdpt->present) {
        HCF //double free? how did I free a non-allocated address?
    }

    struct PageTableEntry* pd_base = (struct PageTableEntry*)((my_pdpt->addr << 12) + hhdm_offset);
    struct PageTableEntry* my_pd = &pd_base[virt_addr_bitmap.pd_index];

    if(!my_pd->present) {
        HCF //double free? how did I free a non-allocated address?
    }

    struct PageTableEntry* pt_base = (struct PageTableEntry*)((my_pd->addr << 12) + hhdm_offset);
    struct PageTableEntry* my_pt = &pt_base[virt_addr_bitmap.pt_index];

    if(!my_pt->present) {
        HCF //double free? how did I free a non-allocated address?
    }

    size_t page_phys = my_pt->addr << 12; //shift by 12 as it is 4k aligned, so they should be zeroes anyways
    free4k_phys(page_phys);
    my_pt->present = false;

    //refresh the cache
    invalidate_page(virtual_addr);
}

static struct GdtEntry generate_gdt_entry(bool is_code, unsigned int privilege_level) {
    return (struct GdtEntry) {
        .limit_low = 0, .limit_high = 0, .base_low=0, .base_high = 0,//these are unused in long mode
        .accessed = 0,
        .read_write = true,
        .conforming_expand_down = 0,
        .code = is_code,
        .code_data_segment = true,
        .DPL = privilege_level,
        .present = true,
        .available = 0,
        .long_mode = is_code,//depends on if is code for some reason?
        .big = !is_code,
        .gran = 1,

    };
}

static void init_tss_descriptor(struct TssDescriptor *desc, struct TssEntry *tss) {
    uint64_t tss_base = (uint64_t)tss;
    uint32_t tss_limit = sizeof(struct TssEntry) - 1;
    
    // Set the limit (size of TSS - 1)
    desc->limit_low = tss_limit & 0xFFFF;
    desc->limit_high = (tss_limit >> 16) & 0xF;
    
    // Set the base address (64-bit address split across multiple fields)
    desc->base_low = tss_base & 0xFFFF;
    desc->base_middle = (tss_base >> 16) & 0xFF;
    desc->base_high = (tss_base >> 24) & 0xFF;
    desc->base_upper = (tss_base >> 32) & 0xFFFFFFFF;
    
    // TSS descriptor type and flags
    desc->type = 0x9;        // Available 64-bit TSS (type 9)
    desc->zero = 0;          // Must be 0
    desc->dpl = 0;           // Descriptor Privilege Level (kernel = 0)
    desc->present = 1;       // Present bit
    desc->available = 0;     // Available for software use
    desc->zero2 = 0;         // Must be 0
    desc->granularity = 0;   // Byte granularity (not 4KB pages)
    desc->reserved = 0;      // Must be 0
}

/// Sets special registers to point to gdt things
/// disables interrupts
extern void apply_gdt_tss(struct GdtTablePtr* gdt_base);

void memory_init(volatile struct limine_memmap_response *memmap_response, uint64_t hhdm_ofs) {
    hhdm_offset = hhdm_ofs;
    original_kernel_page_table = (struct PageTableEntry*)(get_pml4_phys() + hhdm_offset);//cr3 is the physical address

    struct limine_memmap_entry *result = NULL;
    for(uint64_t range_idx=0; range_idx < memmap_response->entry_count; range_idx++) {
        result = memmap_response->entries[range_idx];
        //find a contiguous memory region
        if(result->length >= 1000000000 && result->type == LIMINE_MEMMAP_USABLE) {
            break;
        }
    }

    if(result == NULL) {
        HCF//didn't find any useful ram
    }

    void* base_virt_hhdm = (void*)(result->base + hhdm_offset);
    init_physical_memory(result->base, base_virt_hhdm, result->length);

    void* kheap_start = base_virt_hhdm + BIG_AMOUNT_OF_RAM;
    mmio_start = kheap_start + BIG_AMOUNT_OF_RAM;
    if(mmio_start + BIG_AMOUNT_OF_RAM >= (void*)0xffffffff80000000) {
        //I have overflowed into kernel instruction space
        HCF
    }

    fill_kernel_pml4();//fill the original kernel page table with empty entries, so EVERYONE will share the kernel space
    kmalloc_init(kheap_start);

    //set up GDT and TSS
    memset(&tss_entry, 0, sizeof(struct TssEntry));
    tss_entry.rsp0 = (uint64_t)interrupt_stack_top;
    tss_entry.iomap_base = sizeof(struct TssEntry);

    gdt_table.blank = 0;//first entry should be 0
    gdt_table.kern_code = generate_gdt_entry(true, 0);
    gdt_table.kern_data = generate_gdt_entry(false, 0);

    gdt_table.user_code = generate_gdt_entry(true, 3);
    gdt_table.user_data = generate_gdt_entry(false, 3);

    init_tss_descriptor(&gdt_table.tss_descriptor, &tss_entry);
    
    apply_gdt_tss(&gdt_table_ptr);
}