#ifndef _STDLIB_H
#define _STDLIB_H

#include <stddef.h>
void* malloc(unsigned long bytes);
void free(void* ptr);

__attribute__((noreturn)) 
void abort();
__attribute__((noreturn)) 
void exit(int status);

int atoi(const char *nptr);
long int strtol(const char *nptr, char **endptr, int base);
long long int strtoll(const char *nptr, char **endptr, int base);

void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));

#endif