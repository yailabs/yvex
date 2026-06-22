/*
 * YVEX - Calibration / imatrix manifest API
 *
 * File: include/yvex/imatrix.h
 * Layer: public tool-plane API
 *
 * Purpose:
 *   Declares the OWI.6 imatrix provenance and compatibility surface. Imatrix
 *   manifests link external calibration artifacts to source manifests and
 *   quantization policies. They do not generate calibration data, quantize,
 *   emit GGUF, materialize weights, or participate in runtime inference.
 */
#ifndef YVEX_IMATRIX_H
#define YVEX_IMATRIX_H

#include <yvex/error.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    YVEX_IMATRIX_STATUS_UNKNOWN = 0,
    YVEX_IMATRIX_STATUS_DECLARED,
    YVEX_IMATRIX_STATUS_PRESENT,
    YVEX_IMATRIX_STATUS_MISSING,
    YVEX_IMATRIX_STATUS_INVALID,
    YVEX_IMATRIX_STATUS_UNSUPPORTED_FORMAT
} yvex_imatrix_status;

typedef enum {
    YVEX_IMATRIX_FORMAT_UNKNOWN = 0,
    YVEX_IMATRIX_FORMAT_LLAMA_CPP_DAT,
    YVEX_IMATRIX_FORMAT_DS4_ROUTED_MOE_DAT,
    YVEX_IMATRIX_FORMAT_JSON_MANIFEST,
    YVEX_IMATRIX_FORMAT_OTHER
} yvex_imatrix_format;

typedef enum {
    YVEX_IMATRIX_COVERAGE_UNKNOWN = 0,
    YVEX_IMATRIX_COVERAGE_GLOBAL,
    YVEX_IMATRIX_COVERAGE_TENSOR_PATTERN,
    YVEX_IMATRIX_COVERAGE_TENSOR_ROLE,
    YVEX_IMATRIX_COVERAGE_LAYER_RANGE,
    YVEX_IMATRIX_COVERAGE_EXPERT_GROUP,
    YVEX_IMATRIX_COVERAGE_ROUTED_MOE
} yvex_imatrix_coverage_kind;

typedef enum {
    YVEX_IMATRIX_ISSUE_NONE = 0,
    YVEX_IMATRIX_ISSUE_FILE_MISSING,
    YVEX_IMATRIX_ISSUE_FORMAT_UNSUPPORTED,
    YVEX_IMATRIX_ISSUE_POLICY_REQUIRES_IMATRIX,
    YVEX_IMATRIX_ISSUE_POLICY_RULE_UNCOVERED,
    YVEX_IMATRIX_ISSUE_SOURCE_MISMATCH,
    YVEX_IMATRIX_ISSUE_FORMAT,
    YVEX_IMATRIX_ISSUE_IO
} yvex_imatrix_issue_kind;

typedef struct {
    const char *name;
    const char *architecture;
    const char *source_manifest_path;
    const char *quant_policy_path;
    const char *imatrix_path;
    const char *calibration_dataset;
    const char *calibration_command;
    const char *producer;
    yvex_imatrix_format format;
    yvex_imatrix_status status;
} yvex_imatrix_manifest_options;

typedef struct {
    yvex_imatrix_status status;
    yvex_imatrix_format format;
    const char *name;
    const char *architecture;
    const char *imatrix_path;
    const char *source_manifest_path;
    const char *quant_policy_path;
    unsigned long long issue_count;
    unsigned long long covered_rule_count;
    unsigned long long uncovered_rule_count;
    unsigned long long requires_imatrix_rule_count;
    int file_exists;
} yvex_imatrix_summary;

typedef struct yvex_imatrix_manifest yvex_imatrix_manifest;

int yvex_imatrix_manifest_create(yvex_imatrix_manifest **out,
                                 const yvex_imatrix_manifest_options *options,
                                 yvex_error *err);

int yvex_imatrix_manifest_open(yvex_imatrix_manifest **out,
                               const char *path,
                               yvex_error *err);

void yvex_imatrix_manifest_close(yvex_imatrix_manifest *manifest);

int yvex_imatrix_manifest_write_json(const char *out_path,
                                     const yvex_imatrix_manifest *manifest,
                                     yvex_error *err);

int yvex_imatrix_manifest_validate(const yvex_imatrix_manifest *manifest,
                                   yvex_error *err);

int yvex_imatrix_manifest_get_summary(const yvex_imatrix_manifest *manifest,
                                      yvex_imatrix_summary *out,
                                      yvex_error *err);

const char *yvex_imatrix_status_name(yvex_imatrix_status status);
const char *yvex_imatrix_format_name(yvex_imatrix_format format);
const char *yvex_imatrix_coverage_kind_name(yvex_imatrix_coverage_kind kind);
const char *yvex_imatrix_issue_kind_name(yvex_imatrix_issue_kind issue);

yvex_imatrix_status yvex_imatrix_status_from_name(const char *name);
yvex_imatrix_format yvex_imatrix_format_from_name(const char *name);
yvex_imatrix_coverage_kind yvex_imatrix_coverage_kind_from_name(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_IMATRIX_H */
