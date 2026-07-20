/* Owner: graph internal ABI.
 * Owns: typed reports, normalized family recipes, attention plans, execution, and publication contracts.
 * Does not own: CLI policy, model numeric authority, backend kernels, payload access, persistent KV, or generation.
 * Invariants: cross-owner consumers use explicit immutable contracts and never graph-private implementation state.
 * Boundary: graph execution is not persistent runtime KV, transformer composition, or generation.
 * Purpose: expose the minimum graph contract required by runtime, backend, and operator adapters.
 * Inputs: admitted model, artifact, materialization, and runtime facts.
 * Effects: declarations only; owning implementations define allocation, mutation, and cleanup.
 * Failure: consumers reject unsupported combinations through typed graph and attention failures. */
#ifndef INCLUDE_YVEX_INTERNAL_GRAPH_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_GRAPH_H_INCLUDED

#include <stddef.h>
#include <yvex/backend.h>
#include <yvex/core.h>
#include <yvex/internal/artifact.h>
#include <yvex/internal/model.h>
#include <yvex/registry.h>

/* Immutable admitted model-to-runtime descriptor projection. */
#define YVEX_RUNTIME_DESCRIPTOR_IDENTITY_CAP 65u
#define YVEX_RUNTIME_DESCRIPTOR_QTYPE_CAP 43u
typedef struct {
    const char *status, *artifact_status, *reason, *next_row;
} yvex_runtime_descriptor_fact;
typedef enum {
    YVEX_RUNTIME_DESCRIPTOR_STATUS_REFUSED = 0,
    YVEX_RUNTIME_DESCRIPTOR_STATUS_READY
} yvex_runtime_descriptor_status;
typedef enum {
    YVEX_RUNTIME_DESCRIPTOR_FAILURE_NONE = 0,
    YVEX_RUNTIME_DESCRIPTOR_FAILURE_INVALID_ARGUMENT,
    YVEX_RUNTIME_DESCRIPTOR_FAILURE_ADMISSION,
    YVEX_RUNTIME_DESCRIPTOR_FAILURE_MATERIALIZATION,
    YVEX_RUNTIME_DESCRIPTOR_FAILURE_DUPLICATE_BINDING,
    YVEX_RUNTIME_DESCRIPTOR_FAILURE_MISSING_BINDING,
    YVEX_RUNTIME_DESCRIPTOR_FAILURE_ARCHITECTURE,
    YVEX_RUNTIME_DESCRIPTOR_FAILURE_QTYPE,
    YVEX_RUNTIME_DESCRIPTOR_FAILURE_ALLOCATION
} yvex_runtime_descriptor_failure_code;
typedef struct yvex_runtime_descriptor_failure {
    yvex_runtime_descriptor_failure_code code;
    unsigned long long tensor_index, expected, actual;
    char tensor_name[YVEX_MATERIALIZATION_NAME_CAP];
    const char *reason;
} yvex_runtime_descriptor_failure;
typedef struct {
    unsigned long long tensor_id, descriptor_index;
    const yvex_materialized_tensor_binding *binding;
    yvex_tensor_role role;
    yvex_tensor_collection collection;
    yvex_tensor_scope scope;
    unsigned long long layer_index, predictor_index;
    unsigned int qtype;
    yvex_materialization_placement placement;
    yvex_materialization_access_mode access_mode;
} yvex_runtime_tensor_binding;
typedef struct {
    yvex_runtime_descriptor_status status;
    char artifact_identity[YVEX_SHA256_HEX_CAP];
    char materialization_plan_identity[YVEX_MATERIALIZATION_IDENTITY_CAP];
    char logical_model_identity[YVEX_RUNTIME_DESCRIPTOR_IDENTITY_CAP];
    char runtime_descriptor_identity[YVEX_RUNTIME_DESCRIPTOR_IDENTITY_CAP];
    char runtime_numeric_identity[YVEX_RUNTIME_DESCRIPTOR_IDENTITY_CAP];
    char runtime_hadamard_revision[128];
    unsigned int runtime_numeric_schema_version;
    unsigned long long runtime_compute_policy_count, runtime_activation_policy_count;
    unsigned long long runtime_sparse_topk_policy_count, tensor_count, payload_bytes;
    unsigned long long qtype_tensor_counts[YVEX_RUNTIME_DESCRIPTOR_QTYPE_CAP];
    unsigned long long qtype_bytes[YVEX_RUNTIME_DESCRIPTOR_QTYPE_CAP];
    unsigned long long role_counts[YVEX_TENSOR_ROLE_COUNT];
    unsigned long long global_bindings, main_layer_bindings, mtp_bindings;
    unsigned long long routed_expert_bindings, expert_subview_count;
    unsigned long long missing_required_bindings, duplicate_bindings, unexpected_bindings;
    unsigned long long layer_count, mtp_layer_count, routed_experts, experts_per_token;
    unsigned long long vocabulary_size;
    int tokenizer_metadata_available, graph_execution_ready, generation_ready;
} yvex_runtime_descriptor_summary;
typedef struct yvex_runtime_descriptor yvex_runtime_descriptor;
const char *yvex_runtime_descriptor_status_name(yvex_runtime_descriptor_status status);
const char *yvex_runtime_descriptor_failure_name(yvex_runtime_descriptor_failure_code code);
int yvex_runtime_descriptor_build(yvex_runtime_descriptor **out,
                                  const yvex_complete_artifact_admission *admission,
                                  const yvex_materialization_session *session,
                                  yvex_runtime_descriptor_failure *failure, yvex_error *err);
