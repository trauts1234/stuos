#ifndef UAPI_STDARG_H
#define UAPI_STDARG_H

#ifdef __TINYC__
//embed tiny c compiler's header
typedef __builtin_va_list va_list;
#define va_start __builtin_va_start
#define va_arg __builtin_va_arg
#define va_copy __builtin_va_copy
#define va_end __builtin_va_end

/* fix a buggy dependency on GCC in libio.h */
typedef va_list __gnuc_va_list;
#else
//use compiler's own header
#include <stdarg.h>
#endif

#endif