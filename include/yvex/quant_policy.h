/*
 * YVEX - Quantization policy manifest API
 *
 * File: include/yvex/quant_policy.h
 * Layer: public tool-plane API
 *
 * Purpose:
 *   Declares the OWI.5 declarative quantization policy surface. Policies
 *   describe intended storage qtypes for tensor roles/names/patterns; they do
 *   not quantize, emit GGUF, materialize, or imply execution support.
 */
#ifndef YVEX_QUANT_POLICY_H
#define YVEX_QUANT_POLICY_H

#include <yvex/error.h>
#include <yvex/tensor.h>

#ifdef __cplusplus
extern "C" {
#endif

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

#endif /* YVEX_QUANT_POLICY_H */
