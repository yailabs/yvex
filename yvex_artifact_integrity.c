/*
 * yvex_artifact_integrity.c - Baseline GGUF artifact integrity checks.
 *
 * This module validates local GGUF structure before payload-reading runtime
 * paths trust tensor ranges. It is intentionally narrower than provenance or
 * supply-chain security.
 */

#include <yvex/artifact_integrity.h>
#include <yvex/artifact_identity.h>
#include <yvex/dtype.h>
#include <yvex/model.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void integrity_copy(char *dst, unsigned int cap, const char *src)
{
    int n;

    if (!dst || cap == 0u) {
        return;
    }
    if (!src) {
        src = "";
    }
    n = snprintf(dst, (size_t)cap, "%s", src);
    if (n < 0 || (unsigned int)n >= cap) {
        dst[cap - 1u] = '\0';
    }
}

static int add_error(yvex_artifact_integrity_report *report,
                     const char *code,
                     const char *tensor,
                     const char *reason);

static void apply_identity_digest(const yvex_artifact_file_identity *identity,
                                  const yvex_artifact_integrity_options *options,
                                  yvex_artifact_integrity_report *report)
{
    const char *expected = NULL;
    const char *registered = NULL;

    if (!identity || !report) {
        return;
    }

    report->identity_checked = 1;
    report->file_size = identity->file_size;
    integrity_copy(report->sha256, YVEX_INTEGRITY_SHA256_CAP, identity->sha256);
    integrity_copy(report->digest_status, YVEX_INTEGRITY_DIGEST_STATUS_CAP, "unregistered");

    if (options) {
        expected = options->expect_sha256;
        registered = options->registered_sha256;
    }
    if (registered && registered[0]) {
        integrity_copy(report->registered_sha256, YVEX_INTEGRITY_SHA256_CAP, registered);
    }

    if (expected && expected[0]) {
        integrity_copy(report->expected_sha256, YVEX_INTEGRITY_SHA256_CAP, expected);
        if (!yvex_sha256_hex_is_valid(expected)) {
            integrity_copy(report->digest_status, YVEX_INTEGRITY_DIGEST_STATUS_CAP, "fail");
            (void)add_error(report, "digest-invalid", "",
                            "expected SHA-256 must be 64 lowercase or uppercase hex characters");
        } else if (strcmp(identity->sha256, expected) == 0) {
            integrity_copy(report->digest_status, YVEX_INTEGRITY_DIGEST_STATUS_CAP, "pass");
        } else {
            integrity_copy(report->digest_status, YVEX_INTEGRITY_DIGEST_STATUS_CAP, "fail");
            (void)add_error(report, "digest-mismatch", "",
                            "expected SHA-256 does not match current artifact bytes");
        }
        return;
    }

    if (registered && registered[0]) {
        if (!yvex_sha256_hex_is_valid(registered)) {
            integrity_copy(report->digest_status, YVEX_INTEGRITY_DIGEST_STATUS_CAP, "missing");
            (void)add_error(report, "digest-missing", "",
                            "registered alias lacks valid SHA-256 identity");
        } else if (strcmp(identity->sha256, registered) == 0) {
            integrity_copy(report->digest_status, YVEX_INTEGRITY_DIGEST_STATUS_CAP, "pass");
        } else {
            integrity_copy(report->digest_status, YVEX_INTEGRITY_DIGEST_STATUS_CAP, "fail");
            (void)add_error(report, "digest-mismatch", "",
                            "registered SHA-256 does not match current artifact bytes");
        }
    }
}

static int add_issue(yvex_artifact_integrity_report *report,
                     yvex_integrity_severity severity,
                     const char *code,
                     const char *tensor,
                     const char *reason)
{
    yvex_integrity_issue *issue;

    if (!report) {
        return YVEX_ERR_INVALID_ARG;
    }

    if (severity == YVEX_INTEGRITY_SEVERITY_WARNING) {
        report->warning_count += 1u;
    } else {
        report->error_count += 1u;
    }

    if (report->issue_count >= YVEX_INTEGRITY_MAX_ISSUES) {
        return YVEX_OK;
    }

    issue = &report->issues[report->issue_count++];
    issue->severity = severity;
    integrity_copy(issue->code, YVEX_INTEGRITY_CODE_CAP, code);
    integrity_copy(issue->tensor, YVEX_INTEGRITY_TENSOR_CAP, tensor);
    integrity_copy(issue->reason, YVEX_INTEGRITY_REASON_CAP, reason);
    return YVEX_OK;
}

static int add_error(yvex_artifact_integrity_report *report,
                     const char *code,
                     const char *tensor,
                     const char *reason)
{
    return add_issue(report, YVEX_INTEGRITY_SEVERITY_ERROR, code, tensor, reason);
}

