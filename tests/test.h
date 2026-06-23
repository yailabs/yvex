/*
 * YVEX - Test helpers
 *
 * File: tests/test.h
 * Layer: test
 *
 * Purpose:
 *   Provides tiny assertion helpers for core C tests. This is intentionally
 *   small and dependency-free.
 *
 * Covers:
 *   - failure reporting to stderr
 *   - boolean assertions
 *   - string equality assertions
 *
 * Commands:
 *   - make test-core
 *
 * Expected:
 *   - tests exit 0 on success
 *   - tests print concise failure messages to stderr
 */
#ifndef YVEX_TEST_H
#define YVEX_TEST_H

#include <stdio.h>
#include <string.h>

#define YVEX_TEST_FAIL(msg) \
    do { \
        fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, (msg)); \
        return 1; \
    } while (0)

#define YVEX_TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            YVEX_TEST_FAIL(msg); \
        } \
    } while (0)

#define YVEX_TEST_ASSERT_STREQ(actual, expected, msg) \
    do { \
        const char *yvex_test_actual = (actual); \
        const char *yvex_test_expected = (expected); \
        if (!yvex_test_actual || !yvex_test_expected || strcmp(yvex_test_actual, yvex_test_expected) != 0) { \
            fprintf(stderr, "FAIL: %s:%d: %s: expected '%s', got '%s'\n", \
                    __FILE__, __LINE__, (msg), \
                    yvex_test_expected ? yvex_test_expected : "(null)", \
                    yvex_test_actual ? yvex_test_actual : "(null)"); \
            return 1; \
        } \
    } while (0)

#endif /* YVEX_TEST_H */
