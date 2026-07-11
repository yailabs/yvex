/*
 * YVEX - canonical row-aware GGUF qtype ABI tests
 *
 * This test pins identity and storage geometry to ggml commit
 * af97976c7810cdabb1863172f31c432dab767de7. It proves storage facts only;
 * quantization, dequantization, emission, compute, and runtime remain separate.
 */
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <yvex/gguf_qtype.h>

#include "test.h"
#include "yvex_gguf_private.h"

typedef struct {
    const char *name;
    yvex_gguf_qtype_identity_status identity;
    unsigned int block_size;
    unsigned int block_bytes;
} pinned_qtype;

static const pinned_qtype pinned[] = {
    {"F32", YVEX_GGUF_QTYPE_IDENTITY_ADMITTED, 1u, 4u},
    {"F16", YVEX_GGUF_QTYPE_IDENTITY_ADMITTED, 1u, 2u},
    {"Q4_0", YVEX_GGUF_QTYPE_IDENTITY_ADMITTED, 32u, 18u},
    {"Q4_1", YVEX_GGUF_QTYPE_IDENTITY_ADMITTED, 32u, 20u},
    {"Q4_2", YVEX_GGUF_QTYPE_IDENTITY_REMOVED, 0u, 0u},
    {"Q4_3", YVEX_GGUF_QTYPE_IDENTITY_REMOVED, 0u, 0u},
    {"Q5_0", YVEX_GGUF_QTYPE_IDENTITY_ADMITTED, 32u, 22u},
    {"Q5_1", YVEX_GGUF_QTYPE_IDENTITY_ADMITTED, 32u, 24u},
    {"Q8_0", YVEX_GGUF_QTYPE_IDENTITY_ADMITTED, 32u, 34u},
    {"Q8_1", YVEX_GGUF_QTYPE_IDENTITY_ADMITTED, 32u, 36u},
    {"Q2_K", YVEX_GGUF_QTYPE_IDENTITY_ADMITTED, 256u, 84u},
    {"Q3_K", YVEX_GGUF_QTYPE_IDENTITY_ADMITTED, 256u, 110u},
    {"Q4_K", YVEX_GGUF_QTYPE_IDENTITY_ADMITTED, 256u, 144u},
    {"Q5_K", YVEX_GGUF_QTYPE_IDENTITY_ADMITTED, 256u, 176u},
    {"Q6_K", YVEX_GGUF_QTYPE_IDENTITY_ADMITTED, 256u, 210u},
    {"Q8_K", YVEX_GGUF_QTYPE_IDENTITY_ADMITTED, 256u, 292u},
    {"IQ2_XXS", YVEX_GGUF_QTYPE_IDENTITY_ADMITTED, 256u, 66u},
    {"IQ2_XS", YVEX_GGUF_QTYPE_IDENTITY_ADMITTED, 256u, 74u},
    {"IQ3_XXS", YVEX_GGUF_QTYPE_IDENTITY_ADMITTED, 256u, 98u},
    {"IQ1_S", YVEX_GGUF_QTYPE_IDENTITY_ADMITTED, 256u, 50u},
    {"IQ4_NL", YVEX_GGUF_QTYPE_IDENTITY_ADMITTED, 32u, 18u},
    {"IQ3_S", YVEX_GGUF_QTYPE_IDENTITY_ADMITTED, 256u, 110u},
    {"IQ2_S", YVEX_GGUF_QTYPE_IDENTITY_ADMITTED, 256u, 82u},
    {"IQ4_XS", YVEX_GGUF_QTYPE_IDENTITY_ADMITTED, 256u, 136u},
    {"I8", YVEX_GGUF_QTYPE_IDENTITY_ADMITTED, 1u, 1u},
    {"I16", YVEX_GGUF_QTYPE_IDENTITY_ADMITTED, 1u, 2u},
    {"I32", YVEX_GGUF_QTYPE_IDENTITY_ADMITTED, 1u, 4u},
    {"I64", YVEX_GGUF_QTYPE_IDENTITY_ADMITTED, 1u, 8u},
    {"F64", YVEX_GGUF_QTYPE_IDENTITY_ADMITTED, 1u, 8u},
    {"IQ1_M", YVEX_GGUF_QTYPE_IDENTITY_ADMITTED, 256u, 56u},
    {"BF16", YVEX_GGUF_QTYPE_IDENTITY_ADMITTED, 1u, 2u},
    {"Q4_0_4_4", YVEX_GGUF_QTYPE_IDENTITY_REMOVED, 0u, 0u},
    {"Q4_0_4_8", YVEX_GGUF_QTYPE_IDENTITY_REMOVED, 0u, 0u},
    {"Q4_0_8_8", YVEX_GGUF_QTYPE_IDENTITY_REMOVED, 0u, 0u},
    {"TQ1_0", YVEX_GGUF_QTYPE_IDENTITY_ADMITTED, 256u, 54u},
    {"TQ2_0", YVEX_GGUF_QTYPE_IDENTITY_ADMITTED, 256u, 66u},
    {"IQ4_NL_4_4", YVEX_GGUF_QTYPE_IDENTITY_REMOVED, 0u, 0u},
    {"IQ4_NL_4_8", YVEX_GGUF_QTYPE_IDENTITY_REMOVED, 0u, 0u},
    {"IQ4_NL_8_8", YVEX_GGUF_QTYPE_IDENTITY_REMOVED, 0u, 0u},
    {"MXFP4", YVEX_GGUF_QTYPE_IDENTITY_ADMITTED, 32u, 17u},
    {"NVFP4", YVEX_GGUF_QTYPE_IDENTITY_OUTSIDE_BASELINE, 64u, 36u},
    {"Q1_0", YVEX_GGUF_QTYPE_IDENTITY_OUTSIDE_BASELINE, 128u, 18u},
    {"Q2_0", YVEX_GGUF_QTYPE_IDENTITY_OUTSIDE_BASELINE, 64u, 18u}
};

