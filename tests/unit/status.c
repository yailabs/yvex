/*
 * YVEX - Status tests
 *
 * File: tests/test_status.c
 * Layer: test
 *
 * Purpose:
 *   Proves that every core status code maps to a stable string and that
 *   status predicates behave deterministically.
 *
 * Covers:
 *   - yvex_status_name
 *   - yvex_status_is_ok
 *   - yvex_status_is_error
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_status
 *
 * Expected:
 *   - exits 0 on success
 *   - prints concise failure to stderr
 */
#include <yvex/status.h>

#include "test.h"

int yvex_test_status(void)
{
    YVEX_TEST_ASSERT_STREQ(yvex_status_name(YVEX_OK), "YVEX_OK", "YVEX_OK name");
    YVEX_TEST_ASSERT_STREQ(yvex_status_name(YVEX_ERR), "YVEX_ERR", "YVEX_ERR name");
    YVEX_TEST_ASSERT_STREQ(yvex_status_name(YVEX_ERR_NOMEM), "YVEX_ERR_NOMEM", "YVEX_ERR_NOMEM name");
    YVEX_TEST_ASSERT_STREQ(yvex_status_name(YVEX_ERR_IO), "YVEX_ERR_IO", "YVEX_ERR_IO name");
    YVEX_TEST_ASSERT_STREQ(yvex_status_name(YVEX_ERR_FORMAT), "YVEX_ERR_FORMAT", "YVEX_ERR_FORMAT name");
    YVEX_TEST_ASSERT_STREQ(yvex_status_name(YVEX_ERR_UNSUPPORTED), "YVEX_ERR_UNSUPPORTED", "YVEX_ERR_UNSUPPORTED name");
    YVEX_TEST_ASSERT_STREQ(yvex_status_name(YVEX_ERR_BACKEND), "YVEX_ERR_BACKEND", "YVEX_ERR_BACKEND name");
    YVEX_TEST_ASSERT_STREQ(yvex_status_name(YVEX_ERR_BOUNDS), "YVEX_ERR_BOUNDS", "YVEX_ERR_BOUNDS name");
    YVEX_TEST_ASSERT_STREQ(yvex_status_name(YVEX_ERR_STATE), "YVEX_ERR_STATE", "YVEX_ERR_STATE name");
    YVEX_TEST_ASSERT_STREQ(yvex_status_name(YVEX_ERR_CANCELLED), "YVEX_ERR_CANCELLED", "YVEX_ERR_CANCELLED name");
    YVEX_TEST_ASSERT_STREQ(yvex_status_name(YVEX_ERR_INVALID_ARG), "YVEX_ERR_INVALID_ARG", "YVEX_ERR_INVALID_ARG name");
    YVEX_TEST_ASSERT_STREQ(yvex_status_name((yvex_status)1234), "YVEX_STATUS_UNKNOWN", "unknown status name");

    YVEX_TEST_ASSERT(yvex_status_is_ok(YVEX_OK), "YVEX_OK is ok");
    YVEX_TEST_ASSERT(!yvex_status_is_ok(YVEX_ERR), "YVEX_ERR is not ok");
    YVEX_TEST_ASSERT(!yvex_status_is_ok((yvex_status)1234), "unknown positive status is not ok");
    YVEX_TEST_ASSERT(!yvex_status_is_error(YVEX_OK), "YVEX_OK is not error");
    YVEX_TEST_ASSERT(yvex_status_is_error(YVEX_ERR_INVALID_ARG), "YVEX_ERR_INVALID_ARG is error");
    YVEX_TEST_ASSERT(yvex_status_is_error((yvex_status)1234), "unknown positive status is error");
    return 0;
}
