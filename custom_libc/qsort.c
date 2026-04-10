#include <string.h>

void qsort(void *base, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *)) {

    char tmp[size];
    char *arr = (char *)base;

    for (size_t i = 1; i < nmemb; i++) {
        memcpy(tmp, arr + i * size, size);

        size_t j = i;
        while (j > 0 && compar(arr + (j - 1) * size, tmp) > 0) {
            memcpy(arr + j * size, arr + (j - 1) * size, size);
            j--;
        }

        memcpy(arr + j * size, tmp, size);
    }
}