void yvex_runtime_descriptor_close(yvex_runtime_descriptor *descriptor);
const yvex_runtime_descriptor_summary *
yvex_runtime_descriptor_summary_get(const yvex_runtime_descriptor *descriptor);
const yvex_runtime_tensor_binding *
yvex_runtime_descriptor_find_name(const yvex_runtime_descriptor *descriptor, const char *name);
const yvex_runtime_tensor_binding *
yvex_runtime_descriptor_find_role(const yvex_runtime_descriptor *descriptor, yvex_tensor_role role,
                                  yvex_tensor_scope scope, unsigned long long layer_index,
                                  unsigned long long predictor_index);

#define YVEX_ATTENTION_NO_LAYER (~0ull)
#define YVEX_ATTENTION_NO_TENSOR_INDEX (~0ull)
#define YVEX_GRAPH_LAYER_FIXTURE_MAX_OUTPUT_VALUES 16u

typedef struct {
    const char *backend_name;
    unsigned long long layers, seq_len, position, hidden_dim, head_dim, ffn_dim;
    const float *initial_position_values;
    unsigned long long initial_position_value_count;
} yvex_graph_layer_fixture_options;
typedef struct {
    int executed;
    const char *status, *graph_integrity_guard, *graph_execution_phase;
    const char *backend_status, *backend_op_status;
    unsigned long long layers, total_op_count, output_bytes, scratch_bytes;
    unsigned long long final_output_checksum, final_reference_checksum;
    double final_max_abs_diff;
    unsigned long long output_value_count;
    float output_values[YVEX_GRAPH_LAYER_FIXTURE_MAX_OUTPUT_VALUES];
    int cleanup_attempted;
    const char *cleanup_status;
} yvex_graph_layer_fixture_result;
int yvex_graph_execute_layer_fixture(const yvex_graph_layer_fixture_options *options,
                                     yvex_graph_layer_fixture_result *out, yvex_error *err);

