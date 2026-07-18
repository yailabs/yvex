/*
 * fullmodel.h - fullmodel-family CLI surface ownership.
 * Owner: src/cli/model_artifacts
 * Owns: shared fullmodel-family CLI types and function declarations.
 * Does not own: runtime generation, graph execution, artifact emission, eval,
 * benchmark, or release claims.
 * Invariants: CLI-only and excluded from libyvex.a.
 * Boundary: fullmodel-family reports remain diagnostic/report-only.
 */
#ifndef YVEX_FULLMODEL_SURFACE_H
#define YVEX_FULLMODEL_SURFACE_H

#include "surface_common.h"

typedef enum {
    YVEX_FULLMODEL_COMMAND_REPORT = 0,
    YVEX_FULLMODEL_COMMAND_MATERIALIZATION_PLAN,
    YVEX_FULLMODEL_COMMAND_MATERIALIZE,
    YVEX_FULLMODEL_COMMAND_DESCRIPTOR,
    YVEX_FULLMODEL_COMMAND_FAMILY_RUNTIME
} yvex_fullmodel_command_kind;

typedef struct {
    const char *model;
    const char *backend;
    const char *target;
    const char *registry_path;
    const char *residency;
    const char *require_role;
    const char *require_collection;
    const char *fail_after_phase;
    const char *report_dir;
    const char *format;
    const char *family;
    unsigned long long limit_tensors;
    unsigned long long limit_bytes;
    int has_limit_bytes;
    int dry_run;
    int plan_only;
    int include_blockers;
    int include_roles;
    int include_placement;
    int include_graph;
    int include_kv;
    int include_logits;
    int include_moe;
    int include_output;
    yvex_models_output_mode output_mode;
    yvex_fullmodel_command_kind command;
} yvex_cli_fullmodel_options;

typedef struct {
    unsigned long long embedding;
    unsigned long long embedding_bytes;
    unsigned long long normalization;
    unsigned long long normalization_bytes;
    unsigned long long attention;
    unsigned long long attention_bytes;
    unsigned long long mlp;
    unsigned long long mlp_bytes;
    unsigned long long moe;
    unsigned long long moe_bytes;
    unsigned long long output;
    unsigned long long output_bytes;
    unsigned long long tokenizer;
    unsigned long long tokenizer_bytes;
    unsigned long long unknown;
    unsigned long long unknown_bytes;
    int has_token_embedding;
    int has_attention_norm;
    int has_post_attention_norm;
    int has_attention_q;
    int has_attention_k;
    int has_attention_v;
    int has_attention_out;
    int has_ffn_gate;
    int has_ffn_up;
    int has_ffn_down;
    int has_moe_router;
    int has_moe_expert;
    int has_output_norm;
    int has_output_head;
    int has_tokenizer_metadata;
} yvex_fullmodel_collections;

typedef struct {
    char name[32];
    unsigned long long count;
    unsigned long long bytes;
} yvex_fullmodel_dtype_bucket;

typedef struct {
    const yvex_tensor_info *tensor;
    unsigned long long bytes;
} yvex_fullmodel_largest_tensor;

typedef struct {
    int available;
    int memory_known;
    unsigned long long total_bytes;
    unsigned long long available_bytes;
    unsigned long long required_bytes;
    const char *fit_status;
    char fit_reason[160];
} yvex_fullmodel_backend_fit;

typedef struct {
    const yvex_cli_fullmodel_options *options;
    const char *status;
    const char *model_resolved_path;
    const char *target_id;
    const char *target_class;
    const char *artifact_identity_status;
    const char *tensor_inventory_status;
    const char *required_role_coverage;
    const char *missing_required_roles;
    const char *unsupported_required_roles;
    const char *placement_plan_status;
    const char *memory_budget_status;
    const char *backend_preflight_status;
    const char *materialization_mode;
    const char *full_model_materialization;
    const char *full_model_materialization_proof;
    const char *phase;
    const char *failed_phase;
    const char *failed_reason;
    const char *cleanup_attempted;
    const char *cleanup_status;
    const char *cleanup_idempotent;
    const char *owned_state_released;
    const char *partial_materialization;
    const char *residency_plan;
    const char *runtime_blockers;
    unsigned long long materialized_tensor_count;
    unsigned long long materialized_tensor_bytes;
    unsigned long long refused_tensor_count;
    unsigned long long skipped_tensor_count;
    unsigned long long required_tensor_count;
    unsigned long long required_tensor_bytes;
    unsigned long long peak_planned_bytes;
    unsigned long long cpu_resident_bytes;
    unsigned long long cuda_resident_bytes;
} yvex_fullmodel_materialize_report;

int yvex_model_artifacts_surface_fullmodel_command(int arg_count, char **args);
void yvex_model_artifacts_surface_fullmodel_help(FILE *fp);

int parse_fullmodel_options(int arg_count, char **args, yvex_cli_fullmodel_options *options);
int fullmodel_string_is_empty(const char *text);
int fullmodel_file_size(const char *path, unsigned long long *out);
const char *fullmodel_family_from_arch(yvex_arch arch);
int fullmodel_name_has(const char *name, const char *needle);
void fullmodel_csv_append(char *buf, size_t cap, const char *value);
void fullmodel_record_dtype(yvex_fullmodel_dtype_bucket buckets[32],
                            unsigned int *count,
                            const yvex_tensor_info *tensor);
