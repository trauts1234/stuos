#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <errno.h>
#include <stdint.h>

// ============================================================================
// Lightweight Test Framework
// ============================================================================

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            printf("  [FAIL] %s:%d: %s\n", __FILE__, __LINE__, msg); \
            abort(); \
        } \
    } while (0)

#define TEST_ASSERT_EQ(a, b, msg) \
    do { \
        if ((a) != (b)) { \
            printf("  [FAIL] %s:%d: %s (Expected %lld, got %lld)\n", \
                   __FILE__, __LINE__, msg, (long long)(b), (long long)(a)); \
            abort(); \
        } \
    } while (0)

#define TEST_ASSERT_WCHAR_EQ(a, b, msg) \
    do { \
        if ((a) != (b)) { \
            printf("  [FAIL] %s:%d: %s (Expected U+%04X, got U+%04X)\n", \
                   __FILE__, __LINE__, msg, (unsigned int)(b), (unsigned int)(a)); \
            abort(); \
        } \
    } while (0)


#define RUN_TEST(test_func) \
    do { \
        test_func(); \
    } while (0)

// ============================================================================
// Test Cases
// ============================================================================

// 1. Edge Cases: Null and Zero
static void test_dst_null() {
    const char *src = "hello";
    const char *src_ptr = src;
    
    // When dst is NULL, it should return the required length without modifying src
    size_t ret = mbsrtowcs(NULL, &src_ptr, 10, NULL);
    TEST_ASSERT_EQ(ret, 5, "Return value should be length of string");
    TEST_ASSERT_EQ((uintptr_t)src_ptr, (uintptr_t)src, "src should not be updated when dst is NULL");
}

static void test_len_zero() {
    wchar_t dst[10];
    const char *src = "hello";
    const char *src_ptr = src;
    
    // When len is 0, it should return 0 and not advance src
    size_t ret = mbsrtowcs(dst, &src_ptr, 0, NULL);
    TEST_ASSERT_EQ(ret, 0, "Return value should be 0 when len is 0");
    TEST_ASSERT_EQ((uintptr_t)src_ptr, (uintptr_t)src, "src should not advance when len is 0");
}

static void test_ps_null() {
    wchar_t dst[10];
    const char *src = "test";
    const char *src_ptr = src;
    
    // When ps is NULL, it should use the internal shift state (works for stateless UTF-8)
    size_t ret = mbsrtowcs(dst, &src_ptr, 10, NULL);
    TEST_ASSERT_EQ(ret, 4, "Should succeed with NULL ps");
}

static void test_empty_string() {
    wchar_t dst[10] = {0xFFFF}; // Fill with garbage to ensure it gets overwritten
    const char *src = "";
    const char *src_ptr = src;
    
    size_t ret = mbsrtowcs(dst, &src_ptr, 10, NULL);
    TEST_ASSERT_EQ(ret, 0, "Return value should be 0 for empty string");
    TEST_ASSERT_EQ(dst[0], L'\0', "Should write null terminator");
    TEST_ASSERT(src_ptr == NULL, "src should be set to NULL when string is null-terminated");
}

// 2. Invalid Unicode (UTF-8)
static void test_invalid_byte() {
    wchar_t dst[10];
    const char *src = "a\xFF" "b"; // 'a', invalid byte, 'b'
    const char *src_ptr = src;
    
    errno = 0;
    size_t ret = mbsrtowcs(dst, &src_ptr, 10, NULL);
    TEST_ASSERT_EQ(ret, (size_t)-1, "Should return -1 on invalid byte");
    TEST_ASSERT_EQ(errno, EILSEQ, "errno should be EILSEQ");
    TEST_ASSERT_EQ((uintptr_t)src_ptr, (uintptr_t)(src + 1), "src should point to the invalid byte");
    TEST_ASSERT_WCHAR_EQ(dst[0], L'a', "Should have converted the valid 'a' before failing");
}

static void test_overlong_encoding() {
    wchar_t dst[10];
    const char *src = "\xC0\x80"; // Overlong encoding of NULL (invalid in UTF-8)
    const char *src_ptr = src;
    
    errno = 0;
    size_t ret = mbsrtowcs(dst, &src_ptr, 10, NULL);
    TEST_ASSERT_EQ(ret, (size_t)-1, "Should return -1 on overlong encoding");
    TEST_ASSERT_EQ(errno, EILSEQ, "errno should be EILSEQ");
}

static void test_truncated_sequence() {
    wchar_t dst[10];
    const char *src = "\xE2\x82"; // Truncated 3-byte sequence (missing 3rd byte)
    const char *src_ptr = src;
    
    errno = 0;
    size_t ret = mbsrtowcs(dst, &src_ptr, 10, NULL);
    TEST_ASSERT_EQ(ret, (size_t)-1, "Should return -1 on truncated sequence");
    TEST_ASSERT_EQ(errno, EILSEQ, "errno should be EILSEQ");
}

// 3. Length is exactly the length of the string
static void test_len_exact() {
    wchar_t dst[3];
    const char *src = "abc";
    const char *src_ptr = src;
    
    size_t ret = mbsrtowcs(dst, &src_ptr, 3, NULL);
    TEST_ASSERT_EQ(ret, 3, "Should return 3");
    TEST_ASSERT_WCHAR_EQ(dst[0], L'a', "dst[0] mismatch");
    TEST_ASSERT_WCHAR_EQ(dst[1], L'b', "dst[1] mismatch");
    TEST_ASSERT_WCHAR_EQ(dst[2], L'c', "dst[2] mismatch");
    // Note: It does NOT write a null terminator if it runs out of len
    TEST_ASSERT_EQ((uintptr_t)src_ptr, (uintptr_t)(src + 3), "src should point to the source null terminator");
}

