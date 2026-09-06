#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "assert.h"

/* ---------- comparators ---------- */

static int cmp_int(const void *a, const void *b) {
    int ia = *(const int *)a;
    int ib = *(const int *)b;
    return (ia > ib) - (ia < ib);   /* avoids signed overflow from subtraction */
}

static int cmp_double(const void *a, const void *b) {
    double da = *(const double *)a;
    double db = *(const double *)b;
    return (da > db) - (da < db);
}

static int cmp_char(const void *a, const void *b) {
    return (int)(*(const unsigned char *)a) - (int)(*(const unsigned char *)b);
}

static int cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* Reverse-order comparator for ints */
static int cmp_int_desc(const void *a, const void *b) {
    return cmp_int(b, a);
}

/* ---------- helpers ---------- */

static int is_sorted_int(const int *arr, size_t n, int (*cmp)(const void *, const void *)) {
    for (size_t i = 1; i < n; i++)
        if (cmp(&arr[i-1], &arr[i]) > 0)
            return 0;
    return 1;
}

static int is_sorted_double(const double *arr, size_t n) {
    for (size_t i = 1; i < n; i++)
        if (arr[i-1] > arr[i])
            return 0;
    return 1;
}

static int is_sorted_str(const char **arr, size_t n) {
    for (size_t i = 1; i < n; i++)
        if (strcmp(arr[i-1], arr[i]) > 0)
            return 0;
    return 1;
}

/* Verify every element of 'expected' appears the same number of times in 'actual'
   (i.e. qsort didn't invent or lose elements). O(n^2) but fine for small test data. */
static int same_multiset_int(const int *actual, const int *expected, size_t n) {
    int *used = (int *)malloc(n * sizeof(int));
    if (!used) return 0;
    memset(used, 0, n * sizeof(int));//just cause calloc isn't implemented
    for (size_t i = 0; i < n; i++) {
        int found = 0;
        for (size_t j = 0; j < n; j++) {
            if (!used[j] && actual[i] == expected[j]) {
                used[j] = 1;
                found = 1;
                break;
            }
        }
        if (!found) { free(used); return 0; }
    }
    free(used);
    return 1;
}

/* ---------- tests ---------- */

static void test_zero_length(void) {
    /* qsort on an empty array must be a no-op (base pointer may be NULL per C11 7.22.5) */
    int dummy = 42;
    qsort(&dummy, 0, sizeof(int), cmp_int);   /* non-NULL base, 0 elements */
    assert(dummy == 42);

    /* NULL base with 0 count is undefined behaviour in some readings, so we
       pass a valid pointer but zero count – that IS well-defined. */
}

static void test_single_element(void) {
    int arr[] = {7};
    qsort(arr, 1, sizeof(int), cmp_int);
    assert(arr[0] == 7);
}

static void test_two_elements(void) {
    int arr[] = {2, 1};
    qsort(arr, 2, sizeof(int), cmp_int);
    assert(arr[0] == 1);
    assert(arr[1] == 2);

    int arr2[] = {1, 2};   /* already sorted */
    qsort(arr2, 2, sizeof(int), cmp_int);
    assert(arr2[0] == 1);
    assert(arr2[1] == 2);
}