/* Typed graph report ABI consumed by the graph CLI without private-header access. */
typedef enum {
    YVEX_GRAPH_REPORT_KIND_GRAPH = 0,
    YVEX_GRAPH_REPORT_KIND_MEMORY_PLAN,
    YVEX_GRAPH_REPORT_KIND_PLAN,
    YVEX_GRAPH_REPORT_KIND_PRIMITIVE,
    YVEX_GRAPH_REPORT_KIND_GUARD
} yvex_graph_report_kind;
typedef enum {
    YVEX_GRAPH_REPORT_MODE_NORMAL = 0,
    YVEX_GRAPH_REPORT_MODE_TABLE,
    YVEX_GRAPH_REPORT_MODE_AUDIT,
    YVEX_GRAPH_REPORT_MODE_JSON
} yvex_graph_report_mode;
typedef enum {
    YVEX_GRAPH_ACTION_DUMP = 0,
    YVEX_GRAPH_ACTION_CHECK,
    YVEX_GRAPH_ACTION_EXECUTE_FIXTURE,
    YVEX_GRAPH_ACTION_EXECUTE_PARTIAL,
    YVEX_GRAPH_ACTION_EXECUTE_SEGMENT,
    YVEX_GRAPH_ACTION_EXECUTE_OP,
    YVEX_GRAPH_ACTION_EXECUTE_BLOCK,
    YVEX_GRAPH_ACTION_EXECUTE_LAYERS
} yvex_graph_action;
typedef struct yvex_graph_report_request {
    yvex_graph_report_kind kind;
    yvex_graph_action action;
    yvex_graph_report_mode mode;
    const char *model, *backend, *segment, *tokens_text, *op, *block, *suite, *activation;
    unsigned long long sequence_length, context_length, fixture_token, partial_token;
    unsigned long long token_index, position, head_dim, seq_len, m, k, n;
    unsigned long long hidden_dim, ffn_dim, experts, expert_id, layers;
    int execute_fixture, execute_partial, execute_segment, execute_op;
    int execute_block, execute_layers, causal, gated, use_expert;
    int tokens_seen, fixture_token_seen, partial_token_seen, token_index_seen;
} yvex_graph_report_request;
typedef struct yvex_graph_report {
    yvex_graph_report_kind kind;
    const char *status, *graph_status, *architecture, *model_name, *backend, *backend_status;
    int backend_capability_tensor_alloc, backend_capability_tensor_read_write;
    int backend_capability_op_embed, backend_capability_op_matmul, backend_capability_op_mlp;
    int backend_capability_op_rms_norm, backend_capability_op_rope;
    int backend_capability_op_attention;
    unsigned long long value_count, op_count, missing_required_count;
    const char *memory_status;
    unsigned long long model_tensor_bytes_known, activation_peak_bytes, total_known_bytes;
    int execution_ready;
    const char *reason, *boundary;
    char *body;
    size_t body_len, body_cap;
    int exit_code;
} yvex_graph_report;
void yvex_graph_report_clear(yvex_graph_report *report);
int yvex_graph_report_build(const yvex_graph_report_request *request, yvex_graph_report *report,
                            yvex_error *err);
int yvex_graph_primitive_report_build(const yvex_graph_report_request *request,
                                      yvex_graph_report *report, yvex_error *err);

/* Bounded graph admission evidence shared with runtime and generation. */
typedef struct {
    const char *guard_status, *phase, *graph_kind, *integrity_status;
    const char *identity_status, *metadata_status, *shape_status, *range_status;
    const char *slice_range_status, *backend_status, *backend_op_status;
    int dispatch_attempted, reference_read_attempted;
    int output_allocation_attempted, cleanup_attempted;
    const char *cleanup_status;
    unsigned long long output_bytes_planned, output_bytes_allocated, reference_bytes_planned;
} yvex_cli_graph_guard_report;
int yvex_graph_preflight(const yvex_model_ref *model_ref, const char *backend_name,
                         int execute_fixture, int execute_segment, unsigned int token_id,
                         yvex_cli_graph_guard_report *report, yvex_error *err);