void fullmodel_dtype_summary(char *out,
                             size_t out_cap,
                             const yvex_fullmodel_dtype_bucket buckets[32],
                             unsigned int count);
void fullmodel_record_largest(yvex_fullmodel_largest_tensor top[16],
                              unsigned int *count,
                              unsigned int limit,
                              const yvex_tensor_info *tensor);
void fullmodel_classify_tensor(const yvex_tensor_info *tensor,
                               yvex_fullmodel_collections *collections);
int fullmodel_is_selected_target(const char *text);
void print_fullmodel_common_boundaries(void);
const char *fullmodel_identity_status(const yvex_model_ref *ref,
                                      unsigned long long artifact_bytes);
void fullmodel_probe_backend_fit(const char *backend,
                                 unsigned long long required_bytes,
                                 yvex_fullmodel_backend_fit *fit);
int fullmodel_has_attention_collection(const yvex_fullmodel_collections *collections);
int fullmodel_has_mlp_collection(const yvex_fullmodel_collections *collections);
int fullmodel_has_normalization_collection(const yvex_fullmodel_collections *collections);
int fullmodel_residency_is_future_unsupported(const char *residency);
const yvex_tensor_info *fullmodel_descriptor_find_tensor(yvex_model_context *ctx,
                                                         const char *role);
const char *fullmodel_role_status_from_tensor(yvex_model_context *ctx,
                                              const yvex_fullmodel_collections *collections,
                                              const char *role);

const char *fullmodel_detect_family(const yvex_cli_fullmodel_options *options,
                                    yvex_arch arch,
                                    const char *target_id);
const char *fullmodel_requested_family(const yvex_cli_fullmodel_options *options);
int fullmodel_family_request_matches(const char *requested, const char *detected);
int fullmodel_print_family_runtime_report(const yvex_cli_fullmodel_options *options,
                                          yvex_model_ref *ref,
                                          yvex_model_context *ctx,
                                          const char *target_id,
                                          const char *target_class,
                                          unsigned long long artifact_bytes,
                                          yvex_arch arch,
                                          unsigned long long tensor_count,
                                          unsigned long long total_tensor_bytes,
                                          const yvex_fullmodel_collections *collections,
                                          const char *role_coverage,
                                          const char *missing_roles,
                                          const char *unsupported_roles,
                                          int selected_target);
void fullmodel_print_materialize_report(const yvex_fullmodel_materialize_report *report);
void fullmodel_print_descriptor_phases(const char *role_status,
                                       const char *graph_status,
                                       const char *failure_phase);
void fullmodel_print_phase(unsigned int index,
                           const char *name,
                           const char *status,
                           unsigned long long tensor_count,
                           unsigned long long tensor_bytes,
                           const char *residency,
                           int required,
                           int blocked,
                           const char *blocker);
unsigned int fullmodel_print_blocker(unsigned int index,
                                     const char *category,
                                     const char *severity,
                                     const char *message,
                                     int blocks_full_materialization,
                                     int blocks_generation);
void fullmodel_print_report_normal(const yvex_cli_fullmodel_options *options,
                                   const char *status,
                                   const char *target_id,
                                   const char *target_class,
                                   const char *role_coverage,
                                   const char *top_blocker,
                                   const char *next);
void fullmodel_print_plan_normal(const yvex_cli_fullmodel_options *options,
                                 const char *status,
                                 const char *target_class,
                                 const char *fit,
                                 const char *top_blocker);
void fullmodel_print_descriptor_normal(const yvex_cli_fullmodel_options *options,
                                       const char *target_id,
                                       const char *descriptor_status,
                                       const char *role_status,
                                       const char *missing_roles);
void fullmodel_print_family_runtime_normal(const yvex_cli_fullmodel_options *options,
                                           const char *target_id,
                                           const char *target_class,
                                           const char *role_coverage,
                                           const char *missing_roles);
int print_fullmodel_source_only_report(const char *target, const char *backend);
int print_fullmodel_source_only_plan(const yvex_cli_fullmodel_options *options,
                                     const char *target);
int print_fullmodel_source_only_materialize(const yvex_cli_fullmodel_options *options,
                                            const char *target);
int print_fullmodel_source_only_descriptor(const yvex_cli_fullmodel_options *options,
                                           const char *target);
int print_fullmodel_source_only_family_runtime(const yvex_cli_fullmodel_options *options,
                                               const char *target);
int print_fullmodel_missing_report(const yvex_cli_fullmodel_options *options,
                                   const char *resolved_path);
int print_fullmodel_parse_failure_report(const yvex_cli_fullmodel_options *options,
                                         const yvex_model_ref *ref,
                                         const char *reason,
                                         int rc);
void fullmodel_print_descriptor_report(const yvex_cli_fullmodel_options *options,
                                       yvex_model_ref *ref,
                                       yvex_model_context *ctx,
                                       const char *target_id,
                                       const char *target_class,
                                       unsigned long long artifact_bytes,
                                       yvex_arch arch,
                                       unsigned long long tensor_count,
                                       unsigned long long total_tensor_bytes,
                                       const yvex_fullmodel_collections *collections,
                                       const char *role_coverage,
                                       const char *missing_roles,
                                       const char *unsupported_roles,
                                       int selected_target);

#endif
