/*
 * YVEX - GGUF qtype ABI tests
 *
 * File: tests/unit/gguf_qtype_abi.c
 * Layer: test
 *
 * Purpose:
 *   Proves GGUF qtype byte geometry, storage byte calculation, and explicit
 *   refusal behavior without backend arithmetic, writer, materialization, or
 *   runtime claims.
 *
 * Covers:
 *   - yvex_gguf_qtype_geometry_find
 *   - yvex_gguf_qtype_storage_bytes
 *   - yvex_gguf_qtype_validate_tensor_storage
 *   - yvex_gguf_qtype_refusal_reason
 *   - yvex_gguf_artifact_abi_report_build qtype summary facts
 *
 * Commands:
 *   - make test-core
 *   - sh tests/test_gguf_qtype_abi.sh
 *
 * Expected:
 *   - exits 0 on success
 *   - does not prove qtype policy selection, backend arithmetic, artifact
 *     emission, materialization, or runtime support
 */
#include <limits.h>
#include <string.h>

#include "test.h"
#include "yvex_gguf_private.h"

static int test_known_scalar_geometry(void)
{
    const yvex_gguf_qtype_geometry *geometry;
    const char *reason = NULL;

    geometry = yvex_gguf_qtype_geometry_find(0u);
    YVEX_TEST_ASSERT(geometry != NULL, "F32 qtype geometry exists");
    YVEX_TEST_ASSERT_STREQ(geometry->name, "F32", "F32 qtype name");
    YVEX_TEST_ASSERT(geometry->storage_class == YVEX_GGUF_QTYPE_STORAGE_SCALAR_FLOAT,
                     "F32 storage class");
    YVEX_TEST_ASSERT(geometry->block_size == 1u, "F32 block size");
    YVEX_TEST_ASSERT(geometry->bytes_per_block == 4u, "F32 bytes per block");
    YVEX_TEST_ASSERT(geometry->scalar_width == 4u, "F32 scalar width");
    YVEX_TEST_ASSERT(yvex_gguf_qtype_supported_for_storage(0u, &reason) == 1,
                     "F32 storage geometry accepted");
    YVEX_TEST_ASSERT_STREQ(yvex_gguf_qtype_name(0u), "F32", "F32 canonical name");
    return 0;
}

static int test_known_block_geometry(void)
{
    const yvex_gguf_qtype_geometry *geometry;

    geometry = yvex_gguf_qtype_geometry_find(2u);
    YVEX_TEST_ASSERT(geometry != NULL, "Q4_0 qtype geometry exists");
    YVEX_TEST_ASSERT_STREQ(geometry->name, "Q4_0", "Q4_0 qtype name");
    YVEX_TEST_ASSERT(geometry->storage_class == YVEX_GGUF_QTYPE_STORAGE_BLOCK_QUANTIZED,
                     "Q4_0 storage class");
    YVEX_TEST_ASSERT(geometry->block_size == 32u, "Q4_0 block size");
    YVEX_TEST_ASSERT(geometry->bytes_per_block == 18u, "Q4_0 bytes per block");
    return 0;
}

static int test_unknown_and_refused_qtypes(void)
{
    const char *reason = NULL;

    YVEX_TEST_ASSERT(yvex_gguf_qtype_geometry_find(9999u) == NULL,
                     "unknown qtype lookup misses");
    YVEX_TEST_ASSERT(yvex_gguf_qtype_supported_for_storage(9999u, &reason) == 0,
                     "unknown qtype refused");
    YVEX_TEST_ASSERT(reason && strstr(reason, "unknown") != NULL,
                     "unknown qtype reason names unknown");
    YVEX_TEST_ASSERT_STREQ(yvex_gguf_qtype_name(9999u), "UNKNOWN",
                           "unknown qtype name");

    YVEX_TEST_ASSERT(yvex_gguf_qtype_supported_for_storage(12u, &reason) == 0,
                     "recognized qtype without formula refused");
    YVEX_TEST_ASSERT(reason && strstr(reason, "byte geometry") != NULL,
                     "recognized refusal names geometry");
    return 0;
}

static int test_geometry_table_has_no_invalid_known_rows(void)
{
    size_t i;

    for (i = 0u; i < yvex_gguf_qtype_geometry_count(); ++i) {
        const yvex_gguf_qtype_geometry *geometry = yvex_gguf_qtype_geometry_at(i);
        YVEX_TEST_ASSERT(geometry != NULL, "qtype geometry row exists");
        YVEX_TEST_ASSERT(geometry->name != NULL && geometry->name[0] != '\0',
                         "qtype geometry row has name");
        if (geometry->status == YVEX_GGUF_QTYPE_STATUS_KNOWN) {
            YVEX_TEST_ASSERT(geometry->block_size > 0u, "known qtype block size");
            YVEX_TEST_ASSERT(geometry->bytes_per_block > 0u,
                             "known qtype bytes per block");
        }
    }
    return 0;
}