static int checked_mul_ull(unsigned long long a,
                           unsigned long long b,
                           unsigned long long *out)
{
    if (!out) {
        return 0;
    }
    if (a != 0ull && b > ULLONG_MAX / a) {
        return 0;
    }
    *out = a * b;
    return 1;
}

static int checked_add_ull(unsigned long long a,
                           unsigned long long b,
                           unsigned long long *out)
{
    if (!out) {
        return 0;
    }
    if (b > ULLONG_MAX - a) {
        return 0;
    }
    *out = a + b;
    return 1;
}

static int tensor_element_count(const yvex_tensor_info *tensor,
                                unsigned long long *out)
{
    unsigned long long product = 1ull;
    unsigned int i;

    if (!tensor || !out) {
        return 0;
    }
    if (tensor->rank == 0u || tensor->rank > YVEX_TENSOR_MAX_DIMS) {
        return 0;
    }
    for (i = 0; i < tensor->rank; ++i) {
        if (tensor->dims[i] == 0ull) {
            return 0;
        }
        if (!checked_mul_ull(product, tensor->dims[i], &product)) {
            return 0;
        }
    }
    *out = product;
    return 1;
}

static void set_basic_report(const yvex_artifact *artifact,
                             const yvex_gguf *gguf,
                             const yvex_tensor_table *tensors,
                             yvex_artifact_integrity_report *report)
{
    const yvex_gguf_header *header;
    const yvex_gguf_value *arch_value;
    const char *arch_text = NULL;
    unsigned long long arch_len = 0ull;

    memset(report, 0, sizeof(*report));
    report->checked = 1;
    integrity_copy(report->format, YVEX_INTEGRITY_FORMAT_CAP, "gguf");

    if (artifact) {
        integrity_copy(report->path, YVEX_ARTIFACT_PATH_CAP, yvex_artifact_path(artifact));
        report->file_size = yvex_artifact_size(artifact);
    }
    if (gguf) {
        header = yvex_gguf_header_view(gguf);
        if (header) {
            report->version = header->version;
            report->tensor_count = header->tensor_count;
        }
        arch_value = yvex_gguf_metadata_find(gguf, "general.architecture");
        if (arch_value &&
            yvex_gguf_value_as_string(arch_value, &arch_text, &arch_len) == YVEX_OK &&
            arch_text) {
            unsigned long long copy_len = arch_len;
            if (copy_len >= (unsigned long long)YVEX_INTEGRITY_ARCH_CAP) {
                copy_len = (unsigned long long)YVEX_INTEGRITY_ARCH_CAP - 1ull;
            }
            memcpy(report->architecture, arch_text, (size_t)copy_len);
            report->architecture[copy_len] = '\0';
        }
    }
    if (tensors) {
        report->tensor_count = yvex_tensor_table_count(tensors);
    }
}

static void map_parse_error_to_report(const yvex_error *source,
                                      yvex_artifact_integrity_report *report)
{
    const char *where = yvex_error_where(source);
    const char *message = yvex_error_message(source);
    const char *code = "tensor-directory-parse-failed";
    yvex_status status = yvex_error_code(source);

    if (status == YVEX_ERR_IO) {
        code = "file-open-failed";
    } else if ((strstr(message, "requires") && strstr(message, "header")) ||
               strstr(message, "magic out of bounds")) {
        code = "file-too-small";
    } else if (strstr(message, "bad GGUF magic")) {
        code = "bad-magic";
    } else if (strstr(message, "unsupported GGUF version")) {
        code = "unsupported-version";
    } else if (strstr(message, "string out of bounds")) {
        code = "malformed-string";
    } else if (strstr(message, "tensor name out of bounds")) {
        code = "tensor-directory-parse-failed";
    } else if (strstr(message, "metadata")) {
        code = "metadata-parse-failed";
    } else if (strstr(message, "empty tensor name") ||
               strstr(message, "tensor name is empty")) {
        code = "empty-tensor-name";
    } else if (strstr(message, "tensor rank")) {
        code = "rank-out-of-range";
    } else if (strstr(message, "zero tensor dimension")) {
        code = "zero-dimension";
    } else if (strstr(message, "dimension product overflow")) {
        code = "element-count-overflow";
    } else if (strstr(message, "not aligned")) {
        code = "tensor-alignment-invalid";
    } else if (strstr(message, "offset") || strstr(message, "out of bounds")) {
        code = "tensor-range-out-of-file";
    } else if (where && strstr(where, "metadata")) {
        code = "metadata-parse-failed";
    }

    (void)add_error(report, code, "", message);
}

