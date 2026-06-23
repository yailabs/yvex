/*
 * YVEX - Qwen weight mapping adapter
 */
#ifndef YVEX_QWEN_ADAPTER_H
#define YVEX_QWEN_ADAPTER_H

#include <stddef.h>

#include <yvex/weight_mapping.h>

int yvex_qwen_adapter_map_name(const char *native_name,
                               char *target,
                               size_t target_cap,
                               yvex_tensor_role *role,
                               yvex_weight_mapping_issue_kind *issue);

#endif /* YVEX_QWEN_ADAPTER_H */
