#ifndef MINIMAL_LIBC_H
#define MINIMAL_LIBC_H

#include <uapi/stdint.h>
#include <uapi/stddef.h>

int printf(const char* format, ...);

int strcmp(const char *str1, const char *str2);
char *strcpy(char *destination, const char *source);

void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
size_t strlen(const char* str);
int strncmp(const char *s1, const char *s2, size_t n);

int toupper(int c);

void* malloc(size_t size);
void free(void* ptr);

#endif