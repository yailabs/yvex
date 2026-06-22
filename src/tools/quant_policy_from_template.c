/*
 * YVEX - Quant policy derivation from GGUF template
 *
 * File: src/tools/quant_policy_from_template.c
 * Layer: tool-plane implementation
 */
#include "quant_policy_internal.h"

#include <stdio.h>
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

static int qp_has_role_qtype(const yvex_quant_policy *policy,
                             yvex_tensor_role role,
                             yvex_quant_qtype qtype)
{
    unsigned long long i;

    for (i = 0; i < policy->rule_count; ++i) {
        const yvex_quant_policy_rule *rule = &policy->rules[i];
        if (rule->selector_kind == YVEX_QUANT_SELECTOR_ROLE &&
            rule->role == role && rule->qtype == qtype) {
            return 1;
        }
    }
    return 0;
}

int yvex_quant_policy_create_from_template(yvex_quant_policy **out,
                                           const char *template_path,
                                           const char *architecture,
                                           yvex_error *err)
{
    yvex_artifact_options artifact_options;
    yvex_artifact *artifact = NULL;
    yvex_gguf *gguf = NULL;
    yvex_tensor_table *tensors = NULL;
    yvex_quant_policy *policy = NULL;
    unsigned long long i;
    int rc;

    if (!out || !template_path || !architecture) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "quant_policy_derive", "out, template_path, and architecture are required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;
    policy = (yvex_quant_policy *)calloc(1, sizeof(*policy));
    if (!policy) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "quant_policy_derive", "policy allocation failed");
        return YVEX_ERR_NOMEM;
    }
    policy->name = yvex_quant_policy_strdup("template-derived-policy");
    policy->architecture = yvex_quant_policy_strdup(architecture);
    policy->source_kind = yvex_quant_policy_strdup("template-derived");
    policy->template_path = yvex_quant_policy_strdup(template_path);
    if (!policy->name || !policy->architecture || !policy->source_kind || !policy->template_path) {
        rc = YVEX_ERR_NOMEM;
        yvex_error_set(err, YVEX_ERR_NOMEM, "quant_policy_derive", "policy string allocation failed");
        goto done;
    }

    memset(&artifact_options, 0, sizeof(artifact_options));
    artifact_options.path = template_path;
    artifact_options.readonly = 1;
    artifact_options.map = 1;
    rc = yvex_artifact_open(&artifact, &artifact_options, err);
    if (rc == YVEX_OK) rc = yvex_gguf_open(&gguf, artifact, err);
    if (rc == YVEX_OK) rc = yvex_tensor_table_from_gguf(&tensors, gguf, err);
    if (rc != YVEX_OK) goto done;

    for (i = 0; i < yvex_tensor_table_count(tensors); ++i) {
        const yvex_tensor_info *tensor = yvex_tensor_table_at(tensors, i);
        yvex_quant_qtype qtype;

        if (!tensor) continue;
        qtype = qp_qtype_from_dtype(tensor->dtype);
        if (tensor->role != YVEX_TENSOR_ROLE_UNKNOWN) {
            if (qp_has_role_qtype(policy, tensor->role, qtype)) continue;
            rc = yvex_quant_policy_add_rule(policy, YVEX_QUANT_SELECTOR_ROLE,
                                            yvex_tensor_role_name(tensor->role),
                                            tensor->role, qtype, 0, err);
        } else {
            rc = yvex_quant_policy_add_rule(policy, YVEX_QUANT_SELECTOR_TENSOR_NAME,
                                            tensor->name,
                                            YVEX_TENSOR_ROLE_UNKNOWN, qtype, 0, err);
        }
        if (rc != YVEX_OK) goto done;
    }
    rc = yvex_quant_policy_validate(policy, NULL, err);
    if (rc != YVEX_OK) goto done;
    *out = policy;
    policy = NULL;
    rc = YVEX_OK;

done:
    yvex_quant_policy_close(policy);
    yvex_tensor_table_close(tensors);
    yvex_gguf_close(gguf);
    yvex_artifact_close(artifact);
    return rc;
}
