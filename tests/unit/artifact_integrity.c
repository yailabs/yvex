/*
 * YVEX - Artifact integrity validator tests
 */
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <yvex/yvex.h>

#include "test.h"

static int first_issue_is(const yvex_artifact_integrity_report *report,
                          const char *code)
{
    const yvex_integrity_issue *issue = yvex_artifact_integrity_issue_at(report, 0);

    return issue && strcmp(issue->code, code) == 0;
}

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

    for (i = 0; i < 8u; ++i) {
        buf[(*len)++] = (unsigned char)((value >> (i * 8u)) & 0xffu);
    }
}

static void append_bytes(unsigned char *buf, size_t *len, const void *data, size_t data_len)
{
    memcpy(buf + *len, data, data_len);
    *len += data_len;
}

static void append_string(unsigned char *buf, size_t *len, const char *text)
{
    size_t text_len = strlen(text);

    append_u64(buf, len, (unsigned long long)text_len);
    append_bytes(buf, len, text, text_len);
}

static void append_metadata_string(unsigned char *buf, size_t *len,
                                   const char *key,
                                   const char *value)
{
    append_string(buf, len, key);
    append_u32(buf, len, 8u);
    append_string(buf, len, value);
}

static void append_tensor(unsigned char *buf, size_t *len,
                          const char *name,
                          unsigned long long offset)
{
    append_string(buf, len, name);
    append_u32(buf, len, 2u);
    append_u64(buf, len, 4ull);
    append_u64(buf, len, 8ull);
    append_u32(buf, len, 0u);
    append_u64(buf, len, offset);
}

static int write_duplicate_tensor_fixture(const char *path)
{
    unsigned char buf[1024];
    size_t len = 0u;
    FILE *fp;

    append_bytes(buf, &len, "GGUF", 4u);
    append_u32(buf, &len, 3u);
    append_u64(buf, &len, 2ull);
    append_u64(buf, &len, 1ull);
    append_metadata_string(buf, &len, "general.architecture", "deepseek");
    append_tensor(buf, &len, "token_embd.weight", 0ull);
    append_tensor(buf, &len, "token_embd.weight", 128ull);
    while ((len % 32u) != 0u) {
        buf[len++] = 0u;
    }
    memset(buf + len, 0, 256u);
    len += 256u;

    fp = fopen(path, "wb");
    if (!fp) {
        return 0;
    }
    if (fwrite(buf, 1u, len, fp) != len) {
        fclose(fp);
        return 0;
    }
    fclose(fp);
    return 1;
}

static int test_valid_fixture_passes(void)
{
    yvex_artifact_integrity_report report;
    yvex_error err;
    int rc;

    yvex_error_clear(&err);
    rc = yvex_artifact_integrity_check_path("tests/fixtures/gguf/valid-tokenizer-simple.gguf",
                                            NULL,
                                            &report,
                                            &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "valid tiny GGUF passes");
    YVEX_TEST_ASSERT(report.passed == 1, "valid report passes");
    YVEX_TEST_ASSERT(report.file_size > 0, "valid file size reported");
    YVEX_TEST_ASSERT(report.tensor_count == 1, "valid tensor count");
    YVEX_TEST_ASSERT(report.known_tensor_bytes == 128, "valid tensor bytes");
    return 0;
}

static int test_bad_magic_fails(void)
{
    yvex_artifact_integrity_report report;
    yvex_error err;
    int rc;

    yvex_error_clear(&err);
    rc = yvex_artifact_integrity_check_path("tests/fixtures/gguf/bad-magic.gguf",
                                            NULL,
                                            &report,
                                            &err);
    YVEX_TEST_ASSERT(rc != YVEX_OK, "bad magic fails");
    YVEX_TEST_ASSERT(first_issue_is(&report, "bad-magic"), "bad magic code");
    return 0;
}

static int test_range_out_of_file_fails(void)
{
    yvex_artifact_integrity_report report;
    yvex_error err;
    int rc;

    yvex_error_clear(&err);
    rc = yvex_artifact_integrity_check_path("tests/fixtures/gguf/tensor-offset-out-of-bounds.gguf",
                                            NULL,
                                            &report,
                                            &err);
    YVEX_TEST_ASSERT(rc != YVEX_OK, "range out of file fails");
    YVEX_TEST_ASSERT(first_issue_is(&report, "tensor-range-out-of-file"), "range code");
    return 0;
}

