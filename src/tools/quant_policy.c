/*
 * YVEX - Quantization policy manifest implementation
 *
 * File: src/tools/quant_policy.c
 * Layer: tool-plane implementation
 */
#include "quant_policy_internal.h"

#include <stdlib.h>
#include <string.h>

char *yvex_quant_policy_strdup(const char *s)
{
    size_t len;
    char *out;

    if (!s) s = "";
    len = strlen(s);
    out = (char *)malloc(len + 1u);
    if (!out) return NULL;
    memcpy(out, s, len + 1u);
    return out;
}

const char *yvex_quant_qtype_name(yvex_quant_qtype qtype)
{
    switch (qtype) {
    case YVEX_QUANT_QTYPE_UNKNOWN: return "UNKNOWN";
    case YVEX_QUANT_QTYPE_F32: return "F32";
    case YVEX_QUANT_QTYPE_F16: return "F16";
    case YVEX_QUANT_QTYPE_BF16: return "BF16";
    case YVEX_QUANT_QTYPE_Q8_0: return "Q8_0";
    case YVEX_QUANT_QTYPE_Q4_0: return "Q4_0";
    case YVEX_QUANT_QTYPE_Q4_K: return "Q4_K";
    case YVEX_QUANT_QTYPE_Q5_K: return "Q5_K";
    case YVEX_QUANT_QTYPE_Q6_K: return "Q6_K";
    case YVEX_QUANT_QTYPE_Q2_K: return "Q2_K";
    case YVEX_QUANT_QTYPE_IQ2_XXS: return "IQ2_XXS";
    case YVEX_QUANT_QTYPE_IQ2_XS: return "IQ2_XS";
    case YVEX_QUANT_QTYPE_IQ3_XXS: return "IQ3_XXS";
    case YVEX_QUANT_QTYPE_IQ4_NL: return "IQ4_NL";
    case YVEX_QUANT_QTYPE_OTHER: return "OTHER";
    }
    return "UNKNOWN";
}

yvex_quant_qtype yvex_quant_qtype_from_name(const char *name)
{
    if (!name) return YVEX_QUANT_QTYPE_UNKNOWN;
    if (strcmp(name, "F32") == 0) return YVEX_QUANT_QTYPE_F32;
    if (strcmp(name, "F16") == 0) return YVEX_QUANT_QTYPE_F16;
    if (strcmp(name, "BF16") == 0) return YVEX_QUANT_QTYPE_BF16;
    if (strcmp(name, "Q8_0") == 0) return YVEX_QUANT_QTYPE_Q8_0;
    if (strcmp(name, "Q4_0") == 0) return YVEX_QUANT_QTYPE_Q4_0;
    if (strcmp(name, "Q4_K") == 0) return YVEX_QUANT_QTYPE_Q4_K;
    if (strcmp(name, "Q5_K") == 0) return YVEX_QUANT_QTYPE_Q5_K;
    if (strcmp(name, "Q6_K") == 0) return YVEX_QUANT_QTYPE_Q6_K;
    if (strcmp(name, "Q2_K") == 0) return YVEX_QUANT_QTYPE_Q2_K;
    if (strcmp(name, "IQ2_XXS") == 0) return YVEX_QUANT_QTYPE_IQ2_XXS;
    if (strcmp(name, "IQ2_XS") == 0) return YVEX_QUANT_QTYPE_IQ2_XS;
    if (strcmp(name, "IQ3_XXS") == 0) return YVEX_QUANT_QTYPE_IQ3_XXS;
    if (strcmp(name, "IQ4_NL") == 0) return YVEX_QUANT_QTYPE_IQ4_NL;
    if (strcmp(name, "OTHER") == 0) return YVEX_QUANT_QTYPE_OTHER;
    return YVEX_QUANT_QTYPE_UNKNOWN;
}

const char *yvex_quant_selector_kind_name(yvex_quant_selector_kind kind)
{
    switch (kind) {
    case YVEX_QUANT_SELECTOR_UNKNOWN: return "unknown";
    case YVEX_QUANT_SELECTOR_ROLE: return "role";
    case YVEX_QUANT_SELECTOR_TENSOR_NAME: return "tensor_name";
    case YVEX_QUANT_SELECTOR_TENSOR_PATTERN: return "pattern";
    case YVEX_QUANT_SELECTOR_LAYER_RANGE: return "layer_range";
    case YVEX_QUANT_SELECTOR_EXPERT_GROUP: return "expert_group";
    case YVEX_QUANT_SELECTOR_DEFAULT: return "default";
    }
    return "unknown";
}

