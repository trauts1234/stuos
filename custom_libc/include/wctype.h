#ifndef WCTYPE_H
#define WCTYPE_H

#include "wchar.h"

int iswspace(wint_t);

int iswctype(wint_t wc, wctype_t desc);

#endif