#include "uapi/stdint.h"
#include <stddef.h>
#include <stdbool.h>
#include "rust_bindings.h"
#include "debugging.h"
#include "memory.h"
#include "physical_slab_allocation.h"

static struct MemoryAllocator kmalloc_data;

static uint64_t expand_kheap(void*kheap_end, uint64_t size) {
    if((uint64_t)kheap_end & PAGE_MASK) {HCF}//must be page aligned
    uint64_t rounded_up_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;

    for(uint64_t i = 0; i<rounded_up_pages; i++) {
        void *const ptr = kheap_end + (PAGE_SIZE * i);
        allocate_ram_page(ptr);
    }

    return PAGE_SIZE * rounded_up_pages;
}

void kmalloc_init(void* kernel_heap_base_virt) {
    init_memory_allocator(expand_kheap, kernel_heap_base_virt, &kmalloc_data);
}
void* kmalloc(uint64_t size){
    return allocate(&kmalloc_data, size);
}
void kfree(void* ptr){
    deallocate(&kmalloc_data, ptr);
}

int strcmp(const char *str1, const char *str2) {
    int i = 0;
    while(str1[i] == str2[i]) {
        if(str1[i] == '\0' || str2[i] == '\0') {
            break;
        }
        i++;
    }
    if(str1[i] == '\0' && str2[i] == '\0') {
        return 0;
    }
    else {
        return str1[i] - str2[i];
    }
}

char *strcpy(char *dest, const char *src)
{
   char *save = dest;
   while((*dest++ = *src++));
   return save;
}

// GCC and Clang reserve the right to generate calls to the following
// 4 functions even if they are not directly called.
// Implement them as the C specification mandates.
// DO NOT remove or rename these functions, or stuff will eventually break!
// They CAN be moved to a different .c file.

void *memcpy(void *restrict dest, const void *restrict src, size_t n) {
    uint8_t *restrict pdest = (uint8_t *restrict)dest;
    const uint8_t *restrict psrc = (const uint8_t *restrict)src;

    for (size_t i = 0; i < n; i++) {
        pdest[i] = psrc[i];
    }

    return dest;
}

void *memset(void *s, int c, size_t n) {
    uint8_t *p = (uint8_t *)s;

    for (size_t i = 0; i < n; i++) {
        p[i] = (uint8_t)c;
    }

    return s;
}

void *memmove(void *dest, const void *src, size_t n) {
    uint8_t *pdest = (uint8_t *)dest;
    const uint8_t *psrc = (const uint8_t *)src;

    if (src > dest) {
        for (size_t i = 0; i < n; i++) {
            pdest[i] = psrc[i];
        }
    } else if (src < dest) {
        for (size_t i = n; i > 0; i--) {
            pdest[i-1] = psrc[i-1];
        }
    }

    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *p1 = (const uint8_t *)s1;
    const uint8_t *p2 = (const uint8_t *)s2;

    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] < p2[i] ? -1 : 1;
        }
    }

    return 0;
}

size_t strlen(const char* str) {
    size_t result = 0;
    while(*str++ != '\0') {
        result++;
    }
    return result;
}

int toupper(int c) {
    if(c >= 'a' && c <= 'z') {
        return c + ('A' - 'a');
    }

    return c;
}