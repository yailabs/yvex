/*
 * YVEX - GGUF artifact ABI tests
 *
 * File: tests/unit/gguf_artifact_abi.c
 * Layer: test
 *
 * Purpose:
 *   Proves the internal GGUF artifact ABI report for container, metadata,
 *   tensor_info, and ABI-visible range facts using tiny fixtures only.
 *
 * Covers:
 *   - yvex_gguf_artifact_abi_report_build
 *   - invalid magic refusal
 *   - unsupported version refusal
 *   - malformed metadata refusal
 *   - tensor_info refusal
 *   - range refusal
 *
 * Commands:
 *   - make test-core
 *   - sh tests/test_gguf_artifact_abi.sh
 *
 * Expected:
 *   - exits 0 on success
 *   - does not prove writer, roundtrip, materialization, or runtime support
 */
#include <string.h>

#include "test.h"
#include "yvex_gguf_private.h"

static int build_report(const char *path, yvex_gguf_abi_report *report)
{
    yvex_error err;
    int rc;

    yvex_error_clear(&err);
    rc = yvex_gguf_artifact_abi_report_build(path, report, &err);
    if (rc != YVEX_OK) {
        fprintf(stderr, "GGUF ABI report build failed: %s: %s\n",
                yvex_error_where(&err), yvex_error_message(&err));
        return 1;
    }
    return 0;
}

static int test_valid_fixture(void)
{
    yvex_gguf_abi_report report;

    YVEX_TEST_ASSERT(build_report("tests/fixtures/gguf/valid-metadata-tensors.gguf", &report) == 0,
                     "valid GGUF ABI fixture builds report");
    YVEX_TEST_ASSERT(report.status == YVEX_GGUF_ABI_SECTION_REPORT_ONLY,
                     "valid GGUF ABI is report-only");
    YVEX_TEST_ASSERT(report.container.status == YVEX_GGUF_ABI_SECTION_OK,
                     "container ABI accepted");
    YVEX_TEST_ASSERT(report.container.version == 3u, "container version");
    YVEX_TEST_ASSERT(report.metadata.status == YVEX_GGUF_ABI_SECTION_OK,
                     "metadata ABI accepted");
    YVEX_TEST_ASSERT(report.metadata.entry_count == 5ull, "metadata count");
    YVEX_TEST_ASSERT(report.metadata.string_value_count >= 2ull, "metadata string count");
    YVEX_TEST_ASSERT(report.tensor_info.status == YVEX_GGUF_ABI_SECTION_OK,
                     "tensor_info ABI accepted");
    YVEX_TEST_ASSERT(report.tensor_info.tensor_count == 1ull, "tensor count");
    YVEX_TEST_ASSERT(report.tensor_info.max_rank == 2u, "tensor max rank");
    YVEX_TEST_ASSERT(report.range.status == YVEX_GGUF_ABI_SECTION_OK,
                     "range ABI accepted");
    YVEX_TEST_ASSERT(report.range.checked_tensor_count == 1ull, "range checked tensor count");
    YVEX_TEST_ASSERT(report.descriptor.status == YVEX_GGUF_ABI_SECTION_REPORT_ONLY,
                     "descriptor remains report-only");
    YVEX_TEST_ASSERT_STREQ(report.next_row, YVEX_GGUF_ABI_NEXT_ROW, "next row");
    return 0;
}

static int test_invalid_magic(void)
{
    yvex_gguf_abi_report report;

    YVEX_TEST_ASSERT(build_report("tests/fixtures/gguf/bad-magic.gguf", &report) == 0,
                     "bad magic report builds");
    YVEX_TEST_ASSERT(report.container.status == YVEX_GGUF_ABI_SECTION_MALFORMED,
                     "bad magic marks container malformed");
    YVEX_TEST_ASSERT(strstr(report.failure_reason, "magic") != NULL,
                     "bad magic reason names magic");
    return 0;
}

static int test_unsupported_version(void)
{
    yvex_gguf_abi_report report;

    YVEX_TEST_ASSERT(build_report("tests/fixtures/gguf/unsupported-version.gguf", &report) == 0,
                     "unsupported version report builds");
    YVEX_TEST_ASSERT(report.container.status == YVEX_GGUF_ABI_SECTION_UNSUPPORTED,
                     "unsupported version marks container unsupported");
    YVEX_TEST_ASSERT(report.container.version != 3u, "unsupported version is not parser version");
    return 0;
}

static int test_malformed_metadata(void)
{
    yvex_gguf_abi_report report;

    YVEX_TEST_ASSERT(build_report("tests/fixtures/gguf/metadata-unknown-type.gguf", &report) == 0,
                     "metadata malformed report builds");
    YVEX_TEST_ASSERT(report.container.status == YVEX_GGUF_ABI_SECTION_OK,
                     "container accepted before metadata refusal");
    YVEX_TEST_ASSERT(report.metadata.status == YVEX_GGUF_ABI_SECTION_UNSUPPORTED,
                     "unknown metadata type is unsupported");
    return 0;
}

static int test_tensor_info_refusal(void)
{
    yvex_gguf_abi_report report;

    YVEX_TEST_ASSERT(build_report("tests/fixtures/gguf/tensor-rank-unsupported.gguf", &report) == 0,
                     "tensor_info malformed report builds");
    YVEX_TEST_ASSERT(report.tensor_info.status == YVEX_GGUF_ABI_SECTION_MALFORMED,
                     "unsupported rank marks tensor_info malformed");
    return 0;
}

static int test_range_refusal(void)
{
    yvex_gguf_abi_report report;

    YVEX_TEST_ASSERT(build_report("tests/fixtures/gguf/tensor-offset-out-of-bounds.gguf", &report) == 0,
                     "range refusal report builds");
    YVEX_TEST_ASSERT(report.range.status == YVEX_GGUF_ABI_SECTION_REFUSED,
                     "out-of-bounds tensor offset marks range refused");
    return 0;
}

static int test_missing_file(void)
{
    yvex_gguf_abi_report report;

    YVEX_TEST_ASSERT(build_report("tests/fixtures/gguf/missing.gguf", &report) == 0,
                     "missing file report builds");
    YVEX_TEST_ASSERT(report.container.status == YVEX_GGUF_ABI_SECTION_NOT_PRESENT,
                     "missing file is not-present");
    return 0;
}

static int test_future_owned_boundaries(void)
{
    const char *reason = NULL;

    YVEX_TEST_ASSERT(yvex_gguf_writer_supported(&reason) == 0, "writer remains refused");
    YVEX_TEST_ASSERT(reason != NULL, "writer refusal reason");
    YVEX_TEST_ASSERT(yvex_gguf_roundtrip_supported(&reason) == 0, "roundtrip remains refused");
    YVEX_TEST_ASSERT(reason != NULL, "roundtrip refusal reason");
    YVEX_TEST_ASSERT(yvex_gguf_qtype_geometry_count() > 0u, "qtype geometry table exists");
    return 0;
}

int yvex_test_gguf_artifact_abi(void)
{
    if (test_valid_fixture() != 0) return 1;
    if (test_invalid_magic() != 0) return 1;
    if (test_unsupported_version() != 0) return 1;
    if (test_malformed_metadata() != 0) return 1;
    if (test_tensor_info_refusal() != 0) return 1;
    if (test_range_refusal() != 0) return 1;
    if (test_missing_file() != 0) return 1;
    if (test_future_owned_boundaries() != 0) return 1;
    return 0;
}
