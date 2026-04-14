#include "elf.h"
#include "debugging.h"
#include "fs.h"
#include "kern_libc.h"
#include "physical_slab_allocation.h"
#include "memory.h"
#include "pipes_and_files.h"
#include "scheduling.h"

struct __attribute__((__packed__)) ElfFile {
    uint8_t file_header[4];//should be 0x7f, E, L, F
    uint8_t bit_size_flag;//1 for 32 bit, 2 for 64 bit
    uint8_t endianness;//should be 1 for little endian
    uint8_t header_version;
    uint8_t os_abi;//0 for sysv abi
    uint8_t padding[8];
    uint16_t type;// 1 = relocatable, 2 = executable, 3 = shared, 4 = core
    uint16_t instruction_set;//0x3E for x64
    uint32_t elf_version;//should be 1
    uint64_t entry_offset;//where the code starts executing from
    uint64_t program_header_table_offset;
    uint64_t section_header_table_offset;
    uint32_t flags;//unused on x86
    uint16_t header_size;
    uint16_t program_header_table_entry_size;
    uint16_t program_header_table_num_entries;
    uint16_t section_header_table_entry_size;
    uint16_t section_header_table_num_entries;
    uint16_t section_index_header_string_table;//Section index to the section header string table ???
};

struct ElfProgramHeader {
    uint32_t segment_type;//0 = ignore entry; 1 = load; 2 = dynamic - requires dynamic linking; 3 = interp - contains a file path to an executable to use as an interpreter for the following segment; 4 = note section
    uint32_t flags;//0b1 = x, 0b10 = w, 0b100 = r
    uint64_t p_offset;//where the data is in the file
    uint64_t p_vaddr;//where the virtual address is
    uint64_t ignored;//the ELF can request a specific physical address, but we will ignore this
    uint64_t p_filesz;//how much data to read from the file
    uint64_t p_memsz;// >= p_filesz, size of segment in memory (remainder after p_filesz is zeroed)
    uint64_t required_alignment;//request an alignment of n bytes (0 and 1 mean no alignment)
};

/// @brief Linked list node, storing data about a free memory region
/// This is used when setting up an ELF, to work out which regions of virtual userland memory are free
struct VirtualMemoryRegion {
    bool is_free;
    /// Must point to the start of a page
    void* start_addr;
    /// Must point to the start of a page
    void* end_addr;//exclusive
    struct VirtualMemoryRegion* next;
};

/// Allocates addresses from page_start up to but not including page_end for the elf
static void allocate_virtual_range(struct VirtualMemoryRegion* virtual_memory_tracker_head, void* page_start, void* page_end) {
    if((size_t)page_start & PAGE_MASK || (size_t)page_end & PAGE_MASK) {HCF}//not page-aligned

    struct VirtualMemoryRegion* curr = virtual_memory_tracker_head;
    while (!(curr->start_addr <= page_start && page_end <= curr->end_addr))
    {
        if(curr->next == NULL) {HCF}
        if(curr->is_free && curr->next->is_free) {
            if(curr->end_addr != curr->next->start_addr) {HCF}
            curr->end_addr = curr->next->end_addr;//update curr to talk about the combined free region
            curr->next = curr->next->next;
            // shift_remove(elf, curr->next);//remove curr->next as curr represents the combined region
            continue;
        }
        curr = curr->next;
    }

    if(!curr->is_free) {HCF}//tried to allocate an already-allocated region
    //now I have the segment that wraps round the entire range that needs allocating
    // I am replacing a -> curr -> z with:
    // a -> curr -> allocated_region -> next_free -> z

    struct VirtualMemoryRegion* allocated_region = kmalloc(sizeof(struct VirtualMemoryRegion));
    struct VirtualMemoryRegion* next_free = kmalloc(sizeof(struct VirtualMemoryRegion));
    //the next free region after the allocated region is from the end of the allocated region to the segment we are replacing's end
    next_free->is_free = true;
    next_free->start_addr = page_end;
    next_free->end_addr = curr->end_addr;
    next_free->next = curr->next;
    //allocated region is self-explanatory
    allocated_region->is_free = false;
    allocated_region->start_addr = page_start;
    allocated_region->end_addr = page_end;
    allocated_region->next = next_free;//point to the free region after
    //point curr to the allocated region
    curr->next = allocated_region;
    curr->end_addr = page_start;//stop before the allocated segment

    //allocate the physical memory and make the virtual addresses that I just marked as free point to the memory
    for(void* page_ptr = page_start; page_ptr < page_end; page_ptr += PAGE_SIZE) {
        allocate_page(page_ptr);//also zeroes the page
    }
}

