/**
 * stdint.h - Fixed-width integer types and limits (C99 standard)
 * 
 * This implementation provides the standardized integer types with specified
 * widths as defined in the C99 standard (ISO/IEC 9899:1999).
 */

#ifndef UAPI_STDINT_H
#define UAPI_STDINT_H

/* Exact-width integer types */
typedef signed char        int8_t;
typedef unsigned char      uint8_t;
typedef signed short       int16_t;
typedef unsigned short     uint16_t;
typedef signed int         int32_t;
typedef unsigned int       uint32_t;
typedef signed long long   int64_t;
typedef unsigned long long uint64_t;

/* Minimum-width integer types */
typedef signed char        int_least8_t;
typedef unsigned char      uint_least8_t;
typedef signed short       int_least16_t;
typedef unsigned short     uint_least16_t;
typedef signed int         int_least32_t;
typedef unsigned int       uint_least32_t;
typedef signed long long   int_least64_t;
typedef unsigned long long uint_least64_t;

/* Fastest minimum-width integer types */
typedef signed char        int_fast8_t;
typedef unsigned char      uint_fast8_t;
typedef signed int         int_fast16_t;
typedef unsigned int       uint_fast16_t;
typedef signed int         int_fast32_t;
typedef unsigned int       uint_fast32_t;
typedef signed long long   int_fast64_t;
typedef unsigned long long uint_fast64_t;

/* Integer types capable of holding object pointers */
typedef long           intptr_t;
typedef unsigned long  uintptr_t;

/* Greatest-width integer types */
typedef signed long long   intmax_t;
typedef unsigned long long uintmax_t;

/* Limits of exact-width integer types */
#define INT8_MIN         (-128)
#define INT8_MAX         127
#define UINT8_MAX        255

#define INT16_MIN        (-32768)
#define INT16_MAX        32767
#define UINT16_MAX       65535

#define INT32_MIN        (-2147483648)
#define INT32_MAX        2147483647
#define UINT32_MAX       4294967295U

#define INT64_MIN        (-9223372036854775807LL - 1)
#define INT64_MAX        9223372036854775807LL
#define UINT64_MAX       18446744073709551615ULL

/* Limits of minimum-width integer types */
#define INT_LEAST8_MIN   INT8_MIN
#define INT_LEAST8_MAX   INT8_MAX
#define UINT_LEAST8_MAX  UINT8_MAX

#define INT_LEAST16_MIN  INT16_MIN
#define INT_LEAST16_MAX  INT16_MAX
#define UINT_LEAST16_MAX UINT16_MAX

#define INT_LEAST32_MIN  INT32_MIN
#define INT_LEAST32_MAX  INT32_MAX
#define UINT_LEAST32_MAX UINT32_MAX

#define INT_LEAST64_MIN  INT64_MIN
#define INT_LEAST64_MAX  INT64_MAX
#define UINT_LEAST64_MAX UINT64_MAX

/* Limits of fastest minimum-width integer types */
#define INT_FAST8_MIN    INT8_MIN
#define INT_FAST8_MAX    INT8_MAX
#define UINT_FAST8_MAX   UINT8_MAX

#define INT_FAST16_MIN   INT32_MIN
#define INT_FAST16_MAX   INT32_MAX
#define UINT_FAST16_MAX  UINT32_MAX

#define INT_FAST32_MIN   INT32_MIN
#define INT_FAST32_MAX   INT32_MAX
#define UINT_FAST32_MAX  UINT32_MAX

#define INT_FAST64_MIN   INT64_MIN
#define INT_FAST64_MAX   INT64_MAX
#define UINT_FAST64_MAX  UINT64_MAX

/* Limits of integer types capable of holding object pointers */
#if defined(_LP64) || defined(__LP64__)
#define INTPTR_MIN       INT64_MIN
#define INTPTR_MAX       INT64_MAX
#define UINTPTR_MAX      UINT64_MAX
#else
#define INTPTR_MIN       INT32_MIN
#define INTPTR_MAX       INT32_MAX
#define UINTPTR_MAX      UINT32_MAX
#endif

/* Limits of greatest-width integer types */
#define INTMAX_MIN       INT64_MIN
#define INTMAX_MAX       INT64_MAX
#define UINTMAX_MAX      UINT64_MAX

/* Limits of other integer types */
#define PTRDIFF_MIN      INTPTR_MIN
#define PTRDIFF_MAX      INTPTR_MAX

#define SIZE_MAX         UINTPTR_MAX

/*
#define INT8_C(val)      (val)
#define UINT8_C(val)     (val)
#define INT16_C(val)     (val)
#define UINT16_C(val)    (val)
#define INT32_C(val)     (val)
#define UINT32_C(val)    (val ## U)
#define INT64_C(val)     (val ## LL)
#define UINT64_C(val)    (val ## ULL)

#define INTMAX_C(val)    (val ## LL)
#define UINTMAX_C(val)   (val ## ULL)
*/

#endif /* _STDINT_H */