typedef struct {
    unsigned long long layer_index;
    yvex_attention_class attention_class;
    yvex_attention_compute_contract compute_contract;
    unsigned long long compression_ratio, sliding_window, query_heads, kv_heads;
    unsigned long long head_dimension, rope_head_dimension, query_lora_rank;
    unsigned long long output_lora_rank, output_groups, hidden_dimension;
    unsigned long long indexer_heads, indexer_head_dimension, indexer_topk;
    unsigned long long compressor_ape_columns, indexer_ape_columns;
    double rms_norm_epsilon;
    int compressor_required, indexer_required;
    yvex_attention_position_policy position;
    yvex_attention_activation_policy attention_kv_activation;
    yvex_attention_activation_policy compressor_activation;
    yvex_attention_activation_policy compressor_rotated_activation;
    yvex_attention_activation_policy indexer_query_activation;
    yvex_attention_topk_policy sparse_topk;
    unsigned long long required_binding_count, qtype_compute_refusal_count, payload_bytes_bound;
} yvex_attention_layer_plan;

typedef int (*yvex_attention_recipe_identity_fn)(const void *context, char output[65]);
typedef int (*yvex_attention_recipe_layer_fn)(const void *context, unsigned long long index,
                                              yvex_attention_layer_plan *output);

typedef struct {
    const void *context;
    unsigned long long layer_count, auxiliary_layer_count;
    unsigned long long swa_layer_count, csa_layer_count, hca_layer_count;
    yvex_attention_recipe_identity_fn identity;
    yvex_attention_recipe_layer_fn layer;
} yvex_attention_recipe;

/* Attention execution and publication are an internal cross-owner ABI. */
#define YVEX_DEEPSEEK_ATTENTION_IDENTITY_CAP 65u
#define YVEX_DEEPSEEK_ATTENTION_ROLLING_STATE_SCHEMA_V1 1u

typedef enum {
    YVEX_DEEPSEEK_ATTENTION_STATUS_REFUSED = 0,
    YVEX_DEEPSEEK_ATTENTION_STATUS_PLANNED,
    YVEX_DEEPSEEK_ATTENTION_STATUS_EXECUTION_UNSUPPORTED,
    YVEX_DEEPSEEK_ATTENTION_STATUS_EXECUTION_READY
} yvex_attention_status;

typedef enum {
    YVEX_DEEPSEEK_ATTENTION_FAILURE_NONE = 0,
    YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT,
    YVEX_DEEPSEEK_ATTENTION_FAILURE_ARCHITECTURE,
    YVEX_DEEPSEEK_ATTENTION_FAILURE_MATERIALIZATION,
    YVEX_DEEPSEEK_ATTENTION_FAILURE_DESCRIPTOR,
    YVEX_DEEPSEEK_ATTENTION_FAILURE_MISSING_BINDING,
    YVEX_DEEPSEEK_ATTENTION_FAILURE_QTYPE,
    YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION,
    YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY,
    YVEX_DEEPSEEK_ATTENTION_FAILURE_EXECUTION_UNSUPPORTED,
    YVEX_DEEPSEEK_ATTENTION_FAILURE_READ,
    YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC,
    YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA,
    YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION,
    YVEX_DEEPSEEK_ATTENTION_FAILURE_SCRATCH,
    YVEX_DEEPSEEK_ATTENTION_FAILURE_CANCELLED,
    YVEX_DEEPSEEK_ATTENTION_FAILURE_CLEANUP,
    YVEX_DEEPSEEK_ATTENTION_FAILURE_BACKEND
} yvex_attention_failure_code;

typedef enum {
    YVEX_DEEPSEEK_ATTENTION_ROLLING_NONE = 0,
    YVEX_DEEPSEEK_ATTENTION_ROLLING_MAIN,
    YVEX_DEEPSEEK_ATTENTION_ROLLING_INDEXER
} yvex_attention_rolling_kind;

