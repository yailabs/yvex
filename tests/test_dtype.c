/*
 * YVEX - dtype tests
 *
 * File: tests/test_dtype.c
 * Layer: test
 *
 * Purpose:
 *   Proves model layer dtype/qtype name mapping, GGML raw type mapping, and storage
 *   byte formulas for the storage-accounting subset.
 *
 * Covers:
 *   - yvex_dtype_get_info
 *   - yvex_dtype_from_ggml_type
 *   - yvex_dtype_name
 *   - yvex_dtype_storage_bytes
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_dtype
 *
 * Expected:
 *   - exits 0 on success
 *   - prints concise failure to stderr
 */
#include <limits.h>

#include <yvex/yvex.h>

#include "test.h"

static int test_names_and_mapping(void)
{
    const yvex_dtype_info *info;

    YVEX_TEST_ASSERT_STREQ(yvex_dtype_name(YVEX_DTYPE_F32), "F32", "F32 name");
    YVEX_TEST_ASSERT_STREQ(yvex_dtype_name(YVEX_DTYPE_Q4_0), "Q4_0", "Q4_0 name");
    YVEX_TEST_ASSERT_STREQ(yvex_dtype_name((yvex_dtype)999), "UNKNOWN", "unknown dtype name");

    info = yvex_dtype_get_info(YVEX_DTYPE_F32);
    YVEX_TEST_ASSERT(info != NULL, "F32 info exists");
    YVEX_TEST_ASSERT(info->ggml_type == 0, "F32 raw type");
    YVEX_TEST_ASSERT(info->scalar_bytes == 4, "F32 scalar bytes");
    YVEX_TEST_ASSERT(info->is_supported_for_storage_accounting == 1, "F32 accounting supported");

    info = yvex_dtype_from_ggml_type(8);
    YVEX_TEST_ASSERT(info != NULL, "Q8_0 info exists");
    YVEX_TEST_ASSERT(info->dtype == YVEX_DTYPE_Q8_0, "Q8_0 raw type mapping");

    info = yvex_dtype_from_ggml_type(9999);
    YVEX_TEST_ASSERT(info != NULL, "unknown raw type info exists");
    YVEX_TEST_ASSERT(info->dtype == YVEX_DTYPE_UNKNOWN, "unknown raw type maps unknown");

    return 0;
}

static int expect_bytes(yvex_dtype dtype, unsigned long long elements, unsigned long long expected)
{
    yvex_error err;
    unsigned long long actual = 0;
    int rc;

    rc = yvex_dtype_storage_bytes(dtype, elements, &actual, &err);
    if (rc != YVEX_OK) {
        fprintf(stderr, "storage failed for %s: %s: %s\n",
                yvex_dtype_name(dtype), yvex_error_where(&err), yvex_error_message(&err));
        return 1;
    }
    if (actual != expected) {
        fprintf(stderr, "storage mismatch for %s: expected %llu got %llu\n",
                yvex_dtype_name(dtype), expected, actual);
        return 1;
    }
    return 0;
}

static int test_storage_formulas(void)
{
    YVEX_TEST_ASSERT(expect_bytes(YVEX_DTYPE_F32, 32, 128) == 0, "F32 bytes");
    YVEX_TEST_ASSERT(expect_bytes(YVEX_DTYPE_F16, 32, 64) == 0, "F16 bytes");
    YVEX_TEST_ASSERT(expect_bytes(YVEX_DTYPE_BF16, 32, 64) == 0, "BF16 bytes");
    YVEX_TEST_ASSERT(expect_bytes(YVEX_DTYPE_F64, 4, 32) == 0, "F64 bytes");
    YVEX_TEST_ASSERT(expect_bytes(YVEX_DTYPE_I8, 9, 9) == 0, "I8 bytes");
    YVEX_TEST_ASSERT(expect_bytes(YVEX_DTYPE_I16, 9, 18) == 0, "I16 bytes");
    YVEX_TEST_ASSERT(expect_bytes(YVEX_DTYPE_I32, 9, 36) == 0, "I32 bytes");
    YVEX_TEST_ASSERT(expect_bytes(YVEX_DTYPE_I64, 9, 72) == 0, "I64 bytes");
    YVEX_TEST_ASSERT(expect_bytes(YVEX_DTYPE_Q4_0, 32, 18) == 0, "Q4_0 one block");
    YVEX_TEST_ASSERT(expect_bytes(YVEX_DTYPE_Q4_0, 33, 36) == 0, "Q4_0 ceil blocks");
    YVEX_TEST_ASSERT(expect_bytes(YVEX_DTYPE_Q8_0, 32, 34) == 0, "Q8_0 one block");
    YVEX_TEST_ASSERT(expect_bytes(YVEX_DTYPE_Q8_0, 33, 68) == 0, "Q8_0 ceil blocks");
    return 0;
}

static int test_error_cases(void)
{
    yvex_error err;
    unsigned long long bytes = 123;
    int rc;

    rc = yvex_dtype_storage_bytes(YVEX_DTYPE_Q4_K, 32, &bytes, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_UNSUPPORTED, "unsupported formula rejected");
    YVEX_TEST_ASSERT(bytes == 0, "unsupported clears output bytes");

    rc = yvex_dtype_storage_bytes(YVEX_DTYPE_F32, (ULLONG_MAX / 4u) + 1u, &bytes, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_BOUNDS, "dense byte overflow rejected");

    rc = yvex_dtype_storage_bytes(YVEX_DTYPE_F32, 1, NULL, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_INVALID_ARG, "null output rejected");

    return 0;
}

int main(void)
{
    if (test_names_and_mapping() != 0) {
        return 1;
    }
    if (test_storage_formulas() != 0) {
        return 1;
    }
    if (test_error_cases() != 0) {
        return 1;
    }
    return 0;
}
