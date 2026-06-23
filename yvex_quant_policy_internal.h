/*
 * YVEX - Quant policy internals
 *
 * File: yvex_quant_policy_internal.h
 * Layer: tool-plane implementation
 */
#ifndef YVEX_QUANT_POLICY_INTERNAL_H
#define YVEX_QUANT_POLICY_INTERNAL_H

#include <yvex/artifact.h>
#include <yvex/dtype.h>
#include <yvex/gguf.h>
#include <yvex/quant_policy.h>
#include <yvex/tensor.h>

struct yvex_quant_policy {
    char *name;
    char *architecture;
    char *source_kind;
    char *template_path;
    yvex_quant_policy_rule *rules;
    unsigned long long rule_count;
    unsigned long long rule_cap;
    yvex_quant_policy_summary summary;
};

int yvex_quant_policy_add_rule(yvex_quant_policy *policy,
                               yvex_quant_selector_kind selector_kind,
                               const char *selector,
                               yvex_tensor_role role,
                               yvex_quant_qtype qtype,
                               int requires_imatrix,
                               yvex_error *err);

int yvex_quant_policy_parse_json(yvex_quant_policy **out,
                                 const char *path,
                                 yvex_error *err);

int yvex_quant_policy_validate(yvex_quant_policy *policy,
                               const char *template_path,
                               yvex_error *err);

int yvex_quant_policy_write_json_file(const char *out_path,
                                      const yvex_quant_policy *policy,
                                      yvex_error *err);

void yvex_quant_policy_print_summary(const yvex_quant_policy *policy,
                                     const char *mode,
                                     const char *path);

char *yvex_quant_policy_strdup(const char *s);
yvex_quant_qtype yvex_quant_qtype_from_name(const char *name);
yvex_quant_selector_kind yvex_quant_selector_kind_from_name(const char *name);
yvex_tensor_role yvex_quant_role_from_name(const char *name);
yvex_dtype yvex_quant_qtype_to_dtype(yvex_quant_qtype qtype);
int yvex_quant_qtype_storage_supported(yvex_quant_qtype qtype);
int yvex_quant_qtype_compute_supported(yvex_quant_qtype qtype);

#endif /* YVEX_QUANT_POLICY_INTERNAL_H */
