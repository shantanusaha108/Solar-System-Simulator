#ifndef TEST_UTIL_H
#define TEST_UTIL_H

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static int g_testFailures = 0;

#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        if (cond) { printf("  ok   : %s\n", (msg)); }                          \
        else { printf("  FAIL : %s (%s:%d)\n", (msg), __FILE__, __LINE__);     \
               g_testFailures++; }                                             \
    } while (0)

#define CHECK_NEAR(a, b, tol, msg)                                             \
    do {                                                                       \
        double _a = (a), _b = (b);                                             \
        if (fabs(_a - _b) <= (tol)) { printf("  ok   : %s\n", (msg)); }        \
        else { printf("  FAIL : %s (got %.12g, want %.12g, tol %.3g) %s:%d\n", \
                      (msg), _a, _b, (double)(tol), __FILE__, __LINE__);       \
               g_testFailures++; }                                             \
    } while (0)

#define TEST_REPORT(name)                                                      \
    do {                                                                       \
        if (g_testFailures == 0) { printf("%s: PASSED\n", (name)); return 0; } \
        printf("%s: FAILED (%d)\n", (name), g_testFailures);                   \
        return 1;                                                              \
    } while (0)

#endif /* TEST_UTIL_H */
