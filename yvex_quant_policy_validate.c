/*
 * YVEX - Quant policy validation
 *
 * File: yvex_quant_policy_validate.c
 * Layer: tool-plane implementation
 */
#include "yvex_quant_policy_internal.h"

#include <stdlib.h>
#include <string.h>

static yvex_quant_qtype qp_qtype_from_dtype(yvex_dtype dtype)
{
    switch (dtype) {
    case YVEX_DTYPE_F32: return YVEX_QUANT_QTYPE_F32;
    case YVEX_DTYPE_F16: return YVEX_QUANT_QTYPE_F16;
    case YVEX_DTYPE_BF16: return YVEX_QUANT_QTYPE_BF16;
    case YVEX_DTYPE_Q8_0: return YVEX_QUANT_QTYPE_Q8_0;
    case YVEX_DTYPE_Q4_0: return YVEX_QUANT_QTYPE_Q4_0;
    case YVEX_DTYPE_Q4_K: return YVEX_QUANT_QTYPE_Q4_K;
    case YVEX_DTYPE_Q5_K: return YVEX_QUANT_QTYPE_Q5_K;
    case YVEX_DTYPE_Q6_K: return YVEX_QUANT_QTYPE_Q6_K;
    case YVEX_DTYPE_Q2_K: return YVEX_QUANT_QTYPE_Q2_K;
    case YVEX_DTYPE_IQ2_XXS: return YVEX_QUANT_QTYPE_IQ2_XXS;
    case YVEX_DTYPE_IQ2_XS: return YVEX_QUANT_QTYPE_IQ2_XS;
    case YVEX_DTYPE_IQ3_XXS: return YVEX_QUANT_QTYPE_IQ3_XXS;
    case YVEX_DTYPE_IQ4_NL: return YVEX_QUANT_QTYPE_IQ4_NL;
    default: return YVEX_QUANT_QTYPE_OTHER;
    }
}

static void qp_set_summary(yvex_quant_policy *policy,
                           unsigned long long extra_issues,
                           int fatal)
{
    unsigned long long i;

    memset(&policy->summary, 0, sizeof(policy->summary));
    policy->summary.name = policy->name;
    policy->summary.architecture = policy->architecture;
    policy->summary.rule_count = policy->rule_count;
    policy->summary.status = policy->rule_count > 0 ? YVEX_QUANT_POLICY_STATUS_VALID : YVEX_QUANT_POLICY_STATUS_INVALID;
    policy->summary.issue_count = extra_issues;
    if (extra_issues > 0) policy->summary.status = fatal ? YVEX_QUANT_POLICY_STATUS_INVALID : YVEX_QUANT_POLICY_STATUS_PARTIAL;

    for (i = 0; i < policy->rule_count; ++i) {
        yvex_quant_policy_rule *rule = &policy->rules[i];
        rule->storage_supported = yvex_quant_qtype_storage_supported(rule->qtype);
        rule->compute_supported = yvex_quant_qtype_compute_supported(rule->qtype);
        if (rule->requires_imatrix) {
            policy->summary.requires_imatrix_count++;
            policy->summary.issue_count++;
            if (policy->summary.status == YVEX_QUANT_POLICY_STATUS_VALID) {
                policy->summary.status = YVEX_QUANT_POLICY_STATUS_PARTIAL;
            }
        }
        if (rule->storage_supported) policy->summary.storage_supported_count++;
        else {
            policy->summary.issue_count++;
            if (policy->summary.status == YVEX_QUANT_POLICY_STATUS_VALID) {
                policy->summary.status = YVEX_QUANT_POLICY_STATUS_PARTIAL;
            }
        }
        if (rule->compute_supported) policy->summary.compute_supported_count++;
        else {
            policy->summary.issue_count++;
            if (policy->summary.status == YVEX_QUANT_POLICY_STATUS_VALID) {
                policy->summary.status = YVEX_QUANT_POLICY_STATUS_PARTIAL;
            }
        }
        if (rule->qtype == YVEX_QUANT_QTYPE_UNKNOWN ||
            rule->selector_kind == YVEX_QUANT_SELECTOR_UNKNOWN ||
            (rule->selector_kind == YVEX_QUANT_SELECTOR_ROLE && rule->role == YVEX_TENSOR_ROLE_UNKNOWN)) {
            policy->summary.issue_count++;
            policy->summary.status = YVEX_QUANT_POLICY_STATUS_INVALID;
        }
    }
}

