#ifndef _STDDEF_H
#define _STDDEF_H

#include "uapi/stddef.h"
#include "stdint.h"
typedef uint32_t wchar_t;

#define offsetof(type, field) ((size_t) &((type *)0)->field)

#endif