static void append_u32(unsigned char *buf, size_t *len, unsigned int value)
{
    buf[(*len)++] = (unsigned char)(value & 0xffu);
    buf[(*len)++] = (unsigned char)((value >> 8) & 0xffu);
    buf[(*len)++] = (unsigned char)((value >> 16) & 0xffu);
    buf[(*len)++] = (unsigned char)((value >> 24) & 0xffu);
}

static void append_u64(unsigned char *buf, size_t *len, unsigned long long value)
{
    unsigned int i;

    for (i = 0u; i < 8u; ++i) {
        buf[(*len)++] = (unsigned char)((value >> (8u * i)) & 0xffu);
    }
}

static void append_string(unsigned char *buf, size_t *len, const char *text)
{
    size_t text_len = strlen(text);

    append_u64(buf, len, text_len);
    memcpy(buf + *len, text, text_len);
    *len += text_len;
}

static int write_q4_fixture(const char *path, unsigned long long row_width)
{
    unsigned char buf[512];
    size_t len = 0u;
    FILE *fp;

    memcpy(buf + len, "GGUF", 4u);
    len += 4u;
    append_u32(buf, &len, 3u);
    append_u64(buf, &len, 1ull);
    append_u64(buf, &len, 1ull);
    append_string(buf, &len, "general.architecture");
    append_u32(buf, &len, 8u);
    append_string(buf, &len, "deepseek");
    append_string(buf, &len, "q4.weight");
    append_u32(buf, &len, 2u);
    append_u64(buf, &len, row_width);
    append_u64(buf, &len, 2ull);
    append_u32(buf, &len, YVEX_GGUF_QTYPE_Q4_0);
    append_u64(buf, &len, 0ull);
    while ((len % 32u) != 0u) buf[len++] = 0u;
    memset(buf + len, 0, 36u);
    len += 36u;

    fp = fopen(path, "wb");
    if (!fp) return 0;
    if (fwrite(buf, 1u, len, fp) != len) {
        fclose(fp);
        return 0;
    }
    return fclose(fp) == 0;
}

static int test_pinned_registry_parity(void)
{
    size_t i;

    YVEX_TEST_ASSERT_STREQ(YVEX_GGUF_QTYPE_ABI_UPSTREAM_COMMIT,
                           "af97976c7810cdabb1863172f31c432dab767de7",
                           "pinned ggml revision");
    YVEX_TEST_ASSERT(yvex_gguf_qtype_geometry_count() ==
                     sizeof(pinned) / sizeof(pinned[0]),
                     "pinned qtype row count");
    for (i = 0u; i < sizeof(pinned) / sizeof(pinned[0]); ++i) {
        const yvex_gguf_qtype_geometry *geometry =
            yvex_gguf_qtype_geometry_at(i);
        YVEX_TEST_ASSERT(geometry != NULL, "pinned qtype row exists");
        YVEX_TEST_ASSERT(geometry->qtype == i, "pinned qtype numeric identity");
        YVEX_TEST_ASSERT_STREQ(geometry->name, pinned[i].name,
                               "pinned qtype canonical name");
        YVEX_TEST_ASSERT(geometry->identity_status == pinned[i].identity,
                         "pinned qtype identity status");
        YVEX_TEST_ASSERT(geometry->block_size == pinned[i].block_size,
                         "pinned qtype block width");
        YVEX_TEST_ASSERT(geometry->bytes_per_block == pinned[i].block_bytes,
                         "pinned qtype block bytes");
    }
    return 0;
}