static int qp_match_pattern(const char *pattern, const char *name)
{
    const char *star;
    size_t prefix_len;
    size_t suffix_len;
    size_t name_len;

    if (!pattern || !name) return 0;
    if (strcmp(pattern, "*") == 0) return 1;
    star = strchr(pattern, '*');
    if (!star) return strcmp(pattern, name) == 0;
    prefix_len = (size_t)(star - pattern);
    suffix_len = strlen(star + 1);
    name_len = strlen(name);
    if (name_len < prefix_len + suffix_len) return 0;
    if (strncmp(pattern, name, prefix_len) != 0) return 0;
    if (suffix_len > 0 && strcmp(name + name_len - suffix_len, star + 1) != 0) return 0;
    return 1;
}

static int qp_validate_template(yvex_quant_policy *policy,
                                const char *template_path,
                                unsigned long long *issues,
                                yvex_error *err)
{
    yvex_artifact_options artifact_options;
    yvex_artifact *artifact = NULL;
    yvex_gguf *gguf = NULL;
    yvex_tensor_table *tensors = NULL;
    unsigned long long i;
    int rc;

    memset(&artifact_options, 0, sizeof(artifact_options));
    artifact_options.path = template_path;
    artifact_options.readonly = 1;
    artifact_options.map = 1;
    rc = yvex_artifact_open(&artifact, &artifact_options, err);
    if (rc == YVEX_OK) rc = yvex_gguf_open(&gguf, artifact, err);
    if (rc == YVEX_OK) rc = yvex_tensor_table_from_gguf(&tensors, gguf, err);
    if (rc != YVEX_OK) goto done;

    for (i = 0; i < policy->rule_count; ++i) {
        const yvex_quant_policy_rule *rule = &policy->rules[i];
        unsigned long long j;
        int matched = 0;

        for (j = 0; j < yvex_tensor_table_count(tensors); ++j) {
            const yvex_tensor_info *tensor = yvex_tensor_table_at(tensors, j);
            int applies = 0;

            if (!tensor) continue;
            if (rule->selector_kind == YVEX_QUANT_SELECTOR_DEFAULT) applies = 1;
            else if (rule->selector_kind == YVEX_QUANT_SELECTOR_ROLE && tensor->role == rule->role) applies = 1;
            else if (rule->selector_kind == YVEX_QUANT_SELECTOR_TENSOR_NAME && strcmp(rule->selector, tensor->name) == 0) applies = 1;
            else if (rule->selector_kind == YVEX_QUANT_SELECTOR_TENSOR_PATTERN && qp_match_pattern(rule->selector, tensor->name)) applies = 1;
            if (!applies) continue;
            matched = 1;
            if (qp_qtype_from_dtype(tensor->dtype) != rule->qtype) {
                (*issues)++;
            }
        }
        if (!matched) (*issues)++;
    }

done:
    yvex_tensor_table_close(tensors);
    yvex_gguf_close(gguf);
    yvex_artifact_close(artifact);
    return rc;
}

int yvex_quant_policy_validate(yvex_quant_policy *policy,
                               const char *template_path,
                               yvex_error *err)
{
    unsigned long long template_issues = 0;
    int rc = YVEX_OK;

    if (!policy) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "quant_policy_validate", "policy is required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!policy->name) policy->name = yvex_quant_policy_strdup("unnamed-policy");
    if (!policy->architecture) policy->architecture = yvex_quant_policy_strdup("unknown");
    if (!policy->name || !policy->architecture) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "quant_policy_validate", "policy string allocation failed");
        return YVEX_ERR_NOMEM;
    }
    if (template_path) {
        rc = qp_validate_template(policy, template_path, &template_issues, err);
        if (rc != YVEX_OK) return rc;
    }
    qp_set_summary(policy, template_issues, policy->rule_count == 0);
    return YVEX_OK;
}
