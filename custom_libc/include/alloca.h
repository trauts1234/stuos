#ifndef ALLOCA_H
#define ALLOCA_H

#include <stddef.h>


#ifdef	__GNUC__
#define alloca(s) __builtin_alloca(s)
#else
void *alloca(size_t);
#endif

#endif