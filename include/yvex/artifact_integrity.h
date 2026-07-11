/*
 * YVEX - Artifact integrity baseline
 *
 * File: include/yvex/artifact_integrity.h
 * Layer: public artifact API
 *
 * Purpose:
 *   Defines artifact integrity facts projected from the canonical global GGUF
 *   layout result, optional explicit digest policy, and subordinate selected
 *   tensor proofs before payload consumers proceed.
 *
 * Does not own:
 *   - supply-chain security
 *   - implicit digest enforcement
 *   - registry drift detection
 *   - malware detection
 *   - sandboxing
 *   - model completeness or runtime support
 */
#ifndef YVEX_ARTIFACT_INTEGRITY_H
#define YVEX_ARTIFACT_INTEGRITY_H

#include <yvex/artifact.h>
#include <yvex/error.h>
#include <yvex/gguf.h>
#include <yvex/gguf_layout.h>
#include <yvex/tensor.h>

#ifdef __cplusplus
extern "C" {
#endif

#define YVEX_INTEGRITY_CODE_CAP 64u
#define YVEX_INTEGRITY_TENSOR_CAP 128u
#define YVEX_INTEGRITY_REASON_CAP 256u
#define YVEX_INTEGRITY_FORMAT_CAP 16u
#define YVEX_INTEGRITY_ARCH_CAP 64u
#define YVEX_INTEGRITY_MAX_ISSUES 32u
#define YVEX_INTEGRITY_SHA256_CAP 65u
#define YVEX_INTEGRITY_DIGEST_STATUS_CAP 24u

typedef enum {
    YVEX_INTEGRITY_SEVERITY_ERROR = 0,
    YVEX_INTEGRITY_SEVERITY_WARNING = 1
} yvex_integrity_severity;

typedef struct {
    yvex_integrity_severity severity;
    char code[YVEX_INTEGRITY_CODE_CAP];
    char tensor[YVEX_INTEGRITY_TENSOR_CAP];
    char reason[YVEX_INTEGRITY_REASON_CAP];
    unsigned long long relative_offset;
    unsigned long long absolute_offset;
    unsigned long long tensor_bytes;
    unsigned long long file_size;
    int has_range;
} yvex_integrity_issue;

typedef struct {
    char tensor_name[YVEX_INTEGRITY_TENSOR_CAP];
    yvex_dtype dtype;
    unsigned int rank;
    unsigned long long dims[YVEX_TENSOR_MAX_DIMS];
    unsigned long long element_count;
    unsigned long long storage_unit_bytes;
    unsigned long long storage_byte_count;
    int dtype_known;
    int byte_count_computable;
    int storage_supported;
    int compute_supported_for_selected_embedding;
    int compute_supported_for_fixture_embedding;
    int shape_valid;
    int dtype_valid;
    int byte_count_valid;
} yvex_tensor_shape_accounting;

typedef struct {
    char tensor_name[YVEX_INTEGRITY_TENSOR_CAP];
    yvex_dtype dtype;
    unsigned int rank;
    unsigned long long dims[YVEX_TENSOR_MAX_DIMS];
    unsigned long long element_count;
    unsigned long long dtype_size;
    unsigned long long tensor_bytes;
    unsigned long long tensor_relative_offset;
    unsigned long long tensor_data_offset;
    unsigned long long tensor_absolute_offset;
    unsigned long long tensor_end_offset;
    unsigned long long file_size;
    unsigned long long alignment;
    int aligned;
    int range_valid;
} yvex_tensor_range;

typedef struct {
    unsigned int token_id;
    unsigned long long slice_bytes;
    unsigned long long slice_relative_offset;
    unsigned long long slice_absolute_offset;
    unsigned long long slice_end_offset;
    int range_valid;
} yvex_tensor_slice_range;

typedef struct {
    char tensor_name[YVEX_INTEGRITY_TENSOR_CAP];
    unsigned int token_id;
    unsigned long long hidden_size;
    unsigned long long vocab_size;
    unsigned long long output_count;
    unsigned long long output_bytes;
    unsigned long long slice_bytes;
    int shape_valid;
} yvex_selected_embedding_shape;

typedef struct {
    int require_token_embedding;
    unsigned int token_id;
    const char *expect_sha256;
    const char *registered_sha256;
} yvex_artifact_integrity_options;

typedef struct {
    int checked;
    int passed;
    char path[YVEX_ARTIFACT_PATH_CAP];
    char format[YVEX_INTEGRITY_FORMAT_CAP];
    unsigned long long file_size;
    unsigned int version;
    char architecture[YVEX_INTEGRITY_ARCH_CAP];
    int identity_checked;
    char sha256[YVEX_INTEGRITY_SHA256_CAP];
    char registered_sha256[YVEX_INTEGRITY_SHA256_CAP];
    char expected_sha256[YVEX_INTEGRITY_SHA256_CAP];
    char digest_status[YVEX_INTEGRITY_DIGEST_STATUS_CAP];
    unsigned long long tensor_count;
    int layout_checked;
    yvex_gguf_layout_result layout;
    unsigned long long known_tensor_bytes;
    unsigned long long tensor_ranges_checked;
    unsigned long long tensor_ranges_valid;
    unsigned long long tensor_ranges_invalid;
    unsigned long long tensor_shapes_checked;
    unsigned long long tensor_shapes_valid;
    unsigned long long tensor_shapes_invalid;
    unsigned long long tensor_dtypes_checked;
    unsigned long long tensor_dtypes_valid;
    unsigned long long tensor_dtypes_invalid;
    unsigned long long tensor_byte_counts_checked;
    unsigned long long tensor_byte_counts_invalid;
    char selected_embedding_shape[YVEX_INTEGRITY_DIGEST_STATUS_CAP];
    unsigned long long selected_embedding_hidden_size;
    unsigned long long selected_embedding_vocab_size;
    unsigned long long selected_embedding_output_count;
    unsigned long long selected_embedding_output_bytes;
    unsigned long long selected_embedding_slice_bytes;
    unsigned int error_count;
    unsigned int warning_count;
    unsigned int issue_count;
    yvex_integrity_issue issues[YVEX_INTEGRITY_MAX_ISSUES];
} yvex_artifact_integrity_report;

int yvex_artifact_integrity_check_path(const char *path,
                                       const yvex_artifact_integrity_options *options,
                                       yvex_artifact_integrity_report *out,
                                       yvex_error *err);

int yvex_artifact_integrity_validate(const yvex_artifact *artifact,
                                     const yvex_gguf *gguf,
                                     const yvex_tensor_table *tensors,
                                     const yvex_artifact_integrity_options *options,
                                     yvex_artifact_integrity_report *out,
                                     yvex_error *err);

int yvex_tensor_range_validate(const yvex_artifact *artifact,
                               const yvex_gguf *gguf,
                               const yvex_tensor_info *tensor,
                               yvex_tensor_range *out,
                               yvex_error *err);

int yvex_tensor_shape_accounting_validate(
    const yvex_tensor_info *tensor,
    yvex_tensor_shape_accounting *out,
    yvex_error *err);

int yvex_selected_embedding_shape_validate(
    const yvex_tensor_info *tensor,
    unsigned int token_id,
    yvex_selected_embedding_shape *out,
    yvex_error *err);

int yvex_tensor_embedding_slice_range_validate(
    const yvex_tensor_range *range,
    unsigned int token_id,
    yvex_tensor_slice_range *out,
    yvex_error *err);

const yvex_integrity_issue *yvex_artifact_integrity_issue_at(
    const yvex_artifact_integrity_report *report,
    unsigned int index);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_ARTIFACT_INTEGRITY_H */