/// @brief finds a contiguous region of virtual memory addresses
/// @param size_bytes how much contiguous virtual addressing you need
/// @return pointer to the start of the range (but doesn't allocate anything)
static void* find_contiguous_virtual_range(struct VirtualMemoryRegion* virtual_memory_tracker_head, size_t size_bytes) {
    struct VirtualMemoryRegion* curr = virtual_memory_tracker_head;
    while(1) {
        if(curr->is_free && curr->end_addr - curr->start_addr >= size_bytes) {
            return curr->start_addr;//found a good memory region
        }

        if (curr->next == NULL) return NULL;//out of virtual addresses

        if(curr->end_addr == curr->next->start_addr && curr->is_free && curr->next->is_free) {
            //two contiguous memory regions!
            //merge them!
            curr->end_addr = curr->next->end_addr;//update curr to talk about the combined free region
            kfree(curr->next);
            curr->next = curr->next->next;
            continue;//try again
        }
        curr = curr->next;//try next region
    }
}

/// Allocates in userspace the array and strings for all args, setting output_allocated_argc and output_argv accordingly
static void setup_args_for_new_process(struct VirtualMemoryRegion* virtual_memory_tracker_head, char*const *argv, char*** output_allocated_argv, uint64_t *output_argc) {

    //count args, and work out how much memory is required
    uint64_t argc;
    uint64_t num_bytes_for_args_and_argc = 0;
    for(argc = 0; argv[argc]; argc++) {
        num_bytes_for_args_and_argc += sizeof(char*);//extra room for element in the array
        num_bytes_for_args_and_argc += strlen(argv[argc]) + 1;//extra room for the arg itself
    }
    //to include the null pointer at the end
    num_bytes_for_args_and_argc += sizeof(char*);

    num_bytes_for_args_and_argc = (num_bytes_for_args_and_argc + PAGE_SIZE-1) & ~PAGE_MASK;//round up

    void* args_data_start = find_contiguous_virtual_range(virtual_memory_tracker_head, num_bytes_for_args_and_argc);
    void* args_data_end = args_data_start + num_bytes_for_args_and_argc;
    allocate_virtual_range(virtual_memory_tracker_head, args_data_start, args_data_end);

    *output_allocated_argv = args_data_start;//array of char pointers is at the start
    *output_argc = argc;
    
    //walk through the data, putting in the strings and array
    char* curr_offset = args_data_start + argc * sizeof(char*);//start saving the actual strings after the array
    for(uint64_t i=0; i<argc; i++) {
        (*output_allocated_argv)[i] = curr_offset;//add array item

        strcpy(curr_offset, argv[i]);//write the string into memory if it isn't null
        curr_offset += strlen(argv[i]) + 2;//skip null byte and point to the next string start
    }
    (*output_allocated_argv)[argc] = NULL;//set the last pointer to NULL
}

struct LoadedProgram instantiate_ELF(struct VNode exe, char*const *argv) {

    //read the header from the file
    struct ElfFile header;
    if(exe.read_file(exe.inode_number, 0, (uint8_t*)&header, sizeof(header)) != sizeof(header)) {HCF};

    const uint8_t expected_magic[4] = {0x7f, 'E', 'L', 'F'};
    if(memcmp(header.file_header, expected_magic, 4)) {
        HCF//incorrect magic
    }

    if(header.bit_size_flag != 2) {
        HCF//not 64 bit
    }

    if(header.endianness != 1) {
        HCF//not little endian
    }

    if(header.header_version != 1) {
        HCF//wrong ELF version ??
    }
    

    if(header.os_abi != 0) {
        HCF//not SYSV ABI
    }

    if(header.type != 2) {
        HCF//not executable - perhaps it is a modern linux binary where they have shared libraries that are actually executable?
    }

    if(header.instruction_set != 0x3E) {
        HCF//wrong instruction set
    }

