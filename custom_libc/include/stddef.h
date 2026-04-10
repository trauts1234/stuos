#ifndef _STDDEF_H
#define _STDDEF_H

typedef long int ptrdiff_t;
typedef long unsigned int size_t;
typedef long signed int ssize_t;//seems to be a weird POSIX thing?
#define NULL ((void*)0)

#endif