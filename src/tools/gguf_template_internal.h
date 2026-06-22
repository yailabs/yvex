/*
 * YVEX - GGUF template internals
 *
 * File: src/tools/gguf_template_internal.h
 * Layer: tool-plane implementation
 */
#ifndef YVEX_GGUF_TEMPLATE_INTERNAL_H
#define YVEX_GGUF_TEMPLATE_INTERNAL_H

#include <yvex/artifact.h>
#include <yvex/gguf.h>
#include <yvex/gguf_template.h>
#include <yvex/model.h>
#include <yvex/native_weights.h>
#include <yvex/tensor.h>
#include <yvex/tokenizer.h>

struct yvex_gguf_template {
    yvex_artifact *artifact;
    yvex_gguf *gguf;
    yvex_tensor_table *tensors;
    yvex_model_descriptor *model;
    yvex_tokenizer *tokenizer;
    yvex_native_weight_table *native;
    yvex_gguf_template_summary summary;
    char *architecture;
    char *model_name;
    yvex_gguf_template_issue *issues;
    unsigned long long issue_count;
    unsigned long long issue_cap;
    unsigned long long native_tensor_count;
    unsigned long long matched_exact;
    unsigned long long missing_in_native;
    unsigned long long shape_mismatch;
};

int yvex_gguf_template_validate(yvex_gguf_template *tmpl,
                                const yvex_gguf_template_options *options,
                                yvex_error *err);

int yvex_gguf_template_compare_native(yvex_gguf_template *tmpl,
                                      const yvex_gguf_template_options *options,
                                      yvex_error *err);

int yvex_gguf_template_add_issue(yvex_gguf_template *tmpl,
                                 yvex_gguf_template_issue_kind kind,
                                 const char *tensor_name,
                                 const char *message,
                                 yvex_error *err);

void yvex_gguf_template_print_summary(const yvex_gguf_template *tmpl,
                                      const char *mode,
                                      const char *template_path);

#endif /* YVEX_GGUF_TEMPLATE_INTERNAL_H */