static int test_zero_dimension_fails(void)
{
    yvex_artifact_integrity_report report;
    yvex_error err;
    int rc;

    yvex_error_clear(&err);
    rc = yvex_artifact_integrity_check_path("tests/fixtures/gguf/tensor-dim-zero.gguf",
                                            NULL,
                                            &report,
                                            &err);
    YVEX_TEST_ASSERT(rc != YVEX_OK, "zero dim fails");
    YVEX_TEST_ASSERT(first_issue_is(&report, "zero-dimension"), "zero dimension code");
    return 0;
}

static int test_required_tensor_missing_fails(void)
{
    yvex_artifact_integrity_options options;
    yvex_artifact_integrity_report report;
    yvex_error err;
    int rc;

    memset(&options, 0, sizeof(options));
    options.require_token_embedding = 1;
    yvex_error_clear(&err);
    rc = yvex_artifact_integrity_check_path("tests/fixtures/gguf/valid-minimal.gguf",
                                            &options,
                                            &report,
                                            &err);
    YVEX_TEST_ASSERT(rc != YVEX_OK, "missing required tensor fails");
    YVEX_TEST_ASSERT(first_issue_is(&report, "required-tensor-missing"), "missing tensor code");
    return 0;
}

static int test_token_out_of_range_fails(void)
{
    const char *path = "build/tests/artifact-integrity/controlled-f16.gguf";
    yvex_gguf_emit_options emit_options;
    yvex_gguf_emit_summary summary;
    yvex_artifact_integrity_options options;
    yvex_artifact_integrity_report report;
    yvex_error err;
    int rc;

    memset(&emit_options, 0, sizeof(emit_options));
    memset(&summary, 0, sizeof(summary));
    memset(&options, 0, sizeof(options));
    yvex_error_clear(&err);
    (void)mkdir("build", 0777);
    (void)mkdir("build/tests", 0777);
    (void)mkdir("build/tests/artifact-integrity", 0777);
    remove(path);
    emit_options.out_path = path;
    emit_options.model_name = "integrity-f16-fixture";
    emit_options.architecture = "deepseek";
    emit_options.target_qtype = "F16";
    emit_options.overwrite = 1;
    rc = yvex_gguf_emit_controlled(&emit_options, &summary, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "emit f16 fixture");

    options.require_token_embedding = 1;
    options.token_id = 99u;
    rc = yvex_artifact_integrity_check_path(path, &options, &report, &err);
    YVEX_TEST_ASSERT(rc != YVEX_OK, "token out of range fails");
    YVEX_TEST_ASSERT(first_issue_is(&report, "token-out-of-range"), "token range code");
    return 0;
}

static int test_duplicate_tensor_name_fails(void)
{
    const char *path = "build/tests/artifact-integrity/duplicate-tensor.gguf";
    yvex_artifact_integrity_report report;
    yvex_error err;
    int rc;

    (void)mkdir("build", 0777);
    (void)mkdir("build/tests", 0777);
    (void)mkdir("build/tests/artifact-integrity", 0777);
    remove(path);
    YVEX_TEST_ASSERT(write_duplicate_tensor_fixture(path), "write duplicate fixture");
    yvex_error_clear(&err);
    rc = yvex_artifact_integrity_check_path(path, NULL, &report, &err);
    YVEX_TEST_ASSERT(rc != YVEX_OK, "duplicate tensor fails");
    YVEX_TEST_ASSERT(first_issue_is(&report, "duplicate-tensor-name"), "duplicate code");
    return 0;
}

int yvex_test_artifact_integrity(void)
{
    if (test_valid_fixture_passes() != 0) return 1;
    if (test_bad_magic_fails() != 0) return 1;
    if (test_range_out_of_file_fails() != 0) return 1;
    if (test_zero_dimension_fails() != 0) return 1;
    if (test_required_tensor_missing_fails() != 0) return 1;
    if (test_token_out_of_range_fails() != 0) return 1;
    if (test_duplicate_tensor_name_fails() != 0) return 1;
    return 0;
}