static int test_identity_admission(void)
{
    yvex_gguf_qtype_storage_result result;
    unsigned long long dims[] = {32ull};

    YVEX_TEST_ASSERT(yvex_gguf_qtype_tensor_storage(
                         YVEX_GGUF_QTYPE_Q4_2_REMOVED, dims, 1u, &result) ==
                     YVEX_GGUF_QTYPE_STORAGE_REMOVED_ID,
                     "removed qtype refused distinctly");
    YVEX_TEST_ASSERT(yvex_gguf_qtype_tensor_storage(
                         YVEX_GGUF_QTYPE_NVFP4_OUTSIDE_BASELINE, dims, 1u, &result) ==
                     YVEX_GGUF_QTYPE_STORAGE_OUTSIDE_BASELINE,
                     "outside-baseline qtype refused distinctly");
    YVEX_TEST_ASSERT(yvex_gguf_qtype_tensor_storage(9999u, dims, 1u, &result) ==
                     YVEX_GGUF_QTYPE_STORAGE_UNKNOWN_ID,
                     "unknown qtype refused distinctly");
    YVEX_TEST_ASSERT_STREQ(yvex_gguf_qtype_identity_status_name(
                               YVEX_GGUF_QTYPE_IDENTITY_RESERVED),
                           "reserved", "reserved identity has stable state");
    YVEX_TEST_ASSERT(yvex_gguf_qtype_reference_dequantization_supported(
                         YVEX_GGUF_QTYPE_Q4_0) == 0,
                     "known geometry does not imply reference decoder");
    return 0;
}

static int expect_tensor_bytes(unsigned int qtype,
                               const unsigned long long *dims,
                               unsigned int rank,
                               unsigned long long expected)
{
    yvex_gguf_qtype_storage_result result;

    if (yvex_gguf_qtype_tensor_storage(qtype, dims, rank, &result) !=
        YVEX_GGUF_QTYPE_STORAGE_OK) return 0;
    return result.total_bytes == expected;
}

static int test_row_aware_geometry(void)
{
    yvex_gguf_qtype_storage_result result;
    unsigned long long f32[] = {32ull};
    unsigned long long q4_two_rows[] = {32ull, 2ull};
    unsigned long long q4_flat_only[] = {16ull, 2ull};
    unsigned long long q4_partial[] = {33ull};
    unsigned long long q4k[] = {256ull, 2ull};
    unsigned long long iq2[] = {256ull};
    unsigned long long mxfp4[] = {32ull};
    unsigned long long zero[] = {0ull};
    unsigned long long too_many[] = {1ull, 1ull, 1ull, 1ull, 1ull};

    YVEX_TEST_ASSERT(expect_tensor_bytes(YVEX_GGUF_QTYPE_F32, f32, 1u, 128ull),
                     "scalar row bytes");
    YVEX_TEST_ASSERT(expect_tensor_bytes(YVEX_GGUF_QTYPE_Q4_0,
                                         q4_two_rows, 2u, 36ull),
                     "Q4_0 two complete rows");
    YVEX_TEST_ASSERT(yvex_gguf_qtype_tensor_storage(
                         YVEX_GGUF_QTYPE_Q4_0, q4_flat_only, 2u, &result) ==
                     YVEX_GGUF_QTYPE_STORAGE_ROW_BLOCK_MISMATCH,
                     "flattened alignment cannot admit partial rows");
    YVEX_TEST_ASSERT(yvex_gguf_qtype_tensor_storage(
                         YVEX_GGUF_QTYPE_Q4_0, q4_partial, 1u, &result) ==
                     YVEX_GGUF_QTYPE_STORAGE_ROW_BLOCK_MISMATCH,
                     "partial row is not padded");
    YVEX_TEST_ASSERT(expect_tensor_bytes(YVEX_GGUF_QTYPE_Q4_K, q4k, 2u, 288ull),
                     "K-family row geometry");
    YVEX_TEST_ASSERT(expect_tensor_bytes(YVEX_GGUF_QTYPE_IQ2_XXS, iq2, 1u, 66ull),
                     "IQ-family row geometry");
    YVEX_TEST_ASSERT(expect_tensor_bytes(YVEX_GGUF_QTYPE_TQ1_0, iq2, 1u, 54ull),
                     "TQ-family row geometry");
    YVEX_TEST_ASSERT(expect_tensor_bytes(YVEX_GGUF_QTYPE_MXFP4, mxfp4, 1u, 17ull),
                     "MXFP4 row geometry");
    YVEX_TEST_ASSERT(yvex_gguf_qtype_tensor_storage(
                         YVEX_GGUF_QTYPE_F32, zero, 1u, &result) ==
                     YVEX_GGUF_QTYPE_STORAGE_INVALID_DIMENSION,
                     "zero dimension refused");
    YVEX_TEST_ASSERT(yvex_gguf_qtype_tensor_storage(
                         YVEX_GGUF_QTYPE_F32, f32, 0u, &result) ==
                     YVEX_GGUF_QTYPE_STORAGE_INVALID_RANK,
                     "zero rank refused");
    YVEX_TEST_ASSERT(yvex_gguf_qtype_tensor_storage(
                         YVEX_GGUF_QTYPE_F32, too_many, 5u, &result) ==
                     YVEX_GGUF_QTYPE_STORAGE_INVALID_RANK,
                     "excess rank refused");
    return 0;
}

