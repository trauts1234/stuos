#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

/* ========================================================================== */
/* Lightweight Test Harness                                                   */
/* ========================================================================== */

static int tests_run = 0;
static int tests_passed = 0;

#define TEST_ASSERT(condition, msg) do { \
    if (!(condition)) { \
        printf("  [FAIL] %s:%d: %s\n", __FILE__, __LINE__, msg); \
        return 0; \
    } \
} while(0)

#define RUN_TEST(test_func) do { \
    tests_run++; \
    if (test_func()) { \
        tests_passed++; \
    } else { \
        printf("[FAIL]\n"); \
    } \
} while(0)

static void reset_state(mbstate_t *ps) {
    memset(ps, 0, sizeof(mbstate_t));
}

/* ========================================================================== */
/* Test Cases                                                                 */
/* ========================================================================== */

/* 1. Edge Cases: Null pointers and Zero sizes */

static int test_null_s_pointer(void) {
    mbstate_t ps;
    reset_state(&ps);
    // Passing NULL for 's' resets the shift state and returns 0
    size_t res = mbrtowc(NULL, NULL, 0, &ps);
    TEST_ASSERT(res == 0, "mbrtowc(NULL, NULL, 0) should return 0");
    return 1;
}

static int test_null_pwc_pointer(void) {
    mbstate_t ps;
    reset_state(&ps);
    // Passing NULL for 'pwc' is valid; it checks validity but doesn't store the result
    size_t res = mbrtowc(NULL, "A", 1, &ps);
    TEST_ASSERT(res == 1, "mbrtowc with NULL pwc should return length without storing");
    return 1;
}

static int test_zero_n_size(void) {
    wchar_t wc;
    mbstate_t ps;
    reset_state(&ps);
    // If n=0 and s is not null, it cannot complete the character
    size_t res = mbrtowc(&wc, "A", 0, &ps);
    TEST_ASSERT(res == (size_t)-2, "mbrtowc with n=0 should return -2 (incomplete)");
    return 1;
}

static int test_null_ps_pointer(void) {
    wchar_t wc;
    // Reset internal state
    mbrtowc(NULL, NULL, 0, NULL);
    // Passing NULL for 'ps' uses the internal static mbstate_t
    size_t res = mbrtowc(&wc, "A", 1, NULL);
    TEST_ASSERT(res == 1 && wc == L'A', "mbrtowc with NULL ps should use internal state");
    return 1;
}

/* 2. Null Sentinel and ASCII */

static int test_null_sentinel(void) {
    wchar_t wc = L'X'; // Initialize to non-zero to ensure it gets overwritten
    mbstate_t ps;
    reset_state(&ps);
    
    size_t res = mbrtowc(&wc, "\0", 1, &ps);
    TEST_ASSERT(res == 0, "mbrtowc on null terminator should return 0");
    TEST_ASSERT(wc == L'\0', "Wide char should be set to L'\\0'");
    return 1;
}

static int test_ascii_characters(void) {
    wchar_t wc;
    mbstate_t ps;
    reset_state(&ps);
    
    size_t res = mbrtowc(&wc, "Z", 1, &ps);
    TEST_ASSERT(res == 1, "ASCII should consume 1 byte");
    TEST_ASSERT(wc == L'Z', "ASCII wide char value mismatch");
    return 1;
}

/* 3. Unicode: 2, 3, and 4-byte UTF-8 Sequences */

static int test_2byte_utf8(void) {
    wchar_t wc;
    mbstate_t ps;
    reset_state(&ps);
    
    // 'é' (U+00E9) -> 0xC3 0xA9
    char s[] = "\xC3\xA9";
    size_t res = mbrtowc(&wc, s, 2, &ps);
    
    TEST_ASSERT(res == 2, "2-byte UTF-8 should consume 2 bytes");
    TEST_ASSERT(wc == 0x00E9, "2-byte UTF-8 wide char value mismatch");
    return 1;
}

static int test_3byte_utf8(void) {
    wchar_t wc;
    mbstate_t ps;
    reset_state(&ps);
    
    // '€' (U+20AC) -> 0xE2 0x82 0xAC
    char s[] = "\xE2\x82\xAC";
    size_t res = mbrtowc(&wc, s, 3, &ps);
    
    TEST_ASSERT(res == 3, "3-byte UTF-8 should consume 3 bytes");
    TEST_ASSERT(wc == 0x20AC, "3-byte UTF-8 wide char value mismatch");
    return 1;
}

static int test_4byte_utf8(void) {
    wchar_t wc;
    mbstate_t ps;
    reset_state(&ps);
    
    // '🚀' (U+1F680) -> 0xF0 0x9F 0x9A 0x80
    // Note: Requires 32-bit wchar_t (standard on x64 Linux/macOS)
    char s[] = "\xF0\x9F\x9A\x80";
    size_t res = mbrtowc(&wc, s, 4, &ps);
    
    TEST_ASSERT(res == 4, "4-byte UTF-8 should consume 4 bytes");
    TEST_ASSERT(wc == 0x1F680, "4-byte UTF-8 wide char value mismatch");
    return 1;
}

/* 4. Incremental Parsing (Calling multiple times for one character) */

