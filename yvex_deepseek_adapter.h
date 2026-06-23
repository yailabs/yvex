/*
 * YVEX - DeepSeek weight mapping adapter
 *
 * File: yvex_deepseek_adapter.h
 * Layer: tool-plane implementation
 */
#ifndef YVEX_DEEPSEEK_ADAPTER_H
#define YVEX_DEEPSEEK_ADAPTER_H

#include <stddef.h>

#include <yvex/weight_mapping.h>

int yvex_deepseek_adapter_map_name(const char *native_name,
                                   char *target,
                                   size_t target_cap,
                                   yvex_tensor_role *role,
                                   yvex_weight_mapping_issue_kind *issue);

#endif /* YVEX_DEEPSEEK_ADAPTER_H */
