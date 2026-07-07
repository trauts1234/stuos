#include "stdio.h"
#include "stdlib.h"
#include "errno.h"
#include "string.h"

void test(char* str)
{
    double d1, d2;
    char *e1, *e2;
    int x1, x2;

    errno=0;
    // printf("CASE: %s\n", str);
    e1 = NULL;
    d1 = strtod(str, &e1);
    x1 = errno;

    errno = 0;
    e2 = NULL;
    d2 = strtod(str, &e2);
    x2 = errno;

    if (d1 != d2 || e1 != e2 || x1 != x2) {
        printf("  ERR: %s, %s\n", str, strerror(errno));
        printf("    E1 %f, %g, %s, %d\n", d1, d1, e1 ? e1 : "", x1);
        printf("    E2 %f, %g, %s, %d\n", d2, d2, e2 ? e2 : "", x2);
        if (d1 != d2) puts("different value");
        if (e1 != e2) puts("different end position");
        if (x1 != x2) puts("different errno");
        abort();
    } else {
        // printf("  SUCCESS [%f][%s]\n", d1, e1 ? e1 : "");
    }
    // printf("\n");
}

void strtod_test()
{
    test(".1");
    test("  .");
    test("  1.2e3");
    test(" +1.2e3");
    test("1.2e3");
    test("+1.2e3");
    test("+1.e3");
    test("-1.2e3");
    test("-1.2e3.5");
    test("-1.2e");
    test("--1.2e3.5");
    test("--1-.2e3.5");
    test("-a");
    test("a");
    test(".1e");
    test(".1e3");
    test(".1e-3");
    test(".1e-");
    test(" .e-");
    test(" .e");
    test(" e");
    test(" e0");
    test(" ee");
    test(" -e");
    test(" .9");
    test(" ..9");
    test("009");
    test("0.09e02");
    /* http://thread.gmane.org/gmane.editors.vim.devel/19268/ */
    test("0.9999999999999999999999999999999999");
    test("2.2250738585072010e-308"); // BUG
    /* PHP (slashdot.jp): http://opensource.slashdot.jp/story/11/01/08/0527259/PHP%E3%81%AE%E6%B5%AE%E5%8B%95%E5%B0%8F%E6%95%B0%E7%82%B9%E5%87%A6%E7%90%86%E3%81%AB%E7%84%A1%E9%99%90%E3%83%AB%E3%83%BC%E3%83%97%E3%81%AE%E3%83%90%E3%82%B0 */
    test("2.2250738585072011e-308");
    /* Gauche: http://blog.practical-scheme.net/gauche/20110203-bitten-by-floating-point-numbers-again */
    test("2.2250738585072012e-308");
    test("2.2250738585072013e-308"); // Hmm.
    test("2.2250738585072014e-308"); // Hmm.
}