static int test_incremental_parsing_4_times(void) {
    wchar_t wc;
    mbstate_t ps;
    reset_state(&ps);
    
    // '🚀' (U+1F680) fed 1 byte at a time
    char s[] = "\xF0\x9F\x9A\x80";
    
    TEST_ASSERT(mbrtowc(&wc, s, 1, &ps) == (size_t)-2, "Byte 1 of 4 should return -2");
    TEST_ASSERT(mbrtowc(&wc, s+1, 1, &ps) == (size_t)-2, "Byte 2 of 4 should return -2");
    TEST_ASSERT(mbrtowc(&wc, s+2, 1, &ps) == (size_t)-2, "Byte 3 of 4 should return -2");
    
    // The standard dictates it returns the *total* bytes of the character, not just the remaining
    size_t res = mbrtowc(&wc, s+3, 1, &ps);
    TEST_ASSERT(res == 1, "Final byte of 4 should return 1");
    TEST_ASSERT(wc == 0x1F680, "Incremental 4-byte wide char value mismatch");
    return 1;
}

static int test_incremental_parsing_2_times(void) {
    wchar_t wc;
    mbstate_t ps;
    reset_state(&ps);
    
    // '⼩' (U+2F29) fed 2 bytes, then 1 byte
    char s[] = "\xE2\xBC\xA9";
    size_t val = mbrtowc(&wc, s, 2, &ps);
    TEST_ASSERT(val == (size_t)-2, "First 2 bytes of 3 should return -2");
    size_t res = mbrtowc(&wc, s+2, 1, &ps);
    TEST_ASSERT(res == 1, "Final byte of 3 should return 1");
    TEST_ASSERT(wc == 0x2F29, "Incremental 3-byte wide char value mismatch");
    return 1;
}

/* 5. Error Cases (Invalid UTF-8) */

static int test_invalid_start_byte(void) {
    wchar_t wc;
    mbstate_t ps;
    reset_state(&ps);
    
    // 0x80 is a continuation byte, invalid as a start byte
    char s[] = "\x80";
    errno = 0;
    size_t res = mbrtowc(&wc, s, 1, &ps);
    
    TEST_ASSERT(res == (size_t)-1, "Invalid start byte should return -1");
    TEST_ASSERT(errno == EILSEQ, "errno should be set to EILSEQ");
    return 1;
}

static int test_invalid_continuation_byte(void) {
    wchar_t wc;
    mbstate_t ps;
    reset_state(&ps);
    
    // 0xC3 expects a continuation byte (0x80-0xBF), but gets a space (0x20)
    char s[] = "\xC3\x20";
    errno = 0;
    size_t res = mbrtowc(&wc, s, 2, &ps);
    
    TEST_ASSERT(res == (size_t)-1, "Invalid continuation byte should return -1");
    TEST_ASSERT(errno == EILSEQ, "errno should be set to EILSEQ");
    return 1;
}

static int test_overlong_encoding(void) {
    wchar_t wc;
    mbstate_t ps;
    reset_state(&ps);
    
    // Overlong encoding of '/' (U+002F) using 2 bytes: 0xC0 0xAF
    char s[] = "\xC0\xAF";
    errno = 0;
    size_t res = mbrtowc(&wc, s, 2, &ps);
    
    TEST_ASSERT(res == (size_t)-1, "Overlong encoding should return -1");
    TEST_ASSERT(errno == EILSEQ, "errno should be set to EILSEQ");
    return 1;
}

// static int test_out_of_range_unicode(void) {
//     wchar_t wc;
//     mbstate_t ps;
//     reset_state(&ps);
    
//     // U+110000 (One past the maximum Unicode codepoint U+10FFFF)
//     char s[] = "\xFF\x90\x80\x80";
//     errno = 0;
//     size_t res = mbrtowc(&wc, s, 4, &ps);
    
//     TEST_ASSERT(res == (size_t)-1, "Out of range codepoint should return -1");
//     TEST_ASSERT(errno == EILSEQ, "errno should be set to EILSEQ");
//     return 1;
// }

static int test_truncated_sequence_eof(void) {
    wchar_t wc;
    mbstate_t ps;
    reset_state(&ps);
    
    // 4-byte sequence, but we only provide the first 2 bytes (simulating EOF/truncation)
    char s[] = "\xF0\x9F";
    size_t res = mbrtowc(&wc, s, 2, &ps);
    
    // It's a valid start, but incomplete. Should return -2, NOT -1 (EILSEQ)
    TEST_ASSERT(res == (size_t)-2, "Truncated valid sequence should return -2 (incomplete)");
    return 1;
}

void mbrtowc_test() {
    // Edge Cases
    RUN_TEST(test_null_s_pointer);
    RUN_TEST(test_null_pwc_pointer);
    RUN_TEST(test_zero_n_size);
    RUN_TEST(test_null_ps_pointer);

    // Null Sentinel & ASCII
    RUN_TEST(test_null_sentinel);
    RUN_TEST(test_ascii_characters);

    // Unicode (Full sequences)
    RUN_TEST(test_2byte_utf8);
    RUN_TEST(test_3byte_utf8);
    RUN_TEST(test_4byte_utf8);

    // Incremental Parsing
    RUN_TEST(test_incremental_parsing_2_times);
    RUN_TEST(test_incremental_parsing_4_times);

    // Error Cases
    RUN_TEST(test_invalid_start_byte);
    RUN_TEST(test_invalid_continuation_byte);
    RUN_TEST(test_overlong_encoding);
    // RUN_TEST(test_out_of_range_unicode);
    RUN_TEST(test_truncated_sequence_eof);

    if(tests_passed != tests_run) exit(EXIT_FAILURE);
}