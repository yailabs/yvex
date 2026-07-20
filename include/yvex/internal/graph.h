/* Owner: graph family-recipe ABI.
 * Owns: normalized layer-plan geometry and immutable recipe projection.
 * Does not own: model numeric policy, family schedules, kernels, execution state, payload access, persistent KV, or
 *   generation.
 * Invariants: recipes copy admitted model facts without redefining their values.
 * Boundary: graph recipe admission is not attention execution support.
 * Purpose: decouple generic graph planning from family implementation types.
 * Inputs: model-owned numeric facts and one admitted family projection.
 * Effects: declarations only; no allocation, mutation, or I/O.
 * Failure: consumers reject unsupported combinations through their typed owner. */
#ifndef INCLUDE_YVEX_INTERNAL_GRAPH_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_GRAPH_H_INCLUDED

#include <stddef.h>

#include <yvex/backend.h>
#include <yvex/core.h>
#include <yvex/internal/artifact.h>
#include <yvex/internal/model.h>
#include <yvex/internal/runtime.h>

#define YVEX_ATTENTION_NO_LAYER (~0ull)
#define YVEX_ATTENTION_NO_TENSOR_INDEX (~0ull)

typedef struct {
    unsigned long long layer_index;
    yvex_attention_class attention_class;
    yvex_attention_compute_contract compute_contract;
    unsigned long long compression_ratio;
    unsigned long long sliding_window;
    unsigned long long query_heads;
    unsigned long long kv_heads;
    unsigned long long head_dimension;
    unsigned long long rope_head_dimension;
    unsigned long long query_lora_rank;
    unsigned long long output_lora_rank;
    unsigned long long output_groups;
    unsigned long long hidden_dimension;
    unsigned long long indexer_heads;
    unsigned long long indexer_head_dimension;
    unsigned long long indexer_topk;
    unsigned long long compressor_ape_columns;
    unsigned long long indexer_ape_columns;
    double rms_norm_epsilon;
    int compressor_required;
    int indexer_required;
    yvex_attention_position_policy position;
    yvex_attention_activation_policy attention_kv_activation;
    yvex_attention_activation_policy compressor_activation;
    yvex_attention_activation_policy compressor_rotated_activation;
    yvex_attention_activation_policy indexer_query_activation;
    yvex_attention_topk_policy sparse_topk;
    unsigned long long required_binding_count;
    unsigned long long qtype_compute_refusal_count;
    unsigned long long payload_bytes_bound;
} yvex_attention_layer_plan;

typedef int (*yvex_attention_recipe_identity_fn)(
    const void *context, char output[65]);
typedef int (*yvex_attention_recipe_layer_fn)(
    const void *context, unsigned long long index,
    yvex_attention_layer_plan *output);

typedef struct {
    const void *context;
    unsigned long long layer_count;
    unsigned long long auxiliary_layer_count;
    unsigned long long swa_layer_count;
    unsigned long long csa_layer_count;
    unsigned long long hca_layer_count;
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
    unsigned long long expected;
    unsigned long long actual;
    char tensor_name[YVEX_MATERIALIZATION_NAME_CAP];
    const char *reason;
} yvex_attention_failure;

typedef struct {
    int present;
    unsigned int schema_version;
    yvex_attention_rolling_kind kind;
    yvex_attention_class attention_class;
    unsigned long long layer_index;
    unsigned long long next_token_position;
    unsigned long long ratio;
    unsigned long long head_dimension;
    unsigned long long state_width;
    unsigned long long state_slots;
    unsigned long long previous_fill;
    unsigned long long current_fill;
    unsigned long long cursor;
    unsigned long long kv_state_stride;
    unsigned long long score_state_stride;
    unsigned long long kv_state_extent;
    unsigned long long score_state_extent;
    const float *kv_state;
    const float *score_state;
    int overlap;
    int rotated;
    char attention_plan_identity[YVEX_DEEPSEEK_ATTENTION_IDENTITY_CAP];
} yvex_attention_rolling_state_view;

typedef struct {
    int present;
    unsigned int schema_version;
    yvex_attention_rolling_kind kind;
    yvex_attention_class attention_class;
    unsigned long long layer_index;
    unsigned long long next_token_position;
    unsigned long long ratio;
    unsigned long long head_dimension;
    unsigned long long state_width;
    unsigned long long state_slots;
    unsigned long long previous_fill;
    unsigned long long current_fill;
    unsigned long long cursor;
    unsigned long long kv_state_stride;
    unsigned long long score_state_stride;
    unsigned long long kv_state_extent;
    unsigned long long score_state_extent;
    float *kv_state;
    float *score_state;
    int overlap;
    int rotated;
    char attention_plan_identity[YVEX_DEEPSEEK_ATTENTION_IDENTITY_CAP];
} yvex_attention_rolling_state_output;

