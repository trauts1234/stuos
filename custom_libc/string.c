#include "stdint.h"
#include "stdlib.h"
#include <stddef.h>
#include "string.h"

void *memcpy(void *restrict dest, const void *restrict src, size_t n) {
    uint8_t *restrict pdest = (uint8_t *restrict)dest;
    const uint8_t *restrict psrc = (const uint8_t *restrict)src;

    for (size_t i = 0; i < n; i++) {
        pdest[i] = psrc[i];
    }

    return dest;
}

void *mempcpy(void *dest, const void *src, size_t n) {
    uint8_t *restrict pdest = (uint8_t *restrict)dest;
    const uint8_t *restrict psrc = (const uint8_t *restrict)src;

    for (size_t i = 0; i < n; i++) {
        pdest[i] = psrc[i];
    }

    return dest + n;
}

void *memset(void *s, int c, size_t n) {
    uint8_t *p = (uint8_t *)s;

    for (size_t i = 0; i < n; i++) {
        p[i] = (uint8_t)c;
    }

    return s;
}

void *memmove(void *dest, const void *src, size_t n) {
    uint8_t *pdest = (uint8_t *)dest;
    const uint8_t *psrc = (const uint8_t *)src;

    if (src > dest) {
        for (size_t i = 0; i < n; i++) {
            pdest[i] = psrc[i];
        }
    } else if (src < dest) {
        for (size_t i = n; i > 0; i--) {
            pdest[i-1] = psrc[i-1];
        }
    }

    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *p1 = (const uint8_t *)s1;
    const uint8_t *p2 = (const uint8_t *)s2;

    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] < p2[i] ? -1 : 1;
        }
    }

    return 0;
}

size_t strlen(const char* str) {
    size_t result = 0;
    while(*str++) {
        result++;
    }
    return result;
}

int strcmp(const char *str1, const char *str2) {
    int i = 0;
    while(str1[i] == str2[i]) {
        if(str1[i] == '\0' || str2[i] == '\0') {
            break;
        }
        i++;
    }
    if(str1[i] == '\0' && str2[i] == '\0') {
        return 0;
    }
    else {
        return str1[i] - str2[i];
    }
}
int strncmp(const char *s1, const char *s2, size_t n) {
    while (n-- > 0) {
        //check for string incorrect
        if (*s1 != *s2)return *s1 - *s2;
        //check if strings have ended
        if (*s1 == '\0') break;

        ++s1; ++s2;
    }
    return 0;
}

char *stpcpy(char *dest, const char *src) {
    while ((*dest++ = *src++));
    return dest - 1;
}

char *stpncpy(char *dest, const char *src, size_t n) {
    while (n-- > 0) {
        if ((*dest++ = *src++) == '\0') break;
    }

    while (n-- > 0)
        *dest++ = '\0';

    return dest;
}

char *strcpy(char *dest, const char *src) {
    char* result = dest;
    while ((*dest++ = *src++));
    return result;
}
char *strncpy(char *dest, const char *src, size_t n) {
    size_t i;

    for (i = 0; i < n && src[i] != '\0'; i++)
        dest[i] = src[i];
    
    while(i < n)
        dest[i++] = '\0';

    return dest;
}

char *strchr(const char *s, int c) {
    while(1) {
        if(*s == c) return (char*)s;
        if(*s == 0) return NULL;
        s++;
    }
}

char *strchrnul(const char *s, int c) {
    while(1) {
        if(*s == c) return (char*)s;
        if(*s == 0) return (char*)s;
        s++;
    }
}

size_t strspn(const char *s, const char *accept) {
    size_t result = 0;

    while(1) {
        if(!strchr(accept, *s)) {
            return result;
        }
        s++;
        result++;
    }
}
size_t strcspn(const char *s, const char *reject) {
    size_t result = 0;

    while(1) {
        if(strchr(reject, *s)) {
            return result;
        }
        s++;
        result++;
    }
}

char *strdup(const char *s) {
    size_t length = strlen(s) + 1;

    char* new = malloc(length);
    strcpy(new, s);
    return new;
}

static const char * skip_delimiters(const char *p, const char *delimiters)
{
    for ( ; *p != '\0'; ++p) {
        if (!strchr(delimiters, *p))
            return p;
    }
    return NULL;
}

char *strtok(char *str, const char *delimiters)
{
    //keep track of currently processing string
    static char *strtok_next = NULL;

    //if new string is passed, start parsing that instead
    if (str != NULL) {
        strtok_next = str;
    }

    //if there's nothing to parse or I have reached the end of the string, stop
    if (strtok_next == NULL || *strtok_next == '\0') return NULL;

    //skip delimiters
    const char *start = skip_delimiters(strtok_next, delimiters);
    if (start == NULL) {
        strtok_next = NULL;
        return NULL;
    }

    //walk the string until I reach a delimiter
    const char *end = start;
    while (*end != '\0' && strchr(delimiters, *end) == NULL)
        ++end;

    /* 3. If we found a delimiter, terminate the token and set up for
       the next call; otherwise mark the end of string. */
    if (*end == '\0') {
        strtok_next = NULL;
    } else {
        *(char *)end = '\0';   /* overwrite delimiter with NUL */
        strtok_next = (char *)(end + 1);
    }

    return (char *)start;
}

char *strpbrk(const char *s, const char *accept) {
    for(;*s;s++) {
        for(size_t i = 0; accept[i]; i++) {
            if(*s == accept[i]) {
                return (char*)s;
            }
        }
    }
    return NULL;
}
char *strstr(const char *haystack, const char *needle) {
    size_t needle_size = strlen(needle);
    for(;*haystack; haystack++) {
        if(strncmp(needle, haystack, needle_size) == 0) {
            return (char*)haystack;//found needle in haystack
        }
    }
    return NULL;
}