static int test_overflow_statuses(void)
{
    yvex_gguf_qtype_storage_result result;
    unsigned long long aligned_max = ULLONG_MAX - (ULLONG_MAX % 32ull);
    unsigned long long row_bytes[] = {0ull};
    unsigned long long row_count[] = {32ull, ULLONG_MAX, 2ull};
    unsigned long long elements[] = {0ull, 2ull};
    unsigned long long total[] = {32ull, ULLONG_MAX / 32ull};

    row_bytes[0] = aligned_max;
    elements[0] = aligned_max;
    YVEX_TEST_ASSERT(yvex_gguf_qtype_tensor_storage(
                         YVEX_GGUF_QTYPE_Q8_1, row_bytes, 1u, &result) ==
                     YVEX_GGUF_QTYPE_STORAGE_ROW_BYTE_OVERFLOW,
                     "row byte overflow is typed");
    YVEX_TEST_ASSERT(yvex_gguf_qtype_tensor_storage(
                         YVEX_GGUF_QTYPE_Q4_0, row_count, 3u, &result) ==
                     YVEX_GGUF_QTYPE_STORAGE_ROW_COUNT_OVERFLOW,
                     "row count overflow is typed");
    YVEX_TEST_ASSERT(yvex_gguf_qtype_tensor_storage(
                         YVEX_GGUF_QTYPE_Q4_0, elements, 2u, &result) ==
                     YVEX_GGUF_QTYPE_STORAGE_ELEMENT_COUNT_OVERFLOW,
                     "element count overflow is typed");
    YVEX_TEST_ASSERT(yvex_gguf_qtype_tensor_storage(
                         YVEX_GGUF_QTYPE_Q8_1, total, 2u, &result) ==
                     YVEX_GGUF_QTYPE_STORAGE_TOTAL_BYTE_OVERFLOW,
                     "total byte overflow is typed");
    return 0;
}

static int test_legacy_row_and_range_validation(void)
{
    yvex_gguf_qtype_storage_result result;
    const char *reason = NULL;
    unsigned long long dims[] = {32ull, 2ull};
    unsigned long long bytes = 0ull;

    YVEX_TEST_ASSERT(yvex_gguf_qtype_storage_bytes(
                         YVEX_GGUF_QTYPE_Q4_0, 32ull, &bytes, &reason) == 1,
                     "single-row compatibility calculation succeeds");
    YVEX_TEST_ASSERT(bytes == 18ull, "single-row compatibility bytes");
    YVEX_TEST_ASSERT(yvex_gguf_qtype_storage_bytes(
                         YVEX_GGUF_QTYPE_Q4_0, 33ull, &bytes, &reason) == 0,
                     "single-row compatibility refuses partial block");
    YVEX_TEST_ASSERT(yvex_gguf_qtype_validate_tensor_storage(
                         YVEX_GGUF_QTYPE_Q4_0, dims, 2u, 35ull, &result) ==
                     YVEX_GGUF_QTYPE_STORAGE_EXPECTED_ACTUAL_MISMATCH,
                     "actual span mismatch is typed");
    YVEX_TEST_ASSERT(result.total_bytes == 36ull,
                     "range mismatch preserves expected bytes");
    return 0;
}

