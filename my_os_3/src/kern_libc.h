#ifndef MINIMAL_LIBC_H
#define MINIMAL_LIBC_H

#include "uapi/stdint.h"
#include <stddef.h>

void kmalloc_init(void* kernel_heap_base_virt);
void* kmalloc(uint64_t size);
// void* krealloc(void* original_ptr, uint64_t new_size);
void kfree(void* ptr);

int kprintf(const char* format, ...);

int strcmp(const char *str1, const char *str2);
char *strcpy(char *destination, const char *source);

void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
size_t strlen(const char* str);


#endif