/*
 * YVEX - Version tests
 *
 * File: tests/test_version.c
 * Layer: test
 *
 * Purpose:
 *   Proves that the core version API returns stable compile-time values
 *   without runtime setup.
 *
 * Covers:
 *   - yvex_version_string
 *   - yvex_version_major
 *   - yvex_version_minor
 *   - yvex_version_patch
 *   - yvex_operator_contract_report_build
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_version
 *
 * Expected:
 *   - exits 0 on success
 *   - prints concise failure to stderr
 */
#include <yvex/version.h>

#include "test.h"

int yvex_test_version(void)
{
    yvex_operator_contract_report report;

    YVEX_TEST_ASSERT_STREQ(yvex_version_string(), "0.1.0", "version string");
    YVEX_TEST_ASSERT(yvex_version_major() == 0, "version major");
    YVEX_TEST_ASSERT(yvex_version_minor() == 1, "version minor");
    YVEX_TEST_ASSERT(yvex_version_patch() == 0, "version patch");
    YVEX_TEST_ASSERT(
        yvex_operator_contract_report_build(NULL) == YVEX_ERR_INVALID_ARG,
        "operator contract null report");
    YVEX_TEST_ASSERT(
        yvex_operator_contract_report_build(&report) == YVEX_OK,
        "operator contract report");
    YVEX_TEST_ASSERT_STREQ(report.schema_version, "1",
                           "operator contract schema");
    YVEX_TEST_ASSERT_STREQ(report.protocol_version, "1",
                           "operator contract protocol");
    YVEX_TEST_ASSERT_STREQ(report.yvex_version, "0.1.0",
                           "operator contract version");
    YVEX_TEST_ASSERT_STREQ(report.product, "yvex",
                           "operator contract product");
    return 0;
}