// 4. Buffers too short / too long
static void test_buffer_too_short() {
    wchar_t dst[2];
    const char *src = "abc";
    const char *src_ptr = src;
    
    size_t ret = mbsrtowcs(dst, &src_ptr, 2, NULL);
    TEST_ASSERT_EQ(ret, 2, "Should return 2 (len)");
    TEST_ASSERT_WCHAR_EQ(dst[0], L'a', "dst[0] mismatch");
    TEST_ASSERT_WCHAR_EQ(dst[1], L'b', "dst[1] mismatch");
    // src should point to the next unconverted character ("c")
    TEST_ASSERT_EQ((uintptr_t)src_ptr, (uintptr_t)(src + 2), "src should point to 'c'");
}

static void test_buffer_too_long() {
    wchar_t dst[10];
    const char *src = "abc";
    const char *src_ptr = src;
    
    size_t ret = mbsrtowcs(dst, &src_ptr, 10, NULL);
    TEST_ASSERT_EQ(ret, 3, "Should return 3 (actual length)");
    TEST_ASSERT_WCHAR_EQ(dst[0], L'a', "dst[0] mismatch");
    TEST_ASSERT_WCHAR_EQ(dst[1], L'b', "dst[1] mismatch");
    TEST_ASSERT_WCHAR_EQ(dst[2], L'c', "dst[2] mismatch");
    TEST_ASSERT_WCHAR_EQ(dst[3], L'\0', "Should write null terminator");
    // src should be set to NULL because the source string was null-terminated
    TEST_ASSERT(src_ptr == NULL, "src should be NULL when source is fully consumed");
}

// 5. Unicode / Multibyte specific tests (x64 UTF-8)
static void test_multibyte_basic() {
    // "€" is U+20AC -> UTF-8: E2 82 AC (3 bytes)
    wchar_t dst[10];
    const char *src = "\xE2\x82\xAC"; 
    const char *src_ptr = src;
    
    size_t ret = mbsrtowcs(dst, &src_ptr, 10, NULL);
    TEST_ASSERT_EQ(ret, 1, "Should return 1 wide character");
    TEST_ASSERT_WCHAR_EQ(dst[0], 0x20AC, "Should be Euro sign");
    TEST_ASSERT(src_ptr == NULL, "src should be NULL");
}

static void test_multibyte_exact_len() {
    // "€" (3 bytes) + "a" (1 byte)
    wchar_t dst[1];
    const char *src = "\xE2\x82\xAC" "a";
    const char *src_ptr = src;
    
    size_t ret = mbsrtowcs(dst, &src_ptr, 1, NULL);
    TEST_ASSERT_EQ(ret, 1, "Should return 1");
    TEST_ASSERT_WCHAR_EQ(dst[0], 0x20AC, "Should be Euro sign");
    // src should point past the 3-byte Euro sign, to the "a"
    TEST_ASSERT_EQ((uintptr_t)src_ptr, (uintptr_t)(src + 3), "src should point to 'a'");
}

static void test_multibyte_too_short() {
    // "€" (3 bytes) + "a" (1 byte)
    // We only have room for 1 wide char, but the first char takes 3 bytes.
    // It should successfully convert the first char because len=1 allows 1 wide char.
    wchar_t dst[1];
    const char *src = "\xE2\x82\xAC" "a";
    const char *src_ptr = src;
    
    size_t ret = mbsrtowcs(dst, &src_ptr, 1, NULL);
    TEST_ASSERT_EQ(ret, 1, "Should return 1");
    TEST_ASSERT_WCHAR_EQ(dst[0], 0x20AC, "Should be Euro sign");
    TEST_ASSERT_EQ((uintptr_t)src_ptr, (uintptr_t)(src + 3), "src should point to 'a'");
}

static void test_shift_state_handling() {
    mbstate_t ps;
    memset(&ps, 0, sizeof(ps));
    wchar_t dst[10];
    const char *src = "hello";
    const char *src_ptr = src;
    
    size_t ret = mbsrtowcs(dst, &src_ptr, 10, &ps);
    TEST_ASSERT_EQ(ret, 5, "Should succeed with explicit mbstate_t");
}

// ============================================================================
// Main Execution
// ============================================================================

void mbsrtowcs_test() {

    // Edge cases
    RUN_TEST(test_dst_null);
    RUN_TEST(test_len_zero);
    RUN_TEST(test_ps_null);
    RUN_TEST(test_empty_string);

    // Invalid Unicode
    RUN_TEST(test_invalid_byte);
    RUN_TEST(test_overlong_encoding);
    RUN_TEST(test_truncated_sequence);

    // Length boundaries
    RUN_TEST(test_len_exact);
    RUN_TEST(test_buffer_too_short);
    RUN_TEST(test_buffer_too_long);

    // Unicode / Multibyte specifics
    RUN_TEST(test_multibyte_basic);
    RUN_TEST(test_multibyte_exact_len);
    RUN_TEST(test_multibyte_too_short);
    RUN_TEST(test_shift_state_handling);
}