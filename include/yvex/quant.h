/* Owner: public quant ABI.
 * Owns: imatrix manifests, quantization jobs, and role-based policy.
 * Does not own: numeric execution, GGUF writing, or artifact publication.
 * Invariants: declarations are format-stable, externally consumable, and independently includable.
 * Boundary: quantization inputs and policy contracts.
 * Purpose: Expose quantization inputs and policy contracts.
 * Inputs: Typed caller-owned values and immutable borrowed views as declared below.
 * Effects: Only functions with explicit lifecycle or I/O contracts mutate external state.
 * Failure: Typed status and error outputs remain authoritative; declarations add no capability. */
#ifndef YVEX_QUANT_H
#define YVEX_QUANT_H

#include <yvex/core.h>
#include <yvex/model.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    YVEX_QUANT_PROFILE_SOURCE_FAITHFUL = 0,
    YVEX_QUANT_PROFILE_RELEASE_Q8_Q2
} yvex_quant_profile_kind;

/* Calibration manifests. */
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
    YVEX_IMATRIX_FORMAT_ROUTED_MOE_DAT,
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

/* Quantization jobs. */
typedef enum {
    YVEX_QUANT_JOB_STATUS_UNKNOWN = 0,
    YVEX_QUANT_JOB_STATUS_DECLARED,
    YVEX_QUANT_JOB_STATUS_READY,
    YVEX_QUANT_JOB_STATUS_RUNNING,
    YVEX_QUANT_JOB_STATUS_SUCCEEDED,
    YVEX_QUANT_JOB_STATUS_FAILED,
    YVEX_QUANT_JOB_STATUS_SKIPPED
} yvex_quant_job_status;

typedef enum {
    YVEX_QUANT_JOB_TOOL_UNKNOWN = 0,
    YVEX_QUANT_JOB_TOOL_YVEX_INTERNAL,
    YVEX_QUANT_JOB_TOOL_EXTERNAL
} yvex_quant_job_tool;

typedef struct {
    const char *name;
    const char *architecture;
    const char *tool_path;
    const char *source_manifest_path;
    const char *native_source_dir;
    const char *template_path;
    const char *quant_policy_path;
    const char *imatrix_manifest_path;
    const char *imatrix_path;
    const char *out_gguf_path;
    const char *log_path;
    const char *command;
    yvex_quant_job_tool tool;
    yvex_quant_job_status status;
} yvex_quant_job_options;

typedef struct {
    yvex_quant_job_status status;
    yvex_quant_job_tool tool;
    const char *name;
    const char *architecture;
    const char *tool_path;
    const char *native_source_dir;
    const char *template_path;
    const char *out_gguf_path;
    const char *log_path;
    int tool_exists;
    int source_exists;
    int template_exists;
    int imatrix_exists;
    int output_exists;
} yvex_quant_job_summary;

int yvex_quant_job_write_json(const char *out_path,
                              const yvex_quant_job_options *options,
                              yvex_quant_job_summary *summary_out,
                              yvex_error *err);

int yvex_quant_job_validate(const char *manifest_path,
                            yvex_quant_job_summary *summary_out,
                            yvex_error *err);

const char *yvex_quant_job_status_name(yvex_quant_job_status status);
const char *yvex_quant_job_tool_name(yvex_quant_job_tool tool);

yvex_quant_job_status yvex_quant_job_status_from_name(const char *name);
yvex_quant_job_tool yvex_quant_job_tool_from_name(const char *name);

/* Quantization policy. */
typedef enum {
    YVEX_QUANT_QTYPE_UNKNOWN = 0,
    YVEX_QUANT_QTYPE_F32,
    YVEX_QUANT_QTYPE_F16,
    YVEX_QUANT_QTYPE_BF16,
    YVEX_QUANT_QTYPE_Q8_0,
    YVEX_QUANT_QTYPE_Q4_0,
    YVEX_QUANT_QTYPE_Q4_K,
    YVEX_QUANT_QTYPE_Q5_K,
    YVEX_QUANT_QTYPE_Q6_K,
    YVEX_QUANT_QTYPE_Q2_K,
    YVEX_QUANT_QTYPE_IQ2_XXS,
    YVEX_QUANT_QTYPE_IQ2_XS,
    YVEX_QUANT_QTYPE_IQ3_XXS,
    YVEX_QUANT_QTYPE_IQ4_NL,
    YVEX_QUANT_QTYPE_OTHER
} yvex_quant_qtype;