static void test_already_sorted(void) {
    int arr[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    const size_t n = sizeof arr / sizeof arr[0];
    const int expected[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    qsort(arr, n, sizeof(int), cmp_int);
    assert(memcmp(arr, expected, sizeof arr) == 0);
}

static void test_reverse_sorted(void) {
    int arr[] = {9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
    const size_t n = sizeof arr / sizeof arr[0];
    const int original[] = {9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
    qsort(arr, n, sizeof(int), cmp_int);
    assert(is_sorted_int(arr, n, cmp_int));
    assert(same_multiset_int(arr, original, n));
}

static void test_all_equal(void) {
    int arr[] = {5, 5, 5, 5, 5};
    const size_t n = sizeof arr / sizeof arr[0];
    qsort(arr, n, sizeof(int), cmp_int);
    for (size_t i = 0; i < n; i++)
        assert(arr[i] == 5);
}

static void test_duplicates(void) {
    int arr[] = {3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5};
    const size_t n = sizeof arr / sizeof arr[0];
    const int original[] = {3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 5};
    qsort(arr, n, sizeof(int), cmp_int);
    assert(is_sorted_int(arr, n, cmp_int));
    assert(same_multiset_int(arr, original, n));
}

static void test_negative_numbers(void) {
    int arr[] = {0, -1, -100, 42, -3, 7, INT_MIN, INT_MAX, -2};
    const size_t n = sizeof arr / sizeof arr[0];
    const int original[] = {0, -1, -100, 42, -3, 7, INT_MIN, INT_MAX, -2};
    qsort(arr, n, sizeof(int), cmp_int);
    assert(is_sorted_int(arr, n, cmp_int));
    assert(same_multiset_int(arr, original, n));
    assert(arr[0] == INT_MIN);
    assert(arr[n-1] == INT_MAX);
}

static void test_descending_comparator(void) {
    int arr[] = {3, 1, 4, 1, 5, 9, 2, 6};
    const size_t n = sizeof arr / sizeof arr[0];
    qsort(arr, n, sizeof(int), cmp_int_desc);
    assert(is_sorted_int(arr, n, cmp_int_desc));
    assert(arr[0] >= arr[n-1]);   /* largest first */
}

static void test_doubles(void) {
    double arr[] = {3.14, -2.71, 0.0, 1.41, -0.0, 100.0, 0.001};
    const size_t n = sizeof arr / sizeof arr[0];
    qsort(arr, n, sizeof(double), cmp_double);
    assert(is_sorted_double(arr, n));
}

static void test_chars(void) {
    unsigned char arr[] = {'z', 'a', 'm', 'A', '0', '\x01', '\xff'};
    const size_t n = sizeof arr / sizeof arr[0];
    qsort(arr, n, sizeof(unsigned char), cmp_char);
    for (size_t i = 1; i < n; i++)
        assert(arr[i-1] <= arr[i]);
}

static void test_strings(void) {
    const char *arr[] = {"banana", "apple", "cherry", "date", "apple", "fig"};
    const size_t n = sizeof arr / sizeof arr[0];
    qsort(arr, n, sizeof(char *), cmp_str);
    assert(is_sorted_str(arr, n));
    assert(strcmp(arr[0], "apple") == 0);
    assert(strcmp(arr[n-1], "fig")  == 0);
}

static void test_large_array(void) {
#define LARGE_N 1000
    int *arr = (int *)malloc(LARGE_N * sizeof(int));
    int *original = (int *)malloc(LARGE_N * sizeof(int));
    assert(arr != NULL);
    assert(original != NULL);

    /* Pseudo-random fill via a simple LCG so it's reproducible */
    unsigned int state = 12345;
    for (int i = 0; i < LARGE_N; i++) {
        state = state * 1664525u + 1013904223u;
        arr[i] = original[i] = (int)(state & 0x7FFFFFFF);
    }

    qsort(arr, LARGE_N, sizeof(int), cmp_int);
    assert(is_sorted_int(arr, LARGE_N, cmp_int));
    assert(same_multiset_int(arr, original, LARGE_N));

    free(arr);
    free(original);
#undef LARGE_N
}

/* A struct to confirm qsort works on non-primitive element sizes */
typedef struct { int key; char tag; } Record;

int cmp_record(const void *a, const void *b) {
    return cmp_int(&((const Record *)a)->key, &((const Record *)b)->key);
}

static void test_struct_elements(void) {
    Record arr[] = {{5,'e'},{3,'c'},{1,'a'},{4,'d'},{2,'b'}};
    const size_t n = sizeof arr / sizeof arr[0];
    qsort(arr, n, sizeof(Record), cmp_record);
    for (size_t i = 0; i < n; i++) {
        assert(arr[i].key == (int)(i + 1));
        assert(arr[i].tag == (char)('a' + i));  /* struct fields moved together */
    }
}

/* ---------- entry point ---------- */

void test_qsort(void) {
    test_zero_length();
    test_single_element();
    test_two_elements();
    test_already_sorted();
    test_reverse_sorted();
    test_all_equal();
    test_duplicates();
    test_negative_numbers();
    test_descending_comparator();
    test_doubles();
    test_chars();
    test_strings();
    test_large_array();
    test_struct_elements();
    printf("All qsort tests passed.\n");
}

void stdlib_test() {
    test_qsort();
}