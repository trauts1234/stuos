#ifndef WCHAR_H
#define WCHAR_H

#include "stddef.h"
#include "stdint.h"

#define WEOF 0xffffffffU

typedef struct {
    wchar_t buffer;
    unsigned int expected_length;
    unsigned int expected_remaining_bytes;
} mbstate_t;

typedef unsigned int wint_t;
typedef int(*wctype_t)(wint_t);

wchar_t *wcschr (const wchar_t *, wchar_t);
size_t wcslen (const wchar_t *);

int iswalnum(wint_t);
int iswalpha(wint_t);
int iswblank(wint_t);
int iswcntrl(wint_t);
int iswdigit(wint_t);
int iswgraph(wint_t);
int iswlower(wint_t);
int iswprint(wint_t);
int iswpunct(wint_t);
int iswupper(wint_t);
int iswxdigit(wint_t);

wint_t    towlower(wint_t);
wint_t    towupper(wint_t);

wctype_t wctype(const char *in);

size_t mbrtowc(wchar_t *pwc, const char *s, size_t n, mbstate_t *ps);
size_t mbsrtowcs(wchar_t *dest, const char **src, size_t len, mbstate_t *ps);
size_t mbrlen(const char *s, size_t n, mbstate_t *ps);


#endif