typedef enum {
    YVEX_QUANT_SELECTOR_UNKNOWN = 0,
    YVEX_QUANT_SELECTOR_ROLE,
    YVEX_QUANT_SELECTOR_TENSOR_NAME,
    YVEX_QUANT_SELECTOR_TENSOR_PATTERN,
    YVEX_QUANT_SELECTOR_LAYER_RANGE,
    YVEX_QUANT_SELECTOR_EXPERT_GROUP,
    YVEX_QUANT_SELECTOR_DEFAULT
} yvex_quant_selector_kind;

typedef enum {
    YVEX_QUANT_POLICY_STATUS_UNKNOWN = 0,
    YVEX_QUANT_POLICY_STATUS_VALID,
    YVEX_QUANT_POLICY_STATUS_PARTIAL,
    YVEX_QUANT_POLICY_STATUS_INVALID
} yvex_quant_policy_status;

typedef enum {
    YVEX_QUANT_POLICY_ISSUE_NONE = 0,
    YVEX_QUANT_POLICY_ISSUE_UNKNOWN_QTYPE,
    YVEX_QUANT_POLICY_ISSUE_UNSUPPORTED_STORAGE_QTYPE,
    YVEX_QUANT_POLICY_ISSUE_UNSUPPORTED_COMPUTE_QTYPE,
    YVEX_QUANT_POLICY_ISSUE_UNKNOWN_ROLE,
    YVEX_QUANT_POLICY_ISSUE_UNMATCHED_SELECTOR,
    YVEX_QUANT_POLICY_ISSUE_TEMPLATE_QTYPE_MISMATCH,
    YVEX_QUANT_POLICY_ISSUE_REQUIRES_IMATRIX,
    YVEX_QUANT_POLICY_ISSUE_FORMAT
} yvex_quant_policy_issue_kind;

typedef struct {
    yvex_quant_selector_kind selector_kind;
    const char *selector;
    yvex_tensor_role role;
    yvex_quant_qtype qtype;
    int requires_imatrix;
    int storage_supported;
    int compute_supported;
} yvex_quant_policy_rule;

typedef struct {
    yvex_quant_policy_status status;
    const char *architecture;
    const char *name;
    unsigned long long rule_count;
    unsigned long long issue_count;
    unsigned long long requires_imatrix_count;
    unsigned long long storage_supported_count;
    unsigned long long compute_supported_count;
} yvex_quant_policy_summary;

typedef struct yvex_quant_policy yvex_quant_policy;

int yvex_quant_policy_open(yvex_quant_policy **out,
                           const char *path,
                           yvex_error *err);

void yvex_quant_policy_close(yvex_quant_policy *policy);

int yvex_quant_policy_write_json(const char *out_path,
                                 const yvex_quant_policy *policy,
                                 yvex_error *err);

int yvex_quant_policy_create_from_template(yvex_quant_policy **out,
                                           const char *template_path,
                                           const char *architecture,
                                           yvex_error *err);

int yvex_quant_policy_validate(yvex_quant_policy *policy,
                               const char *template_path,
                               yvex_error *err);

int yvex_quant_policy_get_summary(const yvex_quant_policy *policy,
                                  yvex_quant_policy_summary *out,
                                  yvex_error *err);

unsigned long long yvex_quant_policy_rule_count(const yvex_quant_policy *policy);

const yvex_quant_policy_rule *yvex_quant_policy_rule_at(const yvex_quant_policy *policy,
                                                        unsigned long long index);

const char *yvex_quant_qtype_name(yvex_quant_qtype qtype);
const char *yvex_quant_selector_kind_name(yvex_quant_selector_kind kind);
const char *yvex_quant_policy_status_name(yvex_quant_policy_status status);
const char *yvex_quant_policy_issue_kind_name(yvex_quant_policy_issue_kind issue);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_QUANT_H */
