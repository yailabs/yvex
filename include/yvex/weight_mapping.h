/*
 * Owner: abi.weight_mapping (abi).
 * Owns: the public-abi boundary consumed by repository.
 * Does not own: unrelated subsystem policy or unsupported higher-stage claims.
 * Invariants: scope=generic and visibility=public match config/source_owners.tsv.
 * Boundary: public-abi; moving this contract requires an ownership-manifest change.
 *
 * YVEX - Open-weight tensor mapping API
 *
 * File: include/yvex/weight_mapping.h
 * Layer: public tool-plane API
 *
 * Purpose:
 *   Declares the open-weight intake contract-level tensor mapping surface. It maps native
 *   open-weight tensor names to canonical YVEX roles and proposed GGUF/template
 *   tensor targets without reading payload bytes or emitting GGUF.
 */
#ifndef YVEX_WEIGHT_MAPPING_H
#define YVEX_WEIGHT_MAPPING_H

#include <stddef.h>

#include <yvex/error.h>
#include <yvex/tensor.h>

#ifdef __cplusplus
extern "C" {
#endif

#define YVEX_WEIGHT_MAPPING_MAX_DIMS 8u

typedef enum {
    YVEX_WEIGHT_MAPPING_STATUS_UNKNOWN = 0,
    YVEX_WEIGHT_MAPPING_STATUS_MAPPED,
    YVEX_WEIGHT_MAPPING_STATUS_UNMAPPED,
    YVEX_WEIGHT_MAPPING_STATUS_AMBIGUOUS,
    YVEX_WEIGHT_MAPPING_STATUS_SHAPE_MISMATCH,
    YVEX_WEIGHT_MAPPING_STATUS_UNSUPPORTED_ARCH
} yvex_weight_mapping_status;

typedef enum {
    YVEX_WEIGHT_MAPPING_ISSUE_NONE = 0,
    YVEX_WEIGHT_MAPPING_ISSUE_UNKNOWN_NATIVE_NAME,
    YVEX_WEIGHT_MAPPING_ISSUE_UNKNOWN_TEMPLATE_NAME,
    YVEX_WEIGHT_MAPPING_ISSUE_ROLE_UNSUPPORTED,
    YVEX_WEIGHT_MAPPING_ISSUE_SHAPE_MISMATCH,
    YVEX_WEIGHT_MAPPING_ISSUE_ARCH_UNSUPPORTED,
    YVEX_WEIGHT_MAPPING_ISSUE_MOE_EXPERT_UNPARSED
} yvex_weight_mapping_issue_kind;

typedef struct {
    const char *native_name;
    const char *target_name;
    const char *architecture;
    yvex_tensor_role role;
    yvex_weight_mapping_status status;
    yvex_weight_mapping_issue_kind issue;
    unsigned int native_rank;
    unsigned long long native_dims[YVEX_WEIGHT_MAPPING_MAX_DIMS];
    unsigned int target_rank;
    unsigned long long target_dims[YVEX_WEIGHT_MAPPING_MAX_DIMS];
    int requires_transpose;
} yvex_weight_mapping_info;

typedef struct yvex_weight_mapping_table yvex_weight_mapping_table;

typedef struct {
    const char *architecture;
    const char *native_source_dir;
    const char *template_path;
    int compare_template;
    int require_all_native_mapped;
    int require_all_template_matched;
} yvex_weight_mapping_options;

int yvex_weight_mapping_table_build(yvex_weight_mapping_table **out,
                                    const yvex_weight_mapping_options *options,
                                    yvex_error *err);

void yvex_weight_mapping_table_close(yvex_weight_mapping_table *table);

unsigned long long yvex_weight_mapping_table_count(const yvex_weight_mapping_table *table);

const yvex_weight_mapping_info *yvex_weight_mapping_table_at(const yvex_weight_mapping_table *table,
                                                             unsigned long long index);

const yvex_weight_mapping_info *yvex_weight_mapping_table_find_native(const yvex_weight_mapping_table *table,
                                                                      const char *native_name);

const char *yvex_weight_mapping_status_name(yvex_weight_mapping_status status);
const char *yvex_weight_mapping_issue_kind_name(yvex_weight_mapping_issue_kind issue);
int yvex_gguf_map_deepseek_name(const char *native_name,
                                char *target,
                                size_t target_cap,
                                yvex_tensor_role *role,
                                yvex_weight_mapping_issue_kind *issue);
int yvex_qwen_adapter_map_name(const char *native_name,
                               char *target,
                               size_t target_cap,
                               yvex_tensor_role *role,
                               yvex_weight_mapping_issue_kind *issue);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_WEIGHT_MAPPING_H */
