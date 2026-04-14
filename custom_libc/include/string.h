#ifndef _STRING_H
#define _STRING_H

#include "stddef.h"

void *memcpy(void *dest, const void *src, size_t n);
void *mempcpy(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
size_t strlen(const char* str);
int strcmp(const char *str1, const char *str2);
int strncmp(const char *s1, const char *s2, size_t n);

char *stpcpy(char *dest, const char *src);
char *stpncpy(char *dest, const char *src, size_t n);

char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);

char *strchr(const char *s, int c);
char *strchrnul(const char *s, int c);

size_t strspn(const char *s, const char *accept);
size_t strcspn(const char *s, const char *reject);

char *strdup(const char *s);

char *strtok(char *str, const char *delim);

char *strpbrk(const char *s, const char *accept);
char *strstr(const char *haystack, const char *needle);

char *strerror(int errnum);

#endif