typedef struct {
    yvex_attention_failure_code code;
    unsigned long long layer_index;
    yvex_tensor_role role;
    unsigned long long expected, actual;
    char tensor_name[YVEX_MATERIALIZATION_NAME_CAP];
    const char *reason;
} yvex_attention_failure;

typedef struct {
    int present;
    unsigned int schema_version;
    yvex_attention_rolling_kind kind;
    yvex_attention_class attention_class;
    unsigned long long layer_index, next_token_position, ratio, head_dimension;
    unsigned long long state_width, state_slots, previous_fill, current_fill, cursor;
    unsigned long long kv_state_stride, score_state_stride, kv_state_extent, score_state_extent;
    const float *kv_state, *score_state;
    int overlap, rotated;
    char attention_plan_identity[YVEX_DEEPSEEK_ATTENTION_IDENTITY_CAP];
} yvex_attention_rolling_state_view;

typedef struct {
    int present;
    unsigned int schema_version;
    yvex_attention_rolling_kind kind;
    yvex_attention_class attention_class;
    unsigned long long layer_index, next_token_position, ratio, head_dimension;
    unsigned long long state_width, state_slots, previous_fill, current_fill, cursor;
    unsigned long long kv_state_stride, score_state_stride, kv_state_extent, score_state_extent;
    float *kv_state, *score_state;
    int overlap, rotated;
    char attention_plan_identity[YVEX_DEEPSEEK_ATTENTION_IDENTITY_CAP];
} yvex_attention_rolling_state_output;

typedef struct {
    unsigned long long token_count, local_tail_count, compressed_entry_count, indexer_entry_count;
    const float *local_kv;
    unsigned long long local_kv_stride;
    const unsigned long long *local_positions;
    const float *compressed_kv;
    unsigned long long compressed_kv_stride;
    const unsigned long long *compressed_positions;
    const float *indexer_kv;
    unsigned long long indexer_kv_stride;
    const unsigned long long *indexer_positions;
    yvex_attention_rolling_state_view main_rolling_state;
    yvex_attention_rolling_state_view indexer_rolling_state;
    int immutable;
} yvex_attention_history_view;

typedef struct yvex_attention_publication {
    int owned, complete;
    unsigned long long layer_index;
    yvex_attention_class attention_class;
    unsigned long long token_position, token_count, hidden_width, q_rank;
    unsigned long long query_width, kv_width, compressed_count, compressed_stride;
    unsigned long long indexer_count, indexer_stride, index_query_stride;
    unsigned long long index_weight_stride, topk_stride;
    float *input, *q_low, *query, *raw_kv, *compressed_kv;
    float *indexer_kv, *index_query, *index_weights, *attention_values, *output;
    unsigned long long *compressed_positions, *indexer_positions;
    unsigned long long *topk_counts, *topk_positions;
    yvex_attention_rolling_state_output next_main_rolling_state;
    yvex_attention_rolling_state_output next_indexer_rolling_state;
} yvex_attention_publication;

/* Historical tests use the same owned object as an execution trace. */
typedef yvex_attention_publication yvex_attention_execution_trace;

typedef struct {
    yvex_attention_status status;
    char artifact_identity[YVEX_SHA256_HEX_CAP];
    char materialization_plan_identity[YVEX_MATERIALIZATION_IDENTITY_CAP];
    char logical_model_identity[YVEX_RUNTIME_DESCRIPTOR_IDENTITY_CAP];
    char runtime_descriptor_identity[YVEX_RUNTIME_DESCRIPTOR_IDENTITY_CAP];
    char runtime_numeric_identity[YVEX_RUNTIME_DESCRIPTOR_IDENTITY_CAP];
    char attention_plan_identity[YVEX_DEEPSEEK_ATTENTION_IDENTITY_CAP];
    unsigned long long layer_count, auxiliary_layer_count;
    unsigned long long swa_layer_count, csa_layer_count, hca_layer_count;
    unsigned long long required_binding_count, missing_binding_count;
    unsigned long long qtype_compute_refusal_count, payload_bytes_bound;
    unsigned long long qtype_binding_counts[YVEX_RUNTIME_DESCRIPTOR_QTYPE_CAP];
    int history_contract_ready, state_delta_contract_ready, cpu_reference_ready;
    int cuda_execution_ready, full_execution_ready;
} yvex_attention_summary;

