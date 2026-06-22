/*
 * YVEX - Weight mapping internals
 *
 * File: src/tools/weight_mapping_internal.h
 * Layer: tool-plane implementation
 */
#ifndef YVEX_WEIGHT_MAPPING_INTERNAL_H
#define YVEX_WEIGHT_MAPPING_INTERNAL_H

#include <yvex/artifact.h>
#include <yvex/gguf.h>
#include <yvex/native_weights.h>
#include <yvex/tensor.h>
#include <yvex/weight_mapping.h>

struct yvex_weight_mapping_table {
    yvex_native_weight_table *native;
    yvex_artifact *template_artifact;
    yvex_gguf *template_gguf;
    yvex_tensor_table *template_tensors;
    yvex_weight_mapping_info *items;
    unsigned long long count;
    unsigned long long cap;
};

int yvex_weight_mapping_table_add(yvex_weight_mapping_table *table,
                                  const yvex_native_weight_info *native,
                                  const char *architecture,
                                  const char *target_name,
                                  yvex_tensor_role role,
                                  yvex_weight_mapping_status status,
                                  yvex_weight_mapping_issue_kind issue,
                                  const yvex_tensor_info *target,
                                  int requires_transpose,
                                  yvex_error *err);

void yvex_weight_mapping_print_shape(const unsigned long long *dims, unsigned int rank);

#endif /* YVEX_WEIGHT_MAPPING_INTERNAL_H */
