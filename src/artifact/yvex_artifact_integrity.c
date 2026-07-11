/*
 * yvex_artifact_integrity.c - Local artifact integrity and refusal reports.
 *
 * This file owns structural GGUF validation, tensor range validation, shape,
 * dtype, and byte-count checks, file identity diagnostics, and integrity report
 * construction. It does not claim supply-chain security.
 */

#include <yvex/artifact_integrity.h>
#include <yvex/artifact_identity.h>
#include <yvex/dtype.h>
#include <yvex/gguf_qtype.h>
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

static int add_range_error(yvex_artifact_integrity_report *report,
                           const char *code,
                           const char *tensor,
                           const char *reason,
                           const yvex_tensor_range *range);

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

static int add_range_error(yvex_artifact_integrity_report *report,
                           const char *code,
                           const char *tensor,
                           const char *reason,
                           const yvex_tensor_range *range)
{
    unsigned int before;
    int rc;

    if (!report) {
        return YVEX_ERR_INVALID_ARG;
    }
    before = report->issue_count;
    rc = add_error(report, code, tensor, reason);
    if (rc == YVEX_OK && range && report->issue_count > before) {
        yvex_integrity_issue *issue = &report->issues[before];
        issue->relative_offset = range->tensor_relative_offset;
        issue->absolute_offset = range->tensor_absolute_offset;
        issue->tensor_bytes = range->tensor_bytes;
        issue->file_size = range->file_size;
        issue->has_range = 1;
    }
    return rc;
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

int yvex_tensor_shape_accounting_validate(
    const yvex_tensor_info *tensor,
    yvex_tensor_shape_accounting *out,
    yvex_error *err)
{
    yvex_tensor_shape_accounting local;
    yvex_tensor_shape_accounting *accounting = out ? out : &local;
    const yvex_dtype_info *dtype_info;
    const yvex_gguf_qtype_geometry *geometry;
    unsigned long long element_count = 0ull;
    unsigned long long storage_byte_count = 0ull;
    unsigned int i;
    int rc;

    if (!tensor) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG,
                       "yvex_tensor_shape_accounting_validate",
                       "tensor is required");
        return YVEX_ERR_INVALID_ARG;
    }

    memset(accounting, 0, sizeof(*accounting));
    integrity_copy(accounting->tensor_name, YVEX_INTEGRITY_TENSOR_CAP, tensor->name);
    accounting->dtype = tensor->dtype;
    accounting->rank = tensor->rank;
    for (i = 0; i < tensor->rank && i < YVEX_TENSOR_MAX_DIMS; ++i) {
        accounting->dims[i] = tensor->dims[i];
    }

    if (!tensor->name || tensor->name[0] == '\0') {
        yvex_error_set(err, YVEX_ERR_FORMAT,
                       "yvex_tensor_shape_accounting_validate",
                       "tensor name is empty");
        return YVEX_ERR_FORMAT;
    }
    if (tensor->rank == 0u || tensor->rank > YVEX_TENSOR_MAX_DIMS) {
        yvex_error_setf(err, YVEX_ERR_FORMAT,
                        "yvex_tensor_shape_accounting_validate",
                        "tensor-rank-invalid: %s rank %u", tensor->name, tensor->rank);
        return YVEX_ERR_FORMAT;
    }
    for (i = 0; i < tensor->rank; ++i) {
        if (tensor->dims[i] == 0ull) {
            yvex_error_setf(err, YVEX_ERR_FORMAT,
                            "yvex_tensor_shape_accounting_validate",
                            "tensor-dim-zero: %s", tensor->name);
            return YVEX_ERR_FORMAT;
        }
    }
    if (!tensor_element_count(tensor, &element_count)) {
        yvex_error_setf(err, YVEX_ERR_BOUNDS,
                        "yvex_tensor_shape_accounting_validate",
                        "tensor-element-count-overflow: %s", tensor->name);
        return YVEX_ERR_BOUNDS;
    }
    accounting->element_count = element_count;
    accounting->shape_valid = 1;

    dtype_info = yvex_dtype_get_info(tensor->dtype);
    if (!dtype_info || dtype_info->dtype == YVEX_DTYPE_UNKNOWN) {
        yvex_error_setf(err, YVEX_ERR_UNSUPPORTED,
                        "yvex_tensor_shape_accounting_validate",
                        "tensor-dtype-unknown: %s", tensor->name);
        return YVEX_ERR_UNSUPPORTED;
    }
    accounting->dtype_known = 1;
    accounting->dtype_valid = 1;
    geometry = yvex_gguf_qtype_geometry_find(dtype_info->ggml_type);
    accounting->storage_supported = yvex_dtype_storage_supported(tensor->dtype);
    if (geometry) {
        accounting->storage_unit_bytes = geometry->scalar_width > 0u
            ? (unsigned long long)geometry->scalar_width
            : (unsigned long long)geometry->bytes_per_block;
    }
    accounting->compute_supported_for_selected_embedding =
        tensor->dtype == YVEX_DTYPE_F16 ? 1 : 0;
    accounting->compute_supported_for_fixture_embedding =
        tensor->dtype == YVEX_DTYPE_F32 ? 1 : 0;

    rc = yvex_dtype_tensor_storage_bytes(tensor->dtype,
                                         tensor->dims,
                                         tensor->rank,
                                         &storage_byte_count,
                                         err);
    if (rc == YVEX_ERR_UNSUPPORTED) {
        yvex_error_setf(err, YVEX_ERR_UNSUPPORTED,
                        "yvex_tensor_shape_accounting_validate",
                        "tensor-dtype-size-unknown: %s", tensor->name);
        return rc;
    }
    if (rc != YVEX_OK) return rc;
    if (storage_byte_count == 0ull) {
        yvex_error_setf(err, YVEX_ERR_BOUNDS,
                        "yvex_tensor_shape_accounting_validate",
                        "tensor-byte-count-overflow: %s tensor byte count is zero",
                        tensor->name);
        return YVEX_ERR_BOUNDS;
    }
    if (tensor->storage_bytes != 0ull && tensor->storage_bytes != storage_byte_count) {
        yvex_error_setf(err, YVEX_ERR_BOUNDS,
                        "yvex_tensor_shape_accounting_validate",
                        "tensor-byte-count-overflow: %s tensor table byte count mismatch",
                        tensor->name);
        return YVEX_ERR_BOUNDS;
    }

    accounting->byte_count_computable = 1;
    accounting->storage_byte_count = storage_byte_count;
    accounting->byte_count_valid = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_selected_embedding_shape_validate(
    const yvex_tensor_info *tensor,
    unsigned int token_id,
    yvex_selected_embedding_shape *out,
    yvex_error *err)
{
    yvex_selected_embedding_shape local;
    yvex_selected_embedding_shape *shape = out ? out : &local;
    yvex_tensor_shape_accounting accounting;
    unsigned long long hidden_size;
    unsigned long long vocab_size;
    unsigned long long output_bytes = 0ull;
    unsigned long long slice_bytes = 0ull;
    int rc;

    if (!tensor) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG,
                       "yvex_selected_embedding_shape_validate",
                       "tensor is required");
        return YVEX_ERR_INVALID_ARG;
    }

    memset(shape, 0, sizeof(*shape));
    integrity_copy(shape->tensor_name, YVEX_INTEGRITY_TENSOR_CAP, tensor->name);
    shape->token_id = token_id;

    memset(&accounting, 0, sizeof(accounting));
    rc = yvex_tensor_shape_accounting_validate(tensor, &accounting, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    if (tensor->rank != 2u) {
        yvex_error_setf(err, YVEX_ERR_FORMAT,
                        "yvex_selected_embedding_shape_validate",
                        "selected-embedding-rank-invalid: %s rank %u",
                        tensor->name, tensor->rank);
        return YVEX_ERR_FORMAT;
    }
    if (tensor->dtype != YVEX_DTYPE_F16) {
        yvex_error_setf(err, YVEX_ERR_UNSUPPORTED,
                        "yvex_selected_embedding_shape_validate",
                        "selected-embedding-dtype-invalid: %s requires F16",
                        tensor->name);
        return YVEX_ERR_UNSUPPORTED;
    }

    hidden_size = tensor->dims[0];
    vocab_size = tensor->dims[1];
    shape->hidden_size = hidden_size;
    shape->vocab_size = vocab_size;
    if ((unsigned long long)token_id >= vocab_size) {
        yvex_error_setf(err, YVEX_ERR_BOUNDS,
                        "yvex_selected_embedding_shape_validate",
                        "token-out-of-range: %u >= %llu", token_id, vocab_size);
        return YVEX_ERR_BOUNDS;
    }
    if (!checked_mul_ull(hidden_size,
                         (unsigned long long)sizeof(float),
                         &output_bytes)) {
        yvex_error_setf(err, YVEX_ERR_BOUNDS,
                        "yvex_selected_embedding_shape_validate",
                        "selected-embedding-output-byte-count-overflow: %s",
                        tensor->name);
        return YVEX_ERR_BOUNDS;
    }
    if (!checked_mul_ull(hidden_size, 2ull, &slice_bytes)) {
        yvex_error_setf(err, YVEX_ERR_BOUNDS,
                        "yvex_selected_embedding_shape_validate",
                        "selected-embedding-slice-byte-count-overflow: %s",
                        tensor->name);
        return YVEX_ERR_BOUNDS;
    }

    shape->output_count = hidden_size;
    shape->output_bytes = output_bytes;
    shape->slice_bytes = slice_bytes;
    shape->shape_valid = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_tensor_range_validate(const yvex_artifact *artifact,
                               const yvex_gguf *gguf,
                               const yvex_tensor_info *tensor,
                               yvex_tensor_range *out,
                               yvex_error *err)
{
    yvex_tensor_range local;
    yvex_tensor_range *range = out ? out : &local;
    yvex_tensor_shape_accounting accounting;
    unsigned long long absolute_offset = 0ull;
    unsigned long long end_offset = 0ull;
    unsigned int i;
    int rc;

    if (!artifact || !gguf || !tensor) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_tensor_range_validate",
                       "artifact, gguf, and tensor are required");
        return YVEX_ERR_INVALID_ARG;
    }

    memset(range, 0, sizeof(*range));
    integrity_copy(range->tensor_name, YVEX_INTEGRITY_TENSOR_CAP, tensor->name);
    range->dtype = tensor->dtype;
    range->rank = tensor->rank;
    for (i = 0; i < tensor->rank && i < YVEX_TENSOR_MAX_DIMS; ++i) {
        range->dims[i] = tensor->dims[i];
    }
    range->tensor_relative_offset = tensor->relative_offset;
    range->tensor_data_offset = yvex_gguf_tensor_data_offset(gguf);
    range->file_size = yvex_artifact_size(artifact);
    range->alignment = (unsigned long long)yvex_gguf_alignment(gguf);

    memset(&accounting, 0, sizeof(accounting));
    rc = yvex_tensor_shape_accounting_validate(tensor, &accounting, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    range->element_count = accounting.element_count;
    range->dtype_size = accounting.storage_unit_bytes;
    range->tensor_bytes = accounting.storage_byte_count;
    if (range->alignment > 0ull &&
        (tensor->relative_offset % range->alignment) != 0ull) {
        yvex_error_setf(err, YVEX_ERR_FORMAT, "yvex_tensor_range_validate",
                        "tensor-alignment-invalid: %s relative offset %llu is not aligned to %llu",
                        tensor->name, tensor->relative_offset, range->alignment);
        return YVEX_ERR_FORMAT;
    }
    range->aligned = 1;

    if (!checked_add_ull(range->tensor_data_offset,
                         tensor->relative_offset,
                         &absolute_offset)) {
        yvex_error_setf(err, YVEX_ERR_BOUNDS, "yvex_tensor_range_validate",
                        "tensor-absolute-offset-overflow: %s", tensor->name);
        return YVEX_ERR_BOUNDS;
    }
    range->tensor_absolute_offset = absolute_offset;
    if (tensor->absolute_offset != 0ull && tensor->absolute_offset != absolute_offset) {
        yvex_error_setf(err, YVEX_ERR_BOUNDS, "yvex_tensor_range_validate",
                        "tensor-absolute-offset-overflow: %s tensor table absolute offset mismatch",
                        tensor->name);
        return YVEX_ERR_BOUNDS;
    }
    if (absolute_offset < range->tensor_data_offset) {
        yvex_error_setf(err, YVEX_ERR_BOUNDS, "yvex_tensor_range_validate",
                        "tensor-offset-before-data: %s", tensor->name);
        return YVEX_ERR_BOUNDS;
    }
    if (absolute_offset > range->file_size) {
        yvex_error_setf(err, YVEX_ERR_BOUNDS, "yvex_tensor_range_validate",
                        "tensor-offset-out-of-file: %s absolute offset %llu exceeds file size %llu",
                        tensor->name, absolute_offset, range->file_size);
        return YVEX_ERR_BOUNDS;
    }
    if (!checked_add_ull(absolute_offset, range->tensor_bytes, &end_offset)) {
        yvex_error_setf(err, YVEX_ERR_BOUNDS, "yvex_tensor_range_validate",
                        "tensor-end-offset-overflow: %s", tensor->name);
        return YVEX_ERR_BOUNDS;
    }
    range->tensor_end_offset = end_offset;
    if (end_offset > range->file_size) {
        yvex_error_setf(err, YVEX_ERR_BOUNDS, "yvex_tensor_range_validate",
                        "tensor-range-out-of-file: %s byte range %llu..%llu exceeds file size %llu",
                        tensor->name, absolute_offset, end_offset, range->file_size);
        return YVEX_ERR_BOUNDS;
    }

    range->range_valid = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_tensor_embedding_slice_range_validate(
    const yvex_tensor_range *range,
    unsigned int token_id,
    yvex_tensor_slice_range *out,
    yvex_error *err)
{
    yvex_tensor_slice_range local;
    yvex_tensor_slice_range *slice = out ? out : &local;
    unsigned long long hidden_size;
    unsigned long long vocab_size;
    unsigned long long token_offset_bytes = 0ull;
    unsigned long long slice_bytes = 0ull;
    unsigned long long slice_absolute_offset = 0ull;
    unsigned long long slice_end_offset = 0ull;

    if (!range) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG,
                       "yvex_tensor_embedding_slice_range_validate",
                       "range is required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(slice, 0, sizeof(*slice));
    slice->token_id = token_id;

    if (!range->range_valid || range->rank != 2u) {
        yvex_error_set(err, YVEX_ERR_FORMAT,
                       "yvex_tensor_embedding_slice_range_validate",
                       "selected embedding slice requires a valid rank-2 tensor range");
        return YVEX_ERR_FORMAT;
    }
    if (range->dtype != YVEX_DTYPE_F16) {
        yvex_error_setf(err, YVEX_ERR_UNSUPPORTED,
                        "yvex_tensor_embedding_slice_range_validate",
                        "selected-embedding-dtype-invalid: %s requires F16",
                        range->tensor_name);
        return YVEX_ERR_UNSUPPORTED;
    }
    hidden_size = range->dims[0];
    vocab_size = range->dims[1];
    if (hidden_size == 0ull || vocab_size == 0ull) {
        yvex_error_set(err, YVEX_ERR_FORMAT,
                       "yvex_tensor_embedding_slice_range_validate",
                       "selected embedding dimensions must be non-zero");
        return YVEX_ERR_FORMAT;
    }
    if ((unsigned long long)token_id >= vocab_size) {
        yvex_error_setf(err, YVEX_ERR_BOUNDS,
                        "yvex_tensor_embedding_slice_range_validate",
                        "token-out-of-range: %u >= %llu", token_id, vocab_size);
        return YVEX_ERR_BOUNDS;
    }
    if (range->dtype_size == 0ull) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED,
                       "yvex_tensor_embedding_slice_range_validate",
                       "tensor-dtype-size-unknown");
        return YVEX_ERR_UNSUPPORTED;
    }
    if (!checked_mul_ull(hidden_size, range->dtype_size, &slice_bytes)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS,
                       "yvex_tensor_embedding_slice_range_validate",
                       "token-slice-byte-count-overflow");
        return YVEX_ERR_BOUNDS;
    }
    if (!checked_mul_ull((unsigned long long)token_id, slice_bytes,
                         &token_offset_bytes)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS,
                       "yvex_tensor_embedding_slice_range_validate",
                       "token-slice-offset-overflow");
        return YVEX_ERR_BOUNDS;
    }
    if (!checked_add_ull(range->tensor_absolute_offset, token_offset_bytes,
                         &slice_absolute_offset)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS,
                       "yvex_tensor_embedding_slice_range_validate",
                       "token-slice-offset-overflow");
        return YVEX_ERR_BOUNDS;
    }
    if (!checked_add_ull(slice_absolute_offset, slice_bytes, &slice_end_offset)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS,
                       "yvex_tensor_embedding_slice_range_validate",
                       "token-slice-end-overflow");
        return YVEX_ERR_BOUNDS;
    }
    if (slice_absolute_offset < range->tensor_absolute_offset ||
        slice_end_offset > range->tensor_end_offset) {
        yvex_error_set(err, YVEX_ERR_BOUNDS,
                       "yvex_tensor_embedding_slice_range_validate",
                       "token-slice-range-out-of-tensor");
        return YVEX_ERR_BOUNDS;
    }

    slice->slice_bytes = slice_bytes;
    slice->slice_relative_offset = token_offset_bytes;
    slice->slice_absolute_offset = slice_absolute_offset;
    slice->slice_end_offset = slice_end_offset;
    slice->range_valid = 1;
    yvex_error_clear(err);
    return YVEX_OK;
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

