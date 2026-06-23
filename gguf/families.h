/*
 * gguf/families.h - Model-family tensor mapping used by GGUF conversion.
 *
 * Family adapters map native tensor names to YVEX tensor roles and target
 * names. They are conversion helpers, not runtime support claims.
 */
#ifndef YVEX_GGUF_FAMILIES_H
#define YVEX_GGUF_FAMILIES_H

#include <stddef.h>

#include <yvex/weight_mapping.h>

int yvex_deepseek_adapter_map_name(const char *native_name,
                                   char *target,
                                   size_t target_cap,
                                   yvex_tensor_role *role,
                                   yvex_weight_mapping_issue_kind *issue);

int yvex_qwen_adapter_map_name(const char *native_name,
                               char *target,
                               size_t target_cap,
                               yvex_tensor_role *role,
                               yvex_weight_mapping_issue_kind *issue);

#endif /* YVEX_GGUF_FAMILIES_H */