typedef struct {
    unsigned long long token_count;
    unsigned long long local_tail_count;
    unsigned long long compressed_entry_count;
    unsigned long long indexer_entry_count;
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
    int owned;
    int complete;
    unsigned long long layer_index;
    yvex_attention_class attention_class;
    unsigned long long token_position;
    unsigned long long token_count;
    unsigned long long hidden_width;
    unsigned long long q_rank;
    unsigned long long query_width;
    unsigned long long kv_width;
    unsigned long long compressed_count;
    unsigned long long compressed_stride;
    unsigned long long indexer_count;
    unsigned long long indexer_stride;
    unsigned long long index_query_stride;
    unsigned long long index_weight_stride;
    unsigned long long topk_stride;
    float *input;
    float *q_low;
    float *query;
    float *raw_kv;
    float *compressed_kv;
    float *indexer_kv;
    float *index_query;
    float *index_weights;
    float *attention_values;
    float *output;
    unsigned long long *compressed_positions;
    unsigned long long *indexer_positions;
    unsigned long long *topk_counts;
    unsigned long long *topk_positions;
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
    unsigned long long layer_count;
    unsigned long long auxiliary_layer_count;
    unsigned long long swa_layer_count;
    unsigned long long csa_layer_count;
    unsigned long long hca_layer_count;
    unsigned long long required_binding_count;
    unsigned long long missing_binding_count;
    unsigned long long qtype_compute_refusal_count;
    unsigned long long payload_bytes_bound;
    unsigned long long qtype_binding_counts[YVEX_RUNTIME_DESCRIPTOR_QTYPE_CAP];
    int history_contract_ready;
    int state_delta_contract_ready;
    int cpu_reference_ready;
    int cuda_execution_ready;
    int full_execution_ready;
} yvex_attention_summary;

typedef struct yvex_attention_plan yvex_attention_plan;
typedef int (*yvex_attention_cancel_predicate)(void *context);
typedef struct {
    yvex_attention_cancel_predicate requested;
    void *context;
} yvex_attention_cancellation;

typedef struct {
    unsigned long long layer_index;
    unsigned long long token_position;
    unsigned long long token_count;
    unsigned long long local_history_tokens;
    unsigned long long compressed_history_tokens;
    unsigned long long max_q_b_rows;
    unsigned long long max_kv_rows;
    unsigned long long max_compressor_rows;
    unsigned long long max_indexer_rows;
    unsigned long long scratch_limit_bytes;
    const float *input;
    unsigned long long input_stride;
    const yvex_attention_history_view *history;
    yvex_attention_publication *publication;
    yvex_attention_execution_trace *trace;
    const yvex_attention_cancellation *cancellation;
} yvex_attention_cpu_options;

typedef struct {
    int executed;
    int full_attention;
    int cuda_executed;
    unsigned long long layer_index;
    yvex_attention_class attention_class;
    unsigned long long token_position;
    unsigned long long q_a_rows;
    unsigned long long q_b_rows;
    unsigned long long kv_rows;
    unsigned long long compressor_rows;
    unsigned long long indexer_rows;
    unsigned long long topk_candidates;
    unsigned long long topk_selected;
    unsigned long long local_entries;
    unsigned long long compressed_entries;
    unsigned long long payload_bytes_read;
    unsigned long long state_raw_entries;
    unsigned long long state_compressed_entries;
    unsigned long long state_indexer_entries;
    unsigned long long cuda_kernel_launches;
    unsigned long long cuda_peak_host_bytes;
    unsigned long long cuda_peak_device_bytes;
    double q_projection_checksum;
    double kv_projection_checksum;
    double rope_checksum;
    double attention_checksum;
    double output_checksum;
    char output_identity[YVEX_DEEPSEEK_ATTENTION_IDENTITY_CAP];
} yvex_attention_cpu_result;

typedef struct yvex_graph_family_api {
    int (*plan_build)(yvex_attention_plan **out, const void *family_ir,
                      const yvex_materialization_session *session,
                      const yvex_runtime_descriptor *descriptor,
                      yvex_attention_failure *failure, yvex_error *err);
    void (*plan_close)(yvex_attention_plan *plan);
    const yvex_attention_summary *(*plan_summary)(const yvex_attention_plan *plan);
    unsigned long long (*plan_layer_count)(const yvex_attention_plan *plan);
    const yvex_attention_layer_plan *(*plan_layer_at)(
        const yvex_attention_plan *plan, unsigned long long index);
    int (*history_validate)(const yvex_attention_layer_plan *layer,
                            const yvex_attention_history_view *history,
                            yvex_attention_failure *failure, yvex_error *err);
    int (*rolling_state_step_cpu)(
        const yvex_attention_layer_plan *layer,
        const yvex_attention_rolling_state_view *before,
        const float *token_kv, const float *token_score, const float *ape_row,
        yvex_attention_rolling_state_output *after, float *compressed_out,
        unsigned long long compressed_out_count, int *emitted,
        yvex_attention_failure *failure, yvex_error *err);
    void (*cpu_options_default)(yvex_attention_cpu_options *options);
    void (*publication_release)(yvex_attention_publication *publication);
    void (*execution_trace_release)(yvex_attention_execution_trace *trace);
    int (*cuda_token_execute)(
        const yvex_attention_plan *plan, const void *family_ir,
        yvex_materialization_session *session,
        const yvex_runtime_descriptor *descriptor, yvex_backend *backend,
        const yvex_attention_cpu_options *options,
        yvex_attention_cpu_result *result,
        yvex_attention_failure *failure, yvex_error *err);
    int (*cpu_chunk_execute)(
        const yvex_attention_plan *plan, const void *family_ir,
        yvex_materialization_session *session,
        const yvex_runtime_descriptor *descriptor,
        const yvex_attention_cpu_options *options,
        yvex_attention_cpu_result *result,
        yvex_attention_failure *failure, yvex_error *err);
} yvex_graph_family_api;

#endif