static const char *range_error_code(const yvex_error *err)
{
    const char *message = yvex_error_message(err);

    if (!message) return "tensor-range-invalid";
    if (strstr(message, "tensor-rank-invalid")) return "tensor-rank-invalid";
    if (strstr(message, "tensor-dim-zero")) return "tensor-dim-zero";
    if (strstr(message, "tensor-element-count-overflow")) return "tensor-element-count-overflow";
    if (strstr(message, "tensor-dtype-unknown")) return "tensor-dtype-unknown";
    if (strstr(message, "tensor-dtype-size-unknown")) return "tensor-dtype-size-unknown";
    if (strstr(message, "tensor-byte-count-overflow")) return "tensor-byte-count-overflow";
    if (strstr(message, "selected-embedding-rank-invalid")) return "selected-embedding-rank-invalid";
    if (strstr(message, "selected-embedding-dtype-invalid")) return "selected-embedding-dtype-invalid";
    if (strstr(message, "selected-embedding-output-byte-count-overflow")) return "selected-embedding-output-byte-count-overflow";
    if (strstr(message, "selected-embedding-slice-byte-count-overflow")) return "selected-embedding-slice-byte-count-overflow";
    if (strstr(message, "tensor-relative-offset-overflow")) return "tensor-relative-offset-overflow";
    if (strstr(message, "tensor-absolute-offset-overflow")) return "tensor-absolute-offset-overflow";
    if (strstr(message, "tensor-offset-before-data")) return "tensor-offset-before-data";
    if (strstr(message, "tensor-offset-out-of-file")) return "tensor-offset-out-of-file";
    if (strstr(message, "tensor-end-offset-overflow")) return "tensor-end-offset-overflow";
    if (strstr(message, "tensor-range-out-of-file")) return "tensor-range-out-of-file";
    if (strstr(message, "tensor-alignment-invalid")) return "tensor-alignment-invalid";
    if (strstr(message, "token-out-of-range")) return "token-out-of-range";
    if (strstr(message, "token-slice-offset-overflow")) return "token-slice-offset-overflow";
    if (strstr(message, "token-slice-end-overflow")) return "token-slice-end-overflow";
    if (strstr(message, "token-slice-range-out-of-tensor")) return "token-slice-range-out-of-tensor";
    if (strstr(message, "tensor name is empty")) return "empty-tensor-name";
    return "tensor-range-invalid";
}