    if(header.elf_version != 1) {
        HCF//wrong ELF version???
    }
    if(header.header_size != sizeof(struct ElfFile)) {
        HCF// ELF thinks the header is a different size?
    }
    if(header.program_header_table_entry_size != sizeof(struct ElfProgramHeader)) {
        HCF//each program header should be the size of a program header???
    }

    //generate a new page table
    uint64_t prev_cr3 = get_pml4_phys();
    uint64_t new_cr3 = generate_clean_virtual_addressing();
    set_pml4_phys(new_cr3);

    struct VirtualMemoryRegion* virtual_memory_tracker_head;

    virtual_memory_tracker_head = kmalloc(sizeof(struct VirtualMemoryRegion));
    virtual_memory_tracker_head->start_addr = (void*)PAGE_SIZE;//skip first page
    virtual_memory_tracker_head->end_addr = (void*)0x00007FFFFFFFFFFF;//lower cannonical half
    virtual_memory_tracker_head->next = NULL;
    virtual_memory_tracker_head->is_free = true;

    //read the program headers from disk
    uint64_t headers_num_bytes = header.program_header_table_num_entries * header.program_header_table_entry_size;
    struct ElfProgramHeader* prog_headers = kmalloc(headers_num_bytes);
    if(exe.read_file(exe.inode_number, header.program_header_table_offset, (uint8_t*)prog_headers, headers_num_bytes) != headers_num_bytes) {HCF}

    for(size_t i=0; i<header.program_header_table_num_entries; i++) {
        const struct ElfProgramHeader curr_header = prog_headers[i];//get the i'th program header

        switch (curr_header.segment_type) {
            case 1:break;//load segment
            case 1685382481:continue;//GNU_STACK (gives us permissions information but idc)
            default: HCF//not a load segment? not sure how to deal with this
        }

        if(curr_header.required_alignment != 0 && (curr_header.p_vaddr % curr_header.required_alignment != curr_header.p_offset % curr_header.required_alignment)) {
            //alignment required, and page is not aligned to the file offset
            //apparently ELF doesn't like this
            HCF
        }

        void* page_alloc_start = (void*)(curr_header.p_vaddr & ~PAGE_MASK);//find the page containing p_vaddr
        void* page_alloc_end = (void*)((curr_header.p_vaddr + curr_header.p_memsz + PAGE_SIZE-1) & ~PAGE_MASK);//round page up to find which is the first free page after the allocated region
        allocate_virtual_range(virtual_memory_tracker_head, page_alloc_start, page_alloc_end);

        exe.read_file(exe.inode_number, curr_header.p_offset, (uint8_t*)curr_header.p_vaddr, curr_header.p_filesz);//copy data from file
    }
    kfree(prog_headers);

    const size_t STACK_SIZE = 16 * PAGE_SIZE;//must be a multiple of PAGE_SIZE otherwise allocating a range will fail
    void* stack_virt_base = find_contiguous_virtual_range(virtual_memory_tracker_head, STACK_SIZE);
    allocate_virtual_range(virtual_memory_tracker_head, stack_virt_base, stack_virt_base + STACK_SIZE);
    void* stack_start = stack_virt_base + STACK_SIZE;//start stack at high address

    uint64_t argc;
    char** argv_in_userspace;
    setup_args_for_new_process(virtual_memory_tracker_head, argv, &argv_in_userspace, &argc);

    void* heap_virt_base = find_contiguous_virtual_range(virtual_memory_tracker_head, ram_size_bytes());

    //free the linked list of information
    struct VirtualMemoryRegion* ptr = virtual_memory_tracker_head;
    while (ptr) {
        struct VirtualMemoryRegion *next = ptr->next;
        kfree(ptr);
        ptr = next;
    }

    //reset the page table pointer
    set_pml4_phys(prev_cr3);//reset

    return (struct LoadedProgram) {
        .heap_start = heap_virt_base,
        .page_table_root = new_cr3,
        .file_descriptors = {fop_generate_stdin(), fop_generate_stdout(), fop_generate_stdout(), 0},// use stdout for stderr for the time being
        .initial_state = {
            .rip=header.entry_offset,
            .rbp=(uint64_t)stack_start,
            .rsp=(uint64_t)stack_start,
            .rsi=(uint64_t)argv_in_userspace,//argc pointer
            .rdi=argc,//argv value
            .rdx=0,//no envp arg
            .cs=0x1B,
            .ss=0x23,
            .rflags=0x202,
        }
    };
}