yvex_quant_selector_kind yvex_quant_selector_kind_from_name(const char *name)
{
    if (!name) return YVEX_QUANT_SELECTOR_UNKNOWN;
    if (strcmp(name, "role") == 0) return YVEX_QUANT_SELECTOR_ROLE;
    if (strcmp(name, "tensor_name") == 0) return YVEX_QUANT_SELECTOR_TENSOR_NAME;
    if (strcmp(name, "name") == 0) return YVEX_QUANT_SELECTOR_TENSOR_NAME;
    if (strcmp(name, "pattern") == 0) return YVEX_QUANT_SELECTOR_TENSOR_PATTERN;
    if (strcmp(name, "tensor_pattern") == 0) return YVEX_QUANT_SELECTOR_TENSOR_PATTERN;
    if (strcmp(name, "layer_range") == 0) return YVEX_QUANT_SELECTOR_LAYER_RANGE;
    if (strcmp(name, "expert_group") == 0) return YVEX_QUANT_SELECTOR_EXPERT_GROUP;
    if (strcmp(name, "default") == 0) return YVEX_QUANT_SELECTOR_DEFAULT;
    return YVEX_QUANT_SELECTOR_UNKNOWN;
}

const char *yvex_quant_policy_status_name(yvex_quant_policy_status status)
{
    switch (status) {
    case YVEX_QUANT_POLICY_STATUS_UNKNOWN: return "quant-policy-unknown";
    case YVEX_QUANT_POLICY_STATUS_VALID: return "quant-policy-valid";
    case YVEX_QUANT_POLICY_STATUS_PARTIAL: return "quant-policy-partial";
    case YVEX_QUANT_POLICY_STATUS_INVALID: return "quant-policy-invalid";
    }
    return "quant-policy-unknown";
}

const char *yvex_quant_policy_issue_kind_name(yvex_quant_policy_issue_kind issue)
{
    switch (issue) {
    case YVEX_QUANT_POLICY_ISSUE_NONE: return "none";
    case YVEX_QUANT_POLICY_ISSUE_UNKNOWN_QTYPE: return "unknown_qtype";
    case YVEX_QUANT_POLICY_ISSUE_UNSUPPORTED_STORAGE_QTYPE: return "unsupported_storage_qtype";
    case YVEX_QUANT_POLICY_ISSUE_UNSUPPORTED_COMPUTE_QTYPE: return "unsupported_compute_qtype";
    case YVEX_QUANT_POLICY_ISSUE_UNKNOWN_ROLE: return "unknown_role";
    case YVEX_QUANT_POLICY_ISSUE_UNMATCHED_SELECTOR: return "unmatched_selector";
    case YVEX_QUANT_POLICY_ISSUE_TEMPLATE_QTYPE_MISMATCH: return "template_qtype_mismatch";
    case YVEX_QUANT_POLICY_ISSUE_REQUIRES_IMATRIX: return "requires_imatrix";
    case YVEX_QUANT_POLICY_ISSUE_FORMAT: return "format";
    }
    return "format";
}

yvex_dtype yvex_quant_qtype_to_dtype(yvex_quant_qtype qtype)
{
    switch (qtype) {
    case YVEX_QUANT_QTYPE_F32: return YVEX_DTYPE_F32;
    case YVEX_QUANT_QTYPE_F16: return YVEX_DTYPE_F16;
    case YVEX_QUANT_QTYPE_BF16: return YVEX_DTYPE_BF16;
    case YVEX_QUANT_QTYPE_Q8_0: return YVEX_DTYPE_Q8_0;
    case YVEX_QUANT_QTYPE_Q4_0: return YVEX_DTYPE_Q4_0;
    case YVEX_QUANT_QTYPE_Q4_K: return YVEX_DTYPE_Q4_K;
    case YVEX_QUANT_QTYPE_Q5_K: return YVEX_DTYPE_Q5_K;
    case YVEX_QUANT_QTYPE_Q6_K: return YVEX_DTYPE_Q6_K;
    case YVEX_QUANT_QTYPE_Q2_K: return YVEX_DTYPE_Q2_K;
    case YVEX_QUANT_QTYPE_IQ2_XXS: return YVEX_DTYPE_IQ2_XXS;
    case YVEX_QUANT_QTYPE_IQ2_XS: return YVEX_DTYPE_IQ2_XS;
    case YVEX_QUANT_QTYPE_IQ3_XXS: return YVEX_DTYPE_IQ3_XXS;
    case YVEX_QUANT_QTYPE_IQ4_NL: return YVEX_DTYPE_IQ4_NL;
    case YVEX_QUANT_QTYPE_UNKNOWN:
    case YVEX_QUANT_QTYPE_OTHER:
        return YVEX_DTYPE_UNKNOWN;
    }
    return YVEX_DTYPE_UNKNOWN;
}

int yvex_quant_qtype_storage_supported(yvex_quant_qtype qtype)
{
    const yvex_dtype_info *info = yvex_dtype_get_info(yvex_quant_qtype_to_dtype(qtype));
    return info && info->is_supported_for_storage_accounting;
}

int yvex_quant_qtype_compute_supported(yvex_quant_qtype qtype)
{
    return qtype == YVEX_QUANT_QTYPE_F32;
}

yvex_tensor_role yvex_quant_role_from_name(const char *name)
{
    unsigned int i;

    if (!name) return YVEX_TENSOR_ROLE_UNKNOWN;
    for (i = 0; i <= (unsigned int)YVEX_TENSOR_ROLE_MOE_EXPERT_DOWN; ++i) {
        yvex_tensor_role role = (yvex_tensor_role)i;
        if (strcmp(name, yvex_tensor_role_name(role)) == 0) return role;
    }
    return YVEX_TENSOR_ROLE_UNKNOWN;
}

static void qp_refresh_summary(yvex_quant_policy *policy)
{
    unsigned long long i;

    memset(&policy->summary, 0, sizeof(policy->summary));
    policy->summary.name = policy->name;
    policy->summary.architecture = policy->architecture;
    policy->summary.rule_count = policy->rule_count;
    policy->summary.status = policy->rule_count > 0 ? YVEX_QUANT_POLICY_STATUS_VALID : YVEX_QUANT_POLICY_STATUS_INVALID;
    for (i = 0; i < policy->rule_count; ++i) {
        yvex_quant_policy_rule *rule = &policy->rules[i];
        if (rule->requires_imatrix) policy->summary.requires_imatrix_count++;
        if (rule->storage_supported) policy->summary.storage_supported_count++;
        if (rule->compute_supported) policy->summary.compute_supported_count++;
        if (rule->qtype == YVEX_QUANT_QTYPE_UNKNOWN ||
            rule->selector_kind == YVEX_QUANT_SELECTOR_UNKNOWN ||
            (rule->selector_kind == YVEX_QUANT_SELECTOR_ROLE && rule->role == YVEX_TENSOR_ROLE_UNKNOWN)) {
            policy->summary.issue_count++;
            policy->summary.status = YVEX_QUANT_POLICY_STATUS_INVALID;
        } else if (!rule->storage_supported || !rule->compute_supported || rule->requires_imatrix) {
            policy->summary.issue_count++;
            if (policy->summary.status == YVEX_QUANT_POLICY_STATUS_VALID) {
                policy->summary.status = YVEX_QUANT_POLICY_STATUS_PARTIAL;
            }
        }
    }
}

int yvex_quant_policy_add_rule(yvex_quant_policy *policy,
                               yvex_quant_selector_kind selector_kind,
                               const char *selector,
                               yvex_tensor_role role,
                               yvex_quant_qtype qtype,
                               int requires_imatrix,
                               yvex_error *err)
{
    yvex_quant_policy_rule *next;
    yvex_quant_policy_rule *rule;

    if (!policy || !selector) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "quant_policy_add", "policy and selector are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (policy->rule_count == policy->rule_cap) {
        unsigned long long cap = policy->rule_cap == 0 ? 8u : policy->rule_cap * 2u;
        next = (yvex_quant_policy_rule *)realloc(policy->rules, (size_t)cap * sizeof(policy->rules[0]));
        if (!next) {
            yvex_error_set(err, YVEX_ERR_NOMEM, "quant_policy_add", "rule allocation failed");
            return YVEX_ERR_NOMEM;
        }
        policy->rules = next;
        policy->rule_cap = cap;
    }
    rule = &policy->rules[policy->rule_count];
    memset(rule, 0, sizeof(*rule));
    rule->selector_kind = selector_kind;
    rule->selector = yvex_quant_policy_strdup(selector);
    if (!rule->selector) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "quant_policy_add", "selector allocation failed");
        return YVEX_ERR_NOMEM;
    }
    rule->role = role;
    rule->qtype = qtype;
    rule->requires_imatrix = requires_imatrix ? 1 : 0;
    rule->storage_supported = yvex_quant_qtype_storage_supported(qtype);
    rule->compute_supported = yvex_quant_qtype_compute_supported(qtype);
    policy->rule_count++;
    qp_refresh_summary(policy);
    return YVEX_OK;
}

int yvex_quant_policy_open(yvex_quant_policy **out, const char *path, yvex_error *err)
{
    int rc = yvex_quant_policy_parse_json(out, path, err);
    if (rc == YVEX_OK) {
        rc = yvex_quant_policy_validate(*out, NULL, err);
    }
    return rc;
}

void yvex_quant_policy_close(yvex_quant_policy *policy)
{
    unsigned long long i;

    if (!policy) return;
    free(policy->name);
    free(policy->architecture);
    free(policy->source_kind);
    free(policy->template_path);
    for (i = 0; i < policy->rule_count; ++i) {
        free((char *)policy->rules[i].selector);
    }
    free(policy->rules);
    free(policy);
}

int yvex_quant_policy_write_json(const char *out_path,
                                 const yvex_quant_policy *policy,
                                 yvex_error *err)
{
    return yvex_quant_policy_write_json_file(out_path, policy, err);
}

int yvex_quant_policy_get_summary(const yvex_quant_policy *policy,
                                  yvex_quant_policy_summary *out,
                                  yvex_error *err)
{
    if (!policy || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "quant_policy_summary", "policy and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = policy->summary;
    return YVEX_OK;
}

unsigned long long yvex_quant_policy_rule_count(const yvex_quant_policy *policy)
{
    return policy ? policy->rule_count : 0;
}

const yvex_quant_policy_rule *yvex_quant_policy_rule_at(const yvex_quant_policy *policy,
                                                        unsigned long long index)
{
    if (!policy || index >= policy->rule_count) return NULL;
    return &policy->rules[index];
}