static void set_integrity_error(yvex_error *err,
                                const yvex_artifact_integrity_report *report)
{
    const yvex_integrity_issue *issue = NULL;

    if (report && report->issue_count > 0u) {
        issue = &report->issues[0];
    }
    if (issue && issue->reason[0]) {
        yvex_error_setf(err, YVEX_ERR_FORMAT, "yvex_artifact_integrity",
                        "%s: %s", issue->code, issue->reason);
    } else {
        yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_artifact_integrity",
                       "artifact integrity validation failed");
    }
}

int yvex_artifact_integrity_validate(const yvex_artifact *artifact,
                                     const yvex_gguf *gguf,
                                     const yvex_tensor_table *tensors,
                                     const yvex_artifact_integrity_options *options,
                                     yvex_artifact_integrity_report *out,
                                     yvex_error *err)
{
    yvex_artifact_integrity_report local;
    yvex_artifact_integrity_report *report = out ? out : &local;
    unsigned long long i;
    unsigned long long file_size;
    unsigned int alignment;

    if (!artifact || !gguf || !tensors) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_artifact_integrity_validate",
                       "artifact, gguf, and tensors are required");
        return YVEX_ERR_INVALID_ARG;
    }

    set_basic_report(artifact, gguf, tensors, report);
    file_size = yvex_artifact_size(artifact);
    alignment = yvex_gguf_alignment(gguf);

    for (i = 0; i < yvex_tensor_table_count(tensors); ++i) {
        const yvex_tensor_info *tensor = yvex_tensor_table_at(tensors, i);
        unsigned long long element_count = 0ull;
        unsigned long long storage_bytes = 0ull;
        unsigned long long end_offset = 0ull;
        unsigned long long j;
        int duplicate = 0;
        int rc;

        if (!tensor) {
            (void)add_error(report, "tensor-directory-parse-failed", "",
                            "tensor table row is unavailable");
            continue;
        }
        if (!tensor->name || tensor->name[0] == '\0') {
            (void)add_error(report, "empty-tensor-name", "", "tensor name is empty");
        }
        for (j = 0; j < i; ++j) {
            const yvex_tensor_info *prev = yvex_tensor_table_at(tensors, j);
            if (prev && prev->name && tensor->name && strcmp(prev->name, tensor->name) == 0) {
                duplicate = 1;
                break;
            }
        }
        if (duplicate) {
            (void)add_error(report, "duplicate-tensor-name", tensor->name,
                            "duplicate tensor name in tensor directory");
        }
        if (tensor->rank == 0u || tensor->rank > YVEX_TENSOR_MAX_DIMS) {
            (void)add_error(report, "rank-out-of-range", tensor->name,
                            "tensor rank is outside supported bounds");
            continue;
        }
        if (!tensor_element_count(tensor, &element_count)) {
            unsigned int d;
            int zero_dim = 0;
            for (d = 0; d < tensor->rank && d < YVEX_TENSOR_MAX_DIMS; ++d) {
                if (tensor->dims[d] == 0ull) {
                    zero_dim = 1;
                }
            }
            (void)add_error(report,
                            zero_dim ? "zero-dimension" : "element-count-overflow",
                            tensor->name,
                            zero_dim ? "tensor dimensions must be non-zero"
                                     : "tensor element count overflows");
            continue;
        }
        if (tensor->dtype == YVEX_DTYPE_UNKNOWN) {
            (void)add_error(report, "unknown-dtype", tensor->name,
                            "tensor dtype is not known to YVEX");
            continue;
        }
        rc = yvex_dtype_storage_bytes(tensor->dtype, element_count, &storage_bytes, err);
        if (rc == YVEX_ERR_UNSUPPORTED) {
            (void)add_error(report, "dtype-size-unknown", tensor->name,
                            "tensor dtype has no storage byte accounting");
            yvex_error_clear(err);
            continue;
        }
        if (rc != YVEX_OK) {
            (void)add_error(report, "tensor-byte-count-overflow", tensor->name,
                            "tensor byte-count calculation failed");
            yvex_error_clear(err);
            continue;
        }
        if (tensor->storage_bytes != storage_bytes) {
            (void)add_error(report, "tensor-byte-count-overflow", tensor->name,
                            "tensor table byte count does not match checked dtype math");
            continue;
        }
        if (!checked_add_ull(report->known_tensor_bytes, storage_bytes,
                             &report->known_tensor_bytes)) {
            (void)add_error(report, "tensor-byte-count-overflow", tensor->name,
                            "known tensor byte sum overflows");
        }
        if (alignment > 0u && (tensor->relative_offset % (unsigned long long)alignment) != 0ull) {
            (void)add_error(report, "tensor-alignment-invalid", tensor->name,
                            "tensor relative offset is not aligned");
        }
        if (!checked_add_ull(tensor->absolute_offset, storage_bytes, &end_offset)) {
            (void)add_error(report, "tensor-offset-overflow", tensor->name,
                            "tensor end offset overflows");
            continue;
        }
        if (tensor->absolute_offset > file_size || end_offset > file_size) {
            (void)add_error(report, "tensor-range-out-of-file", tensor->name,
                            "tensor byte range exceeds file size");
        }
    }

    if (options && options->require_token_embedding) {
        const yvex_tensor_info *tensor = yvex_tensor_table_find(tensors, "token_embd.weight");

        if (!tensor) {
            (void)add_error(report, "required-tensor-missing", "token_embd.weight",
                            "required tensor not found: token_embd.weight");
        } else {
            if (tensor->rank != 2u) {
                (void)add_error(report, "required-tensor-rank-invalid", tensor->name,
                                "selected embedding readiness requires rank 2");
            }
            if (tensor->dtype != YVEX_DTYPE_F16) {
                (void)add_error(report, "required-tensor-dtype-invalid", tensor->name,
                                "selected embedding readiness requires F16 token_embd.weight");
            }
            if (tensor->rank == 2u && (unsigned long long)options->token_id >= tensor->dims[1]) {
                (void)add_error(report, "token-out-of-range", tensor->name,
                                "partial token id is outside embedding range");
            }
        }
    }

    report->passed = report->error_count == 0u ? 1 : 0;
    if (!report->passed) {
        set_integrity_error(err, report);
        return YVEX_ERR_FORMAT;
    }

    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_artifact_integrity_check_path(const char *path,
                                       const yvex_artifact_integrity_options *options,
                                       yvex_artifact_integrity_report *out,
                                       yvex_error *err)
{
    yvex_artifact_options artifact_options;
    yvex_artifact_file_identity identity;
    yvex_artifact *artifact = NULL;
    yvex_gguf *gguf = NULL;
    yvex_tensor_table *tensors = NULL;
    yvex_artifact_integrity_report local;
    yvex_artifact_integrity_report *report = out ? out : &local;
    int rc;

    memset(&artifact_options, 0, sizeof(artifact_options));
    memset(report, 0, sizeof(*report));
    report->checked = 1;
    integrity_copy(report->path, YVEX_ARTIFACT_PATH_CAP, path);
    integrity_copy(report->format, YVEX_INTEGRITY_FORMAT_CAP, "unknown");
    integrity_copy(report->digest_status, YVEX_INTEGRITY_DIGEST_STATUS_CAP, "unknown");

    if (!path || path[0] == '\0') {
        (void)add_error(report, "file-open-failed", "", "model path is required");
        set_integrity_error(err, report);
        return YVEX_ERR_INVALID_ARG;
    }

    rc = yvex_artifact_identity_read(path, &identity, err);
    if (rc != YVEX_OK) {
        (void)add_error(report, "file-open-failed", "", yvex_error_message(err));
        set_integrity_error(err, report);
        return rc;
    }

    artifact_options.path = path;
    artifact_options.readonly = 1;
    artifact_options.map = 1;
    rc = yvex_artifact_open(&artifact, &artifact_options, err);
    if (rc != YVEX_OK) {
        map_parse_error_to_report(err, report);
        apply_identity_digest(&identity, options, report);
        set_integrity_error(err, report);
        return rc;
    }
    report->file_size = yvex_artifact_size(artifact);

    rc = yvex_gguf_open(&gguf, artifact, err);
    if (rc == YVEX_OK) {
        rc = yvex_tensor_table_from_gguf(&tensors, gguf, err);
    }
    if (rc != YVEX_OK) {
        if (gguf) {
            set_basic_report(artifact, gguf, tensors, report);
        }
        map_parse_error_to_report(err, report);
        apply_identity_digest(&identity, options, report);
        yvex_tensor_table_close(tensors);
        yvex_gguf_close(gguf);
        yvex_artifact_close(artifact);
        set_integrity_error(err, report);
        return rc;
    }

    rc = yvex_artifact_integrity_validate(artifact, gguf, tensors, options, report, err);
    apply_identity_digest(&identity, options, report);
    report->passed = report->error_count == 0u ? 1 : 0;
    yvex_tensor_table_close(tensors);
    yvex_gguf_close(gguf);
    yvex_artifact_close(artifact);
    if (!report->passed) {
        set_integrity_error(err, report);
        return rc != YVEX_OK ? rc : YVEX_ERR_FORMAT;
    }
    yvex_error_clear(err);
    return YVEX_OK;
}

const yvex_integrity_issue *yvex_artifact_integrity_issue_at(
    const yvex_artifact_integrity_report *report,
    unsigned int index)
{
    if (!report || index >= report->issue_count) {
        return NULL;
    }
    return &report->issues[index];
}