typedef struct yvex_attention_plan yvex_attention_plan;
typedef int (*yvex_attention_cancel_predicate)(void *context);
typedef struct {
    yvex_attention_cancel_predicate requested;
    void *context;
} yvex_attention_cancellation;

typedef struct {
    unsigned long long layer_index, token_position, token_count;
    unsigned long long local_history_tokens, compressed_history_tokens;
    unsigned long long max_q_b_rows, max_kv_rows, max_compressor_rows, max_indexer_rows;
    unsigned long long scratch_limit_bytes;
    const float *input;
    unsigned long long input_stride;
    const yvex_attention_history_view *history;
    yvex_attention_publication *publication;
    yvex_attention_execution_trace *trace;
    const yvex_attention_cancellation *cancellation;
} yvex_attention_cpu_options;

typedef struct {
    int executed, full_attention, cuda_executed;
    unsigned long long layer_index;
    yvex_attention_class attention_class;
    unsigned long long token_position, q_a_rows, q_b_rows, kv_rows;
    unsigned long long compressor_rows, indexer_rows, topk_candidates, topk_selected;
    unsigned long long local_entries, compressed_entries, payload_bytes_read;
    unsigned long long state_raw_entries, state_compressed_entries, state_indexer_entries;
    unsigned long long cuda_kernel_launches, cuda_peak_host_bytes, cuda_peak_device_bytes;
    double q_projection_checksum, kv_projection_checksum, rope_checksum;
    double attention_checksum, output_checksum;
    char output_identity[YVEX_DEEPSEEK_ATTENTION_IDENTITY_CAP];
} yvex_attention_cpu_result;

typedef struct yvex_graph_family_api {
    int (*plan_build)(yvex_attention_plan **out, const void *family_ir,
                      const yvex_materialization_session *session,
                      const yvex_runtime_descriptor *descriptor, yvex_attention_failure *failure,
                      yvex_error *err);
    void (*plan_close)(yvex_attention_plan *plan);
    const yvex_attention_summary *(*plan_summary)(const yvex_attention_plan *plan);
    unsigned long long (*plan_layer_count)(const yvex_attention_plan *plan);
    const yvex_attention_layer_plan *(*plan_layer_at)(const yvex_attention_plan *plan,
                                                      unsigned long long index);
    int (*history_validate)(const yvex_attention_layer_plan *layer,
                            const yvex_attention_history_view *history,
                            yvex_attention_failure *failure, yvex_error *err);
    int (*rolling_state_step_cpu)(const yvex_attention_layer_plan *layer,
                                  const yvex_attention_rolling_state_view *before,
                                  const float *token_kv, const float *token_score,
                                  const float *ape_row, yvex_attention_rolling_state_output *after,
                                  float *compressed_out, unsigned long long compressed_out_count,
                                  int *emitted, yvex_attention_failure *failure, yvex_error *err);
    void (*cpu_options_default)(yvex_attention_cpu_options *options);
    void (*publication_release)(yvex_attention_publication *publication);
    void (*execution_trace_release)(yvex_attention_execution_trace *trace);
    int (*cuda_token_execute)(const yvex_attention_plan *plan, const void *family_ir,
                              yvex_materialization_session *session,
                              const yvex_runtime_descriptor *descriptor, yvex_backend *backend,
                              const yvex_attention_cpu_options *options,
                              yvex_attention_cpu_result *result, yvex_attention_failure *failure,
                              yvex_error *err);
    int (*cpu_chunk_execute)(const yvex_attention_plan *plan, const void *family_ir,
                             yvex_materialization_session *session,
                             const yvex_runtime_descriptor *descriptor,
                             const yvex_attention_cpu_options *options,
                             yvex_attention_cpu_result *result, yvex_attention_failure *failure,
                             yvex_error *err);
} yvex_graph_family_api;