static void map_parse_result_to_report(const yvex_gguf_parse_result *result,
                                       const yvex_error *source,
                                       yvex_artifact_integrity_report *report)
{
    const char *message = yvex_error_message(source);
    const char *code = "tensor-directory-parse-failed";

    if (!result) {
        code = yvex_error_code(source) == YVEX_ERR_IO
                   ? "file-open-failed" : code;
    } else {
        switch (result->code) {
        case YVEX_GGUF_PARSE_SHORT_READ:
            code = result->section == YVEX_GGUF_PARSE_SECTION_CONTAINER
                       ? "file-too-small" :
                   result->section == YVEX_GGUF_PARSE_SECTION_METADATA
                       ? "metadata-parse-failed" : "tensor-directory-parse-failed";
            break;
        case YVEX_GGUF_PARSE_INVALID_MAGIC:
            code = "bad-magic";
            break;
        case YVEX_GGUF_PARSE_UNSUPPORTED_VERSION:
            code = "unsupported-version";
            break;
        case YVEX_GGUF_PARSE_MALFORMED_KEY:
        case YVEX_GGUF_PARSE_EMPTY_METADATA_KEY:
        case YVEX_GGUF_PARSE_DUPLICATE_METADATA_KEY:
        case YVEX_GGUF_PARSE_UNSUPPORTED_METADATA_TYPE:
        case YVEX_GGUF_PARSE_MALFORMED_VALUE:
        case YVEX_GGUF_PARSE_MALFORMED_ARRAY:
            code = "metadata-parse-failed";
            break;
        case YVEX_GGUF_PARSE_MALFORMED_STRING:
            code = "malformed-string";
            break;
        case YVEX_GGUF_PARSE_INVALID_RANK:
            code = "rank-out-of-range";
            break;
        case YVEX_GGUF_PARSE_INVALID_DIMENSION:
            code = "zero-dimension";
            break;
        case YVEX_GGUF_PARSE_INVALID_ALIGNMENT:
            code = "tensor-alignment-invalid";
            break;
        case YVEX_GGUF_PARSE_MALFORMED_TENSOR_NAME:
            code = "tensor-directory-parse-failed";
            break;
        case YVEX_GGUF_PARSE_EMPTY_TENSOR_NAME:
            code = "empty-tensor-name";
            break;
        case YVEX_GGUF_PARSE_DUPLICATE_TENSOR_NAME:
            code = "duplicate-tensor-name";
            break;
        case YVEX_GGUF_PARSE_OFFSET_OVERFLOW:
            code = "tensor-absolute-offset-overflow";
            break;
        case YVEX_GGUF_PARSE_ELEMENT_COUNT_OVERFLOW:
        case YVEX_GGUF_PARSE_ROW_COUNT_OVERFLOW:
            code = "element-count-overflow";
            break;
        case YVEX_GGUF_PARSE_ROW_BYTE_OVERFLOW:
            code = "row-byte-overflow";
            break;
        case YVEX_GGUF_PARSE_TOTAL_BYTE_OVERFLOW:
            code = "tensor-byte-count-overflow";
            break;
        case YVEX_GGUF_PARSE_INCOMPLETE_DIRECTORY:
            code = "tensor-range-out-of-file";
            break;
        case YVEX_GGUF_PARSE_REFUSED_QTYPE:
            code = "unknown-dtype";
            break;
        case YVEX_GGUF_PARSE_RESOURCE_LIMIT:
        case YVEX_GGUF_PARSE_INVALID_COUNT:
        case YVEX_GGUF_PARSE_ALLOCATION_FAILURE:
            code = "parser-resource-refused";
            break;
        default:
            break;
        }
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

    if (!artifact || !gguf || !tensors) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_artifact_integrity_validate",
                       "artifact, gguf, and tensors are required");
        return YVEX_ERR_INVALID_ARG;
    }

    set_basic_report(artifact, gguf, tensors, report);

    for (i = 0; i < yvex_tensor_table_count(tensors); ++i) {
        const yvex_tensor_info *tensor = yvex_tensor_table_at(tensors, i);
        yvex_tensor_shape_accounting accounting;
        yvex_tensor_range range;
        int rc;

        if (!tensor) {
            (void)add_error(report, "tensor-directory-parse-failed", "",
                            "tensor table row is unavailable");
            continue;
        }
        if (!tensor->name || tensor->name[0] == '\0') {
            (void)add_error(report, "empty-tensor-name", "", "tensor name is empty");
        }
        memset(&accounting, 0, sizeof(accounting));
        report->tensor_shapes_checked += 1ull;
        report->tensor_dtypes_checked += 1ull;
        report->tensor_byte_counts_checked += 1ull;
        rc = yvex_tensor_shape_accounting_validate(tensor, &accounting, err);
        if (rc == YVEX_OK) {
            report->tensor_shapes_valid += 1ull;
            report->tensor_dtypes_valid += 1ull;
            if (!checked_add_ull(report->known_tensor_bytes,
                                 accounting.storage_byte_count,
                                 &report->known_tensor_bytes)) {
                (void)add_error(report, "tensor-byte-count-overflow", tensor->name,
                                "known tensor byte sum overflows");
            }
        } else {
            const char *code = range_error_code(err);
            const char *public_code = code;
            const char *reason = yvex_error_message(err);

            if (strcmp(code, "tensor-rank-invalid") == 0) {
                report->tensor_shapes_invalid += 1ull;
                public_code = "rank-out-of-range";
            } else if (strcmp(code, "tensor-dim-zero") == 0) {
                report->tensor_shapes_invalid += 1ull;
                public_code = "zero-dimension";
            } else if (strcmp(code, "tensor-element-count-overflow") == 0) {
                report->tensor_shapes_invalid += 1ull;
                public_code = "element-count-overflow";
            } else if (strcmp(code, "tensor-dtype-unknown") == 0) {
                report->tensor_shapes_valid += 1ull;
                report->tensor_dtypes_invalid += 1ull;
                public_code = "unknown-dtype";
            } else if (strcmp(code, "tensor-dtype-size-unknown") == 0) {
                report->tensor_shapes_valid += 1ull;
                report->tensor_dtypes_invalid += 1ull;
                public_code = "dtype-size-unknown";
            } else if (strcmp(code, "tensor-byte-count-overflow") == 0) {
                report->tensor_shapes_valid += 1ull;
                report->tensor_dtypes_valid += 1ull;
                report->tensor_byte_counts_invalid += 1ull;
            } else {
                report->tensor_shapes_invalid += 1ull;
            }
            (void)add_error(report, public_code, tensor->name, reason);
            yvex_error_clear(err);
            continue;
        }
        memset(&range, 0, sizeof(range));
        report->tensor_ranges_checked += 1ull;
        rc = yvex_tensor_range_validate(artifact, gguf, tensor, &range, err);
        if (rc == YVEX_OK) {
            report->tensor_ranges_valid += 1ull;
        } else {
            const char *code = range_error_code(err);
            const char *public_code = code;
            const char *reason = yvex_error_message(err);

            report->tensor_ranges_invalid += 1ull;
            if (strcmp(code, "tensor-dim-zero") == 0) {
                public_code = "zero-dimension";
            } else if (strcmp(code, "tensor-element-count-overflow") == 0) {
                public_code = "element-count-overflow";
            } else if (strcmp(code, "tensor-dtype-unknown") == 0) {
                public_code = "unknown-dtype";
            } else if (strcmp(code, "tensor-dtype-size-unknown") == 0) {
                public_code = "dtype-size-unknown";
            } else if (strcmp(code, "tensor-rank-invalid") == 0) {
                public_code = "rank-out-of-range";
            } else if (strcmp(code, "tensor-offset-out-of-file") == 0) {
                public_code = "tensor-range-out-of-file";
            }
            (void)add_range_error(report, public_code, tensor->name, reason, &range);
            yvex_error_clear(err);
        }
    }

    if (options && options->require_token_embedding) {
        const yvex_tensor_info *tensor = yvex_tensor_table_find(tensors, "token_embd.weight");

        if (!tensor) {
            (void)add_error(report, "required-tensor-missing", "token_embd.weight",
                            "required tensor not found: token_embd.weight");
        } else {
            yvex_selected_embedding_shape selected_shape;
            int shape_rc;

            memset(&selected_shape, 0, sizeof(selected_shape));
            shape_rc = yvex_selected_embedding_shape_validate(tensor,
                                                              options->token_id,
                                                              &selected_shape,
                                                              err);
            if (shape_rc == YVEX_OK) {
                yvex_tensor_range range;
                yvex_tensor_slice_range slice;
                int rc;

                integrity_copy(report->selected_embedding_shape,
                               YVEX_INTEGRITY_DIGEST_STATUS_CAP,
                               "valid");
                report->selected_embedding_hidden_size = selected_shape.hidden_size;
                report->selected_embedding_vocab_size = selected_shape.vocab_size;
                report->selected_embedding_output_count = selected_shape.output_count;
                report->selected_embedding_output_bytes = selected_shape.output_bytes;
                report->selected_embedding_slice_bytes = selected_shape.slice_bytes;

                memset(&range, 0, sizeof(range));
                memset(&slice, 0, sizeof(slice));
                rc = yvex_tensor_range_validate(artifact, gguf, tensor, &range, err);
                if (rc == YVEX_OK) {
                    rc = yvex_tensor_embedding_slice_range_validate(&range,
                                                                    options->token_id,
                                                                    &slice,
                                                                    err);
                }
                if (rc != YVEX_OK) {
                    const char *code = range_error_code(err);
                    if (strcmp(code, "token-out-of-range") == 0) {
                        (void)add_range_error(report, "token-out-of-range", tensor->name,
                                              "partial token id is outside embedding range",
                                              &range);
                    } else {
                        (void)add_range_error(report, code, tensor->name,
                                              yvex_error_message(err), &range);
                    }
                    yvex_error_clear(err);
                }
            } else {
                const char *code = range_error_code(err);
                const char *public_code = code;
                if (strcmp(code, "selected-embedding-dtype-invalid") == 0) {
                    public_code = "required-tensor-dtype-invalid";
                } else if (strcmp(code, "selected-embedding-rank-invalid") == 0) {
                    public_code = "required-tensor-rank-invalid";
                } else if (strcmp(code, "token-out-of-range") == 0) {
                    public_code = "token-out-of-range";
                } else if (strcmp(code, "tensor-dim-zero") == 0) {
                    public_code = "zero-dimension";
                } else if (strcmp(code, "tensor-element-count-overflow") == 0) {
                    public_code = "element-count-overflow";
                } else if (strcmp(code, "tensor-dtype-unknown") == 0) {
                    public_code = "unknown-dtype";
                }
                (void)add_error(report, public_code, tensor->name,
                                yvex_error_message(err));
                yvex_error_clear(err);
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
    yvex_gguf_parse_result parse_result;
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
    rc = yvex_artifact_open(&artifact, &artifact_options, err);
    if (rc != YVEX_OK) {
        map_parse_result_to_report(NULL, err, report);
        apply_identity_digest(&identity, options, report);
        set_integrity_error(err, report);
        return rc;
    }
    report->file_size = yvex_artifact_size(artifact);

    rc = yvex_gguf_open_ex(&gguf, artifact, NULL, &parse_result, err);
    if (rc == YVEX_OK) {
        rc = yvex_tensor_table_from_gguf(&tensors, gguf, err);
    }
    if (rc != YVEX_OK) {
        if (gguf) {
            set_basic_report(artifact, gguf, tensors, report);
        }
        map_parse_result_to_report(&parse_result, err, report);
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