static int test_storage_byte_calculation(void)
{
    const char *reason = NULL;
    unsigned long long bytes = 0ull;

    YVEX_TEST_ASSERT(yvex_gguf_qtype_storage_bytes(0u, 32ull, &bytes, &reason) == 1,
                     "F32 byte calculation succeeds");
    YVEX_TEST_ASSERT(bytes == 128ull, "F32 byte count");
    YVEX_TEST_ASSERT(reason && strstr(reason, "scalar") != NULL,
                     "F32 reason names scalar");

    YVEX_TEST_ASSERT(yvex_gguf_qtype_storage_bytes(2u, 32ull, &bytes, &reason) == 1,
                     "Q4_0 byte calculation succeeds");
    YVEX_TEST_ASSERT(bytes == 18ull, "Q4_0 one block byte count");

    YVEX_TEST_ASSERT(yvex_gguf_qtype_storage_bytes(2u, 33ull, &bytes, &reason) == 1,
                     "Q4_0 non-multiple byte calculation succeeds");
    YVEX_TEST_ASSERT(bytes == 36ull, "Q4_0 ceil-block byte count");
    YVEX_TEST_ASSERT(reason && strstr(reason, "ceil-block") != NULL,
                     "non-multiple policy names ceil-block padding");
    return 0;
}

static int test_overflow_and_range_mismatch(void)
{
    const char *reason = NULL;
    unsigned long long bytes = 0ull;
    unsigned long long expected = 0ull;

    YVEX_TEST_ASSERT(yvex_gguf_qtype_storage_bytes(0u,
                                                   (ULLONG_MAX / 4ull) + 1ull,
                                                   &bytes,
                                                   &reason) == 0,
                     "F32 storage overflow refused");
    YVEX_TEST_ASSERT(reason && strstr(reason, "overflow") != NULL,
                     "overflow reason names overflow");

    YVEX_TEST_ASSERT(yvex_gguf_qtype_validate_tensor_storage(0u,
                                                             32ull,
                                                             64ull,
                                                             &expected,
                                                             &reason) == 0,
                     "range mismatch refused");
    YVEX_TEST_ASSERT(expected == 128ull, "range mismatch expected bytes");
    YVEX_TEST_ASSERT(reason && strstr(reason, "does not match") != NULL,
                     "range mismatch reason");
    return 0;
}

static int test_report_integration(void)
{
    yvex_gguf_abi_report report;
    yvex_error err;
    int rc;

    yvex_error_clear(&err);
    rc = yvex_gguf_artifact_abi_report_build("tests/fixtures/gguf/valid-metadata-tensors.gguf",
                                             &report,
                                             &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "GGUF ABI report builds");
    YVEX_TEST_ASSERT(report.qtype.status == YVEX_GGUF_ABI_SECTION_OK,
                     "qtype ABI report accepted");
    YVEX_TEST_ASSERT(report.qtype.checked_tensor_count == 1ull,
                     "qtype checked tensor count");
    YVEX_TEST_ASSERT(report.qtype.known_tensor_count == 1ull,
                     "qtype known tensor count");
    YVEX_TEST_ASSERT(report.qtype.total_storage_bytes == 128ull,
                     "qtype report total storage bytes");
    YVEX_TEST_ASSERT(report.tensor_info.qtype_known_tensor_count == 1ull,
                     "tensor_info qtype integration");
    YVEX_TEST_ASSERT(report.range.qtype_checked_tensor_count == 1ull,
                     "range qtype integration");
    YVEX_TEST_ASSERT(report.range.total_expected_storage_bytes == 128ull,
                     "range expected qtype bytes");
    YVEX_TEST_ASSERT_STREQ(report.next_row, YVEX_GGUF_QTYPE_ABI_NEXT_ROW,
                           "qtype ABI next row");
    return 0;
}

static int test_report_row_facts(void)
{
    const yvex_gguf_qtype_geometry *geometry;
    yvex_gguf_qtype_report_row row;

    geometry = yvex_gguf_qtype_geometry_find(2u);
    yvex_gguf_qtype_report_row_from_geometry(geometry, 33ull, &row);
    YVEX_TEST_ASSERT(row.qtype == 2u, "report row qtype");
    YVEX_TEST_ASSERT_STREQ(row.name, "Q4_0", "report row name");
    YVEX_TEST_ASSERT_STREQ(row.storage_class, "block-quantized",
                           "report row storage class");
    YVEX_TEST_ASSERT_STREQ(row.status, "known-storage-geometry",
                           "report row status");
    YVEX_TEST_ASSERT(row.expected_storage_bytes == 36ull,
                     "report row expected bytes");
    YVEX_TEST_ASSERT_STREQ(row.next_row, YVEX_GGUF_QTYPE_ABI_NEXT_ROW,
                           "report row next row");
    return 0;
}

int yvex_test_gguf_qtype_abi(void)
{
    if (test_known_scalar_geometry() != 0) return 1;
    if (test_known_block_geometry() != 0) return 1;
    if (test_unknown_and_refused_qtypes() != 0) return 1;
    if (test_geometry_table_has_no_invalid_known_rows() != 0) return 1;
    if (test_storage_byte_calculation() != 0) return 1;
    if (test_overflow_and_range_mismatch() != 0) return 1;
    if (test_report_integration() != 0) return 1;
    if (test_report_row_facts() != 0) return 1;
    return 0;
}
