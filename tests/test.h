#ifndef YVEX_TEST_H
#define YVEX_TEST_H

#include <stdio.h>
#include <string.h>

#define YVEX_TEST_ASSERT(expr) \
    do { \
        if (!(expr)) { \
            fprintf(stderr, "%s:%d: assertion failed: %s\n", __FILE__, __LINE__, #expr); \
            return 1; \
        } \
    } while (0)

#define YVEX_TEST_STREQ(actual, expected) \
    do { \
        const char *yvex_test_actual = (actual); \
        const char *yvex_test_expected = (expected); \
        if (!yvex_test_actual || !yvex_test_expected || strcmp(yvex_test_actual, yvex_test_expected) != 0) { \
            fprintf(stderr, "%s:%d: expected '%s', got '%s'\n", \
                    __FILE__, __LINE__, \
                    yvex_test_expected ? yvex_test_expected : "(null)", \
                    yvex_test_actual ? yvex_test_actual : "(null)"); \
            return 1; \
        } \
    } while (0)

#endif