static int test_report_integration(void)
{
    yvex_gguf_abi_report report;
    yvex_gguf_qtype_report_row row;
    const yvex_gguf_qtype_geometry *geometry;
    unsigned long long dims[] = {32ull, 2ull};
    yvex_error err;
    int rc;

    yvex_error_clear(&err);
    rc = yvex_gguf_artifact_abi_report_build(
        "tests/fixtures/gguf/valid-metadata-tensors.gguf", &report, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "GGUF ABI report builds");
    YVEX_TEST_ASSERT(report.qtype.status == YVEX_GGUF_ABI_SECTION_OK,
                     "qtype ABI report accepted");
    YVEX_TEST_ASSERT(report.qtype.checked_tensor_count == 1ull,
                     "qtype checked tensor count");
    YVEX_TEST_ASSERT(report.qtype.total_storage_bytes == 128ull,
                     "qtype report total storage bytes");
    YVEX_TEST_ASSERT(report.range.total_expected_storage_bytes == 128ull,
                     "range owner consumes canonical bytes");
    YVEX_TEST_ASSERT_STREQ(report.next_row, YVEX_GGUF_QTYPE_ABI_NEXT_ROW,
                           "qtype ABI next row");

    geometry = yvex_gguf_qtype_geometry_find(YVEX_GGUF_QTYPE_Q4_0);
    yvex_gguf_qtype_report_row_from_geometry(geometry, dims, 2u, &row);
    YVEX_TEST_ASSERT_STREQ(row.identity_status, "admitted", "report identity");
    YVEX_TEST_ASSERT_STREQ(row.storage_status, "ok", "report storage status");
    YVEX_TEST_ASSERT_STREQ(row.reference_dequantization, "unavailable",
                           "report decoder boundary");
    YVEX_TEST_ASSERT(row.expected_storage_bytes == 36ull,
                     "report uses row-aware bytes");
    return 0;
}

static int test_gguf_and_range_consumers_preserve_rows(void)
{
    const char *valid_path = "build/tests/gguf-qtype/q4-valid.gguf";
    const char *invalid_path = "build/tests/gguf-qtype/q4-invalid.gguf";
    yvex_gguf_abi_report report;
    yvex_error err;
    int rc;

    (void)mkdir("build", 0777);
    (void)mkdir("build/tests", 0777);
    (void)mkdir("build/tests/gguf-qtype", 0777);
    YVEX_TEST_ASSERT(write_q4_fixture(valid_path, 32ull),
                     "write row-aware Q4_0 fixture");
    YVEX_TEST_ASSERT(write_q4_fixture(invalid_path, 16ull),
                     "write partial-row Q4_0 fixture");

    yvex_error_clear(&err);
    rc = yvex_gguf_artifact_abi_report_build(valid_path, &report, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "GGUF owner accepts two complete Q4_0 rows");
    YVEX_TEST_ASSERT(report.qtype.total_storage_bytes == 36ull,
                     "GGUF qtype aggregate uses row geometry");
    YVEX_TEST_ASSERT(report.range.total_expected_storage_bytes == 36ull,
                     "GGUF range map uses row geometry");

    yvex_error_clear(&err);
    rc = yvex_gguf_artifact_abi_report_build(invalid_path, &report, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK,
                     "GGUF report returns typed refusal facts");
    YVEX_TEST_ASSERT(report.qtype.status == YVEX_GGUF_ABI_SECTION_REFUSED,
                     "GGUF owner refuses flattened-only Q4_0 alignment");
    YVEX_TEST_ASSERT(strstr(report.qtype.reason, "row width") != NULL,
                     "GGUF report retains row mismatch reason");
    return 0;
}

int yvex_test_gguf_qtype_abi(void)
{
    if (test_pinned_registry_parity() != 0) return 1;
    if (test_identity_admission() != 0) return 1;
    if (test_row_aware_geometry() != 0) return 1;
    if (test_overflow_statuses() != 0) return 1;
    if (test_legacy_row_and_range_validation() != 0) return 1;
    if (test_report_integration() != 0) return 1;
    if (test_gguf_and_range_consumers_preserve_rows() != 0) return 1;
    return 0;
}