typedef enum { YVEX_ATTENTION_PROBE_CANONICAL = 0 } yvex_attention_probe_kind;
typedef enum {
    YVEX_ATTENTION_PROBE_SCOPE_QUICK = 0,
    YVEX_ATTENTION_PROBE_SCOPE_FULL
} yvex_attention_probe_scope;
typedef struct {
    yvex_backend_kind backend;
    yvex_attention_probe_kind probe;
    yvex_attention_probe_scope scope;
    int compare_backends;
    int (*cancel_requested)(void *context);
    void *cancel_context;
} yvex_attention_probe_request;
typedef struct {
    unsigned long long value_count;
    unsigned long long finite_value_count;
    unsigned long long nonfinite_value_count;
    unsigned long long first_failing_coordinate;
    double maximum_absolute_error;
    double maximum_relative_error;
    double squared_error_sum;
    int within_tolerance;
    int bitwise_equal;
} yvex_graph_f32_comparison;
int yvex_graph_f32_compare(const float *left, const float *right,
                           unsigned long long count, double absolute_tolerance,
                           double relative_tolerance, yvex_graph_f32_comparison *out,
                           yvex_error *err);
typedef struct {
    char attention_execution_identity[YVEX_SHA256_HEX_CAP], output_digest[YVEX_SHA256_HEX_CAP];
    char cpu_output_digest[YVEX_SHA256_HEX_CAP], cuda_output_digest[YVEX_SHA256_HEX_CAP];
    char comparison_contract_identity[YVEX_SHA256_HEX_CAP];
    char cuda_device[128];
    unsigned long long layers_executed, bindings_executed;
    unsigned long long swa_layers_executed, csa_layers_executed, hca_layers_executed;
    unsigned long long topk_selected, hca_ratio, payload_bytes_read;
    unsigned long long kernel_launches, peak_device_bytes, comparison_values;
    unsigned long long comparison_finite_values, comparison_nonfinite_values;
    unsigned long long first_failing_layer, first_failing_coordinate;
    int cuda_compute_capability_major, cuda_compute_capability_minor;
    double comparison_maximum_absolute_error, comparison_maximum_relative_error;
    double comparison_rmse;
    int comparison_available, comparison_passed;
    int bitwise_equality_observed, bitwise_equality_required;
} yvex_attention_probe_result;

