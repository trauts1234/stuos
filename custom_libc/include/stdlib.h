#ifndef _STDLIB_H
#define _STDLIB_H

#include "stddef.h"

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

void* malloc(unsigned long bytes);
void free(void* ptr);

__attribute__((noreturn)) 
void abort();
__attribute__((noreturn)) 
void exit(int status);

int atoi(const char *nptr);
double atof(const char *nptr);
long int strtol(const char *nptr, char **endptr, int base);
long long int strtoll(const char *nptr, char **endptr, int base);
double strtod(const char *nptr, char **endptr);

void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));

#endif