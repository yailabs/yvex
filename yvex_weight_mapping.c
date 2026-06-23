/*
 * YVEX - Open-weight tensor mapping implementation
 *
 * File: yvex_weight_mapping.c
 * Layer: tool-plane implementation
 */
#include "yvex_weight_mapping_internal.h"
#include "yvex_deepseek_adapter.h"
#include "yvex_qwen_adapter.h"

#include <stdlib.h>
#include <string.h>

static char *wm_strdup(const char *s)
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

static int wm_supported_arch(const char *architecture)
{
    return architecture &&
           (strcmp(architecture, "deepseek4") == 0 ||
            strcmp(architecture, "deepseek") == 0 ||
            strcmp(architecture, "qwen3") == 0 ||
            strcmp(architecture, "qwen") == 0);
}

const char *yvex_weight_mapping_status_name(yvex_weight_mapping_status status)
{
    switch (status) {
    case YVEX_WEIGHT_MAPPING_STATUS_UNKNOWN: return "unknown";
    case YVEX_WEIGHT_MAPPING_STATUS_MAPPED: return "mapped";
    case YVEX_WEIGHT_MAPPING_STATUS_UNMAPPED: return "unmapped";
    case YVEX_WEIGHT_MAPPING_STATUS_AMBIGUOUS: return "ambiguous";
    case YVEX_WEIGHT_MAPPING_STATUS_SHAPE_MISMATCH: return "shape_mismatch";
    case YVEX_WEIGHT_MAPPING_STATUS_UNSUPPORTED_ARCH: return "unsupported_arch";
    }
    return "unknown";
}

const char *yvex_weight_mapping_issue_kind_name(yvex_weight_mapping_issue_kind issue)
{
    switch (issue) {
    case YVEX_WEIGHT_MAPPING_ISSUE_NONE: return "none";
    case YVEX_WEIGHT_MAPPING_ISSUE_UNKNOWN_NATIVE_NAME: return "unknown_native_name";
    case YVEX_WEIGHT_MAPPING_ISSUE_UNKNOWN_TEMPLATE_NAME: return "unknown_template_name";
    case YVEX_WEIGHT_MAPPING_ISSUE_ROLE_UNSUPPORTED: return "role_unsupported";
    case YVEX_WEIGHT_MAPPING_ISSUE_SHAPE_MISMATCH: return "shape_mismatch";
    case YVEX_WEIGHT_MAPPING_ISSUE_ARCH_UNSUPPORTED: return "arch_unsupported";
    case YVEX_WEIGHT_MAPPING_ISSUE_MOE_EXPERT_UNPARSED: return "moe_expert_unparsed";
    }
    return "unknown_native_name";
}

static int wm_same_shape_native_target(const yvex_native_weight_info *native,
                                       const yvex_tensor_info *target,
                                       int *requires_transpose)
{
    unsigned int i;

    if (requires_transpose) *requires_transpose = 0;
    if (!native || !target || native->rank != target->rank) {
        return 0;
    }
    for (i = 0; i < native->rank; ++i) {
        if (native->dims[i] != target->dims[i]) {
            break;
        }
    }
    if (i == native->rank) {
        return 1;
    }
    if (native->rank == 2 &&
        native->dims[0] == target->dims[1] &&
        native->dims[1] == target->dims[0]) {
        if (requires_transpose) *requires_transpose = 1;
        return 1;
    }
    return 0;
}

static const yvex_tensor_info *wm_find_by_role_shape(const yvex_tensor_table *table,
                                                     yvex_tensor_role role,
                                                     const yvex_native_weight_info *native,
                                                     int *requires_transpose)
{
    unsigned long long i;

    if (!table || role == YVEX_TENSOR_ROLE_UNKNOWN || !native) {
        return NULL;
    }
    for (i = 0; i < yvex_tensor_table_count(table); ++i) {
        const yvex_tensor_info *candidate = yvex_tensor_table_at(table, i);
        int transpose = 0;

        if (!candidate || candidate->role != role) continue;
        if (wm_same_shape_native_target(native, candidate, &transpose)) {
            if (requires_transpose) *requires_transpose = transpose;
            return candidate;
        }
    }
    return NULL;
}

int yvex_weight_mapping_table_add(yvex_weight_mapping_table *table,
                                  const yvex_native_weight_info *native,
                                  const char *architecture,
                                  const char *target_name,
                                  yvex_tensor_role role,
                                  yvex_weight_mapping_status status,
                                  yvex_weight_mapping_issue_kind issue,
                                  const yvex_tensor_info *target,
                                  int requires_transpose,
                                  yvex_error *err)
{
    yvex_weight_mapping_info *next;
    yvex_weight_mapping_info *row;
    unsigned int i;

    if (!table || !native || !architecture) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "weight_mapping_add", "invalid mapping row");
        return YVEX_ERR_INVALID_ARG;
    }
    if (table->count == table->cap) {
        unsigned long long cap = table->cap == 0 ? 64u : table->cap * 2u;
        next = (yvex_weight_mapping_info *)realloc(table->items, (size_t)cap * sizeof(table->items[0]));
        if (!next) {
            yvex_error_set(err, YVEX_ERR_NOMEM, "weight_mapping_add", "mapping table allocation failed");
            return YVEX_ERR_NOMEM;
        }
        table->items = next;
        table->cap = cap;
    }
    row = &table->items[table->count];
    memset(row, 0, sizeof(*row));
    row->native_name = wm_strdup(native->name);
    row->target_name = wm_strdup(target ? target->name : target_name);
    row->architecture = wm_strdup(architecture);
    if (!row->native_name || !row->target_name || !row->architecture) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "weight_mapping_add", "mapping row allocation failed");
        free((char *)row->native_name);
        free((char *)row->target_name);
        free((char *)row->architecture);
        memset(row, 0, sizeof(*row));
        return YVEX_ERR_NOMEM;
    }
    row->role = role;
    row->status = status;
    row->issue = issue;
    row->native_rank = native->rank;
    for (i = 0; i < native->rank && i < YVEX_WEIGHT_MAPPING_MAX_DIMS; ++i) {
        row->native_dims[i] = native->dims[i];
    }
    if (target) {
        row->target_rank = target->rank;
        for (i = 0; i < target->rank && i < YVEX_WEIGHT_MAPPING_MAX_DIMS; ++i) {
            row->target_dims[i] = target->dims[i];
        }
    }
    row->requires_transpose = requires_transpose;
    table->count++;
    return YVEX_OK;
}

static int wm_open_template(yvex_weight_mapping_table *table,
                            const char *template_path,
                            yvex_error *err)
{
    yvex_artifact_options artifact_options;
    int rc;

    if (!template_path) return YVEX_OK;
    memset(&artifact_options, 0, sizeof(artifact_options));
    artifact_options.path = template_path;
    artifact_options.readonly = 1;
    artifact_options.map = 1;
    rc = yvex_artifact_open(&table->template_artifact, &artifact_options, err);
    if (rc == YVEX_OK) rc = yvex_gguf_open(&table->template_gguf, table->template_artifact, err);
    if (rc == YVEX_OK) rc = yvex_tensor_table_from_gguf(&table->template_tensors, table->template_gguf, err);
    return rc;
}

static int wm_map_native_row(yvex_weight_mapping_table *table,
                             const yvex_native_weight_info *native,
                             const yvex_weight_mapping_options *options,
                             yvex_error *err)
{
    char target_candidate[256];
    yvex_tensor_role role = YVEX_TENSOR_ROLE_UNKNOWN;
    yvex_weight_mapping_issue_kind issue = YVEX_WEIGHT_MAPPING_ISSUE_NONE;
    yvex_weight_mapping_status status = YVEX_WEIGHT_MAPPING_STATUS_MAPPED;
    const yvex_tensor_info *target = NULL;
    int requires_transpose = 0;
    int mapped;

    if (strcmp(options->architecture, "qwen3") == 0 ||
        strcmp(options->architecture, "qwen") == 0) {
        mapped = yvex_qwen_adapter_map_name(native->name, target_candidate, sizeof(target_candidate),
                                            &role, &issue);
    } else {
        mapped = yvex_deepseek_adapter_map_name(native->name, target_candidate, sizeof(target_candidate),
                                                &role, &issue);
    }
    if (!mapped) {
        return yvex_weight_mapping_table_add(table, native, options->architecture, "unknown",
                                             YVEX_TENSOR_ROLE_UNKNOWN,
                                             YVEX_WEIGHT_MAPPING_STATUS_UNMAPPED,
                                             issue, NULL, 0, err);
    }

    if (table->template_tensors) {
        target = yvex_tensor_table_find(table->template_tensors, target_candidate);
        if (target) {
            if (!wm_same_shape_native_target(native, target, &requires_transpose)) {
                status = YVEX_WEIGHT_MAPPING_STATUS_SHAPE_MISMATCH;
                issue = YVEX_WEIGHT_MAPPING_ISSUE_SHAPE_MISMATCH;
            }
        } else {
            target = wm_find_by_role_shape(table->template_tensors, role, native, &requires_transpose);
            if (!target) {
                status = YVEX_WEIGHT_MAPPING_STATUS_UNMAPPED;
                issue = YVEX_WEIGHT_MAPPING_ISSUE_UNKNOWN_TEMPLATE_NAME;
            }
        }
    }

    return yvex_weight_mapping_table_add(table, native, options->architecture, target_candidate,
                                         role, status, issue, target, requires_transpose, err);
}

int yvex_weight_mapping_table_build(yvex_weight_mapping_table **out,
                                    const yvex_weight_mapping_options *options,
                                    yvex_error *err)
{
    yvex_weight_mapping_table *table;
    yvex_native_weight_options native_options;
    unsigned long long i;
    int rc;

    if (!out || !options || !options->architecture || !options->native_source_dir) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "weight_mapping_build", "architecture and native_source_dir are required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;
    if (!wm_supported_arch(options->architecture)) {
        yvex_error_setf(err, YVEX_ERR_INVALID_ARG, "weight_mapping_build",
                        "unsupported architecture: %s", options->architecture);
        return YVEX_ERR_INVALID_ARG;
    }

    table = (yvex_weight_mapping_table *)calloc(1, sizeof(*table));
    if (!table) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "weight_mapping_build", "mapping table allocation failed");
        return YVEX_ERR_NOMEM;
    }

    memset(&native_options, 0, sizeof(native_options));
    native_options.source_dir = options->native_source_dir;
    native_options.recursive = 1;
    native_options.include_metadata = 0;
    rc = yvex_native_weight_table_open(&table->native, &native_options, err);
    if (rc == YVEX_OK && (options->compare_template || options->template_path)) {
        rc = wm_open_template(table, options->template_path, err);
    }
    if (rc != YVEX_OK) {
        yvex_weight_mapping_table_close(table);
        return rc;
    }

    for (i = 0; i < yvex_native_weight_table_count(table->native); ++i) {
        const yvex_native_weight_info *native = yvex_native_weight_table_at(table->native, i);
        rc = wm_map_native_row(table, native, options, err);
        if (rc != YVEX_OK) {
            yvex_weight_mapping_table_close(table);
            return rc;
        }
    }

    if (options->require_all_native_mapped) {
        for (i = 0; i < table->count; ++i) {
            if (table->items[i].status != YVEX_WEIGHT_MAPPING_STATUS_MAPPED) {
                yvex_error_set(err, YVEX_ERR_FORMAT, "weight_mapping_build", "unmapped native tensor present");
                yvex_weight_mapping_table_close(table);
                return YVEX_ERR_FORMAT;
            }
        }
    }
    if (options->require_all_template_matched && table->template_tensors) {
        unsigned long long j;
        for (i = 0; i < yvex_tensor_table_count(table->template_tensors); ++i) {
            const yvex_tensor_info *tensor = yvex_tensor_table_at(table->template_tensors, i);
            int matched = 0;
            for (j = 0; j < table->count; ++j) {
                if (tensor && table->items[j].target_name &&
                    strcmp(tensor->name, table->items[j].target_name) == 0 &&
                    table->items[j].status == YVEX_WEIGHT_MAPPING_STATUS_MAPPED) {
                    matched = 1;
                    break;
                }
            }
            if (!matched) {
                yvex_error_set(err, YVEX_ERR_FORMAT, "weight_mapping_build", "unmatched template tensor present");
                yvex_weight_mapping_table_close(table);
                return YVEX_ERR_FORMAT;
            }
        }
    }

    *out = table;
    yvex_error_clear(err);
    return YVEX_OK;
}

void yvex_weight_mapping_table_close(yvex_weight_mapping_table *table)
{
    unsigned long long i;

    if (!table) return;
    for (i = 0; i < table->count; ++i) {
        free((char *)table->items[i].native_name);
        free((char *)table->items[i].target_name);
        free((char *)table->items[i].architecture);
    }
    free(table->items);
    yvex_tensor_table_close(table->template_tensors);
    yvex_gguf_close(table->template_gguf);
    yvex_artifact_close(table->template_artifact);
    yvex_native_weight_table_close(table->native);
    free(table);
}

unsigned long long yvex_weight_mapping_table_count(const yvex_weight_mapping_table *table)
{
    return table ? table->count : 0;
}

const yvex_weight_mapping_info *yvex_weight_mapping_table_at(const yvex_weight_mapping_table *table,
                                                             unsigned long long index)
{
    if (!table || index >= table->count) return NULL;
    return &table->items[index];
}

const yvex_weight_mapping_info *yvex_weight_mapping_table_find_native(const yvex_weight_mapping_table *table,
                                                                      const char *native_name)
{
    unsigned long long i;

    if (!table || !native_name) return NULL;
    for (i = 0; i < table->count; ++i) {
        if (strcmp(table->items[i].native_name, native_name) == 0) {
            return &table->items[i];
        }
    }
    return NULL;
}