/* Operator-owned graph reachability contract. */
#define YVEX_GRAPH_ATTENTION_TEXT_CAP 128u
#define YVEX_GRAPH_ATTENTION_REASON_CAP 256u
typedef struct {
    const char *target, *source_path, *models_root, *source_manifest_path, *artifact_path;
    yvex_backend_kind backend;
    yvex_attention_probe_kind probe;
    yvex_attention_probe_scope scope;
    int compare_backends;
    int (*cancel_requested)(void *context);
    void *cancel_context;
} yvex_graph_attention_operator_request;
typedef struct {
    int completed;
    char status[32];
    char command[YVEX_GRAPH_ATTENTION_TEXT_CAP];
    char target[YVEX_GRAPH_ATTENTION_TEXT_CAP];
    char family[32];
    char backend[32];
    char scope[16];
    char input_class[64];
    char execution_class[32];
    char weights_class[64];
    char artifact_path[YVEX_PATH_CAP];
    char cuda_device[YVEX_GRAPH_ATTENTION_TEXT_CAP];
    char source_snapshot_identity[17];
    char payload_identity[YVEX_SHA256_HEX_CAP];
    char artifact_identity[YVEX_SHA256_HEX_CAP];
    char artifact_transform_identity[YVEX_SHA256_HEX_CAP];
    char transform_identity[YVEX_SHA256_HEX_CAP];
    char materialization_identity[YVEX_SHA256_HEX_CAP];
    char logical_model_identity[YVEX_SHA256_HEX_CAP];
    char runtime_numeric_identity[YVEX_SHA256_HEX_CAP];
    char runtime_descriptor_identity[YVEX_SHA256_HEX_CAP];
    char attention_plan_identity[YVEX_SHA256_HEX_CAP];
    char attention_execution_identity[YVEX_SHA256_HEX_CAP];
    char output_digest[YVEX_SHA256_HEX_CAP];
    char cpu_output_digest[YVEX_SHA256_HEX_CAP];
    char cuda_output_digest[YVEX_SHA256_HEX_CAP];
    char comparison_contract_identity[YVEX_SHA256_HEX_CAP];
    char current_writer_plan_identity[YVEX_SHA256_HEX_CAP];
    char payload_plan_identity[YVEX_SHA256_HEX_CAP];
    char payload_byte_identity[YVEX_SHA256_HEX_CAP];
    char reason[YVEX_GRAPH_ATTENTION_REASON_CAP];
    unsigned long long main_layers_total, layers_executed, bindings_total, bindings_executed;
    unsigned long long swa_layers_executed, csa_layers_executed, hca_layers_executed;
    unsigned long long topk_selected, hca_ratio, payload_bytes_read;
    unsigned long long kernel_launches, peak_device_bytes, comparison_values;
    unsigned long long comparison_finite_values, comparison_nonfinite_values;
    unsigned long long first_failing_layer, first_failing_coordinate;
    int cuda_compute_capability_major, cuda_compute_capability_minor;
    double comparison_maximum_absolute_error, comparison_maximum_relative_error;
    double comparison_rmse;
    int comparison_available, comparison_passed;
    int bitwise_equality_observed, bitwise_equality_required;
    int physical_payload_compatible, artifact_rebuild_required;
    int materialization_rebuild_required, tensor_inventory_equal, qtype_equal;
    int layout_equal, offset_equal, payload_digest_equal;
    int attention_execution_supported, attention_cuda_execution_ready;
    int production_api_available, internal_live_runner_available, operator_command_available;
    int end_user_generation_available, runtime_generation_ready;
    unsigned long long artifact_bytes_hashed;
    int artifact_identity_verified;
    char failure_code[32];
    char failure_where[YVEX_ERROR_WHERE_CAP];
} yvex_graph_attention_operator_result;

/* Execute the canonical operator probe through a supplied admitted family recipe.
 * Inputs: sealed attention owners, immutable request, and caller-owned result.
 * Effects: executes CPU and/or CUDA attention and publishes only a complete aggregate.
 * Failure: releases all probe state and backend resources without committing partial result.
 * Boundary: bounded attention input only; no persistent KV, prompt, transformer, or generation. */
int yvex_attention_probe_execute(const yvex_graph_family_api *family,
                                 const yvex_attention_plan *plan, const void *family_ir,
                                 yvex_materialization_session *session,
                                 const yvex_runtime_descriptor *descriptor,
                                 const yvex_attention_probe_request *request,
                                 yvex_attention_probe_result *result,
                                 yvex_attention_failure *failure, yvex_error *err);

/* Execute one operator-reachable probe through admitted artifact and graph owners.
 * Inputs: explicit canonical paths, backend, probe, and scope.
 * Effects: owns the temporary lifecycle and publishes only a complete typed result.
 * Failure: reverse-order cleanup preserves external source/artifact state.
 * Boundary: attention probe only; no prompt, persistent KV, transformer, or generation. */
int yvex_graph_attention_operator_execute(const yvex_graph_attention_operator_request *request,
                                          yvex_graph_attention_operator_result *result,
                                          yvex_error *err);

#endif
