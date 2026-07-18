/*
 * yvex_deepseek_attention.h - DeepSeek-V4 attention graph boundary.
 *
 * Owner:
 *   src/graph
 *
 * Owns:
 *   DeepSeek-V4-Flash attention-plan admission from the canonical architecture
 *   IR, committed materialization session, and runtime descriptor; typed CPU
 *   and CUDA execution, history-view, and state-delta contracts.
 *
 * Does not own:
 *   source parsing, GGUF mapping, materialization, runtime descriptor
 *   projection, persistent KV storage, prefill, decode, logits, sampling,
 *   generation, eval, benchmark, release claims, CLI parsing, or rendering.
 *
 * Invariants:
 *   plans bind immutable upstream identities and validate every release
 *   attention layer before numerical execution; execution publishes output
 *   and rolling state only after the complete class-specific equation passes.
 *
 * Boundary:
 *   complete attention-layer execution is graph evidence, not persistent KV,
 *   transformer execution, or runtime generation.
 */
#ifndef YVEX_DEEPSEEK_ATTENTION_H
#define YVEX_DEEPSEEK_ATTENTION_H

#include "src/artifact/yvex_artifact_materialize.h"
#include "src/model/architecture/yvex_deepseek_v4_ir.h"
#include "src/model/yvex_runtime_descriptor.h"

#include <stddef.h>
#include <yvex/backend.h>
#include <yvex/error.h>
#include <yvex/tensor.h>

#define YVEX_DEEPSEEK_ATTENTION_IDENTITY_CAP 65u
#define YVEX_DEEPSEEK_ATTENTION_ROLLING_STATE_SCHEMA_V1 1u

typedef enum {
    YVEX_DEEPSEEK_ATTENTION_STATUS_REFUSED = 0,
    YVEX_DEEPSEEK_ATTENTION_STATUS_PLANNED,
    YVEX_DEEPSEEK_ATTENTION_STATUS_EXECUTION_UNSUPPORTED,
    YVEX_DEEPSEEK_ATTENTION_STATUS_EXECUTION_READY
} yvex_deepseek_attention_status;

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
    YVEX_DEEPSEEK_ATTENTION_FAILURE_BACKEND
} yvex_deepseek_attention_failure_code;

typedef enum {
    YVEX_DEEPSEEK_ATTENTION_DELTA_EMPTY = 0,
    YVEX_DEEPSEEK_ATTENTION_DELTA_PENDING,
    YVEX_DEEPSEEK_ATTENTION_DELTA_COMMITTED,
    YVEX_DEEPSEEK_ATTENTION_DELTA_ABORTED
} yvex_deepseek_attention_delta_status;

typedef enum {
    YVEX_DEEPSEEK_ATTENTION_ROLLING_NONE = 0,
    YVEX_DEEPSEEK_ATTENTION_ROLLING_MAIN,
    YVEX_DEEPSEEK_ATTENTION_ROLLING_INDEXER
} yvex_deepseek_attention_rolling_kind;

typedef enum {
    YVEX_DEEPSEEK_ATTENTION_COMPONENT_ATTENTION_OUTPUT = 0,
    YVEX_DEEPSEEK_ATTENTION_COMPONENT_RAW_LOCAL_KV,
    YVEX_DEEPSEEK_ATTENTION_COMPONENT_COMPRESSED_MAIN_KV,
    YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_KV,
    YVEX_DEEPSEEK_ATTENTION_COMPONENT_MAIN_KV_STATE,
    YVEX_DEEPSEEK_ATTENTION_COMPONENT_MAIN_SCORE_STATE,
    YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_KV_STATE,
    YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_SCORE_STATE,
    YVEX_DEEPSEEK_ATTENTION_COMPONENT_COUNT
} yvex_deepseek_attention_component_kind;

typedef enum {
    YVEX_DEEPSEEK_ATTENTION_COMPONENT_STORAGE_F32 = 1
} yvex_deepseek_attention_component_storage;

typedef enum {
    YVEX_DEEPSEEK_ATTENTION_TRANSACTION_EMPTY = 0,
    YVEX_DEEPSEEK_ATTENTION_TRANSACTION_BEGUN,
    YVEX_DEEPSEEK_ATTENTION_TRANSACTION_ABORTED,
    YVEX_DEEPSEEK_ATTENTION_TRANSACTION_COMMITTED
} yvex_deepseek_attention_transaction_status;

typedef struct {
    yvex_deepseek_attention_failure_code code;
    unsigned long long layer_index;
    yvex_tensor_role role;
    unsigned long long expected;
    unsigned long long actual;
    char tensor_name[YVEX_MATERIALIZATION_NAME_CAP];
    const char *reason;
} yvex_deepseek_attention_failure;

typedef struct {
    int present;
    unsigned int schema_version;
    yvex_deepseek_attention_rolling_kind kind;
    yvex_deepseek_v4_attention_class attention_class;
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
} yvex_deepseek_attention_rolling_state_view;

typedef struct {
    int present;
    unsigned int schema_version;
    yvex_deepseek_attention_rolling_kind kind;
    yvex_deepseek_v4_attention_class attention_class;
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
} yvex_deepseek_attention_rolling_state_output;

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
    yvex_deepseek_attention_rolling_state_view main_rolling_state;
    yvex_deepseek_attention_rolling_state_view indexer_rolling_state;
    int immutable;
} yvex_deepseek_attention_history_view;

typedef struct {
    int owned;
    int complete;
    unsigned long long layer_index;
    yvex_deepseek_v4_attention_class attention_class;
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
    yvex_deepseek_attention_rolling_state_output next_main_rolling_state;
    yvex_deepseek_attention_rolling_state_output next_indexer_rolling_state;
} yvex_deepseek_attention_execution_trace;

typedef struct {
    yvex_deepseek_attention_delta_status status;
    unsigned long long layer_index;
    yvex_deepseek_v4_attention_class attention_class;
    unsigned long long raw_kv_entries;
    unsigned long long compressed_kv_entries;
    unsigned long long indexer_entries;
    unsigned long long token_position;
    float *raw_kv_out;
    unsigned long long raw_kv_stride;
    float *compressed_kv_out;
    unsigned long long compressed_kv_stride;
    float *indexer_kv_out;
    unsigned long long indexer_kv_stride;
    yvex_deepseek_attention_rolling_state_output next_main_rolling_state;
    yvex_deepseek_attention_rolling_state_output next_indexer_rolling_state;
    int published;
} yvex_deepseek_attention_state_delta;

typedef struct {
    yvex_deepseek_attention_component_kind kind;
    yvex_deepseek_attention_component_storage storage;
    unsigned int rank;
    unsigned long long dims[4];
    unsigned long long stride;
    unsigned long long expected_elements;
    unsigned long long produced_elements;
    unsigned long long byte_extent;
    unsigned long long position_start;
    unsigned long long position_count;
    void *data;
    int required;
    int acquired;
    int written;
    int sealed;
} yvex_deepseek_attention_component_span;

typedef struct {
    yvex_deepseek_attention_component_kind fail_acquire_kind;
    yvex_deepseek_attention_component_kind fail_seal_kind;
    int fail_begin;
    int fail_commit;
    int fail_abort;
} yvex_deepseek_attention_memory_sink_options;

typedef struct {
    int initialized;
    int has_committed;
    yvex_deepseek_attention_memory_sink_options options;
    yvex_deepseek_attention_component_span committed[
        YVEX_DEEPSEEK_ATTENTION_COMPONENT_COUNT];
    char committed_identity[YVEX_DEEPSEEK_ATTENTION_IDENTITY_CAP];
} yvex_deepseek_attention_memory_sink;

typedef struct {
    yvex_deepseek_attention_transaction_status status;
    yvex_deepseek_attention_memory_sink *sink;
    unsigned long long layer_index;
    yvex_deepseek_v4_attention_class attention_class;
    unsigned long long token_position;
    unsigned long long token_count;
    char previous_state_identity[YVEX_DEEPSEEK_ATTENTION_IDENTITY_CAP];
    char transaction_identity[YVEX_DEEPSEEK_ATTENTION_IDENTITY_CAP];
    yvex_deepseek_attention_component_span components[
        YVEX_DEEPSEEK_ATTENTION_COMPONENT_COUNT];
} yvex_deepseek_attention_state_transaction;

typedef struct {
    unsigned long long layer_index;
    yvex_deepseek_v4_attention_class attention_class;
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
    yvex_deepseek_v4_runtime_activation_policy attention_kv_activation;
    yvex_deepseek_v4_runtime_activation_policy compressor_activation;
    yvex_deepseek_v4_runtime_activation_policy compressor_rotated_activation;
    yvex_deepseek_v4_runtime_activation_policy indexer_query_activation;
    yvex_deepseek_v4_runtime_sparse_topk_policy sparse_topk;
    unsigned long long required_binding_count;
    unsigned long long qtype_compute_refusal_count;
    unsigned long long payload_bytes_bound;
} yvex_deepseek_attention_layer_plan;

typedef struct {
    yvex_deepseek_attention_status status;
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
    int history_contract_ready;
    int state_delta_contract_ready;
    int cpu_reference_ready;
    int cuda_execution_ready;
    int full_execution_ready;
} yvex_deepseek_attention_summary;

typedef struct yvex_deepseek_attention_plan yvex_deepseek_attention_plan;

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
    int collect_reference_metrics;
    const float *input;
    unsigned long long input_stride;
    const yvex_deepseek_attention_history_view *history;
    yvex_deepseek_attention_execution_trace *trace;
} yvex_deepseek_attention_cpu_options;

typedef struct {
    int executed;
    int full_attention;
    int reference_independent;
    int cuda_executed;
    unsigned long long layer_index;
    yvex_deepseek_v4_attention_class attention_class;
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
    unsigned long long deduplicated_entries;
    unsigned long long payload_bytes_read;
    unsigned long long state_raw_entries;
    unsigned long long state_compressed_entries;
    unsigned long long state_indexer_entries;
    unsigned long long reference_comparisons;
    unsigned long long cuda_kernel_launches;
    unsigned long long cuda_peak_device_bytes;
    double q_projection_checksum;
    double kv_projection_checksum;
    double rope_checksum;
    double attention_checksum;
    double output_checksum;
    char output_identity[YVEX_DEEPSEEK_ATTENTION_IDENTITY_CAP];
    double max_abs_error;
    double max_relative_error;
    double rmse;
} yvex_deepseek_attention_cpu_result;

const char *yvex_deepseek_attention_status_name(
    yvex_deepseek_attention_status status);
const char *yvex_deepseek_attention_failure_name(
    yvex_deepseek_attention_failure_code code);
int yvex_deepseek_attention_execute_supported(const char **reason);

int yvex_deepseek_attention_plan_build(
    yvex_deepseek_attention_plan **out,
    const yvex_deepseek_v4_ir *ir,
    const yvex_materialization_session *session,
    const yvex_runtime_descriptor *descriptor,
    yvex_deepseek_attention_failure *failure,
    yvex_error *err);
void yvex_deepseek_attention_plan_close(yvex_deepseek_attention_plan *plan);
const yvex_deepseek_attention_summary *yvex_deepseek_attention_plan_summary(
    const yvex_deepseek_attention_plan *plan);
unsigned long long yvex_deepseek_attention_plan_layer_count(
    const yvex_deepseek_attention_plan *plan);
const yvex_deepseek_attention_layer_plan *yvex_deepseek_attention_plan_layer_at(
    const yvex_deepseek_attention_plan *plan,
    unsigned long long index);

int yvex_deepseek_attention_history_validate(
    const yvex_deepseek_attention_layer_plan *layer,
    const yvex_deepseek_attention_history_view *history,
    yvex_deepseek_attention_failure *failure,
    yvex_error *err);
int yvex_deepseek_attention_state_delta_begin(
    const yvex_deepseek_attention_layer_plan *layer,
    unsigned long long token_position,
    yvex_deepseek_attention_state_delta *delta,
    yvex_deepseek_attention_failure *failure,
    yvex_error *err);
int yvex_deepseek_attention_state_delta_commit(
    yvex_deepseek_attention_state_delta *delta,
    yvex_deepseek_attention_failure *failure,
    yvex_error *err);
int yvex_deepseek_attention_state_delta_abort(
    yvex_deepseek_attention_state_delta *delta,
    yvex_deepseek_attention_failure *failure,
    yvex_error *err);
int yvex_deepseek_attention_rolling_state_validate(
    const yvex_deepseek_attention_layer_plan *layer,
    const yvex_deepseek_attention_rolling_state_view *state,
    yvex_deepseek_attention_rolling_kind kind,
    yvex_deepseek_attention_failure *failure,
    yvex_error *err);
int yvex_deepseek_attention_rolling_state_step_cpu(
    const yvex_deepseek_attention_layer_plan *layer,
    const yvex_deepseek_attention_rolling_state_view *before,
    const float *token_kv,
    const float *token_score,
    const float *ape_row,
    yvex_deepseek_attention_rolling_state_output *after,
    float *compressed_out,
    unsigned long long compressed_out_count,
    int *emitted,
    yvex_deepseek_attention_failure *failure,
    yvex_error *err);

void yvex_deepseek_attention_memory_sink_options_default(
    yvex_deepseek_attention_memory_sink_options *options);
int yvex_deepseek_attention_memory_sink_init(
    yvex_deepseek_attention_memory_sink *sink,
    const yvex_deepseek_attention_memory_sink_options *options,
    yvex_deepseek_attention_failure *failure,
    yvex_error *err);
void yvex_deepseek_attention_memory_sink_release(
    yvex_deepseek_attention_memory_sink *sink);
int yvex_deepseek_attention_state_transaction_begin(
    yvex_deepseek_attention_memory_sink *sink,
    const yvex_deepseek_attention_layer_plan *layer,
    const yvex_deepseek_attention_history_view *history,
    unsigned long long token_position,
    unsigned long long token_count,
    yvex_deepseek_attention_state_transaction *transaction,
    yvex_deepseek_attention_failure *failure,
    yvex_error *err);
int yvex_deepseek_attention_state_transaction_acquire(
    yvex_deepseek_attention_state_transaction *transaction,
    yvex_deepseek_attention_component_kind kind,
    yvex_deepseek_attention_component_span *out,
    yvex_deepseek_attention_failure *failure,
    yvex_error *err);
int yvex_deepseek_attention_state_transaction_write(
    yvex_deepseek_attention_state_transaction *transaction,
    yvex_deepseek_attention_component_kind kind,
    const void *bytes,
    size_t byte_count,
    yvex_deepseek_attention_failure *failure,
    yvex_error *err);
int yvex_deepseek_attention_state_transaction_seal(
    yvex_deepseek_attention_state_transaction *transaction,
    yvex_deepseek_attention_component_kind kind,
    unsigned long long produced_elements,
    yvex_deepseek_attention_failure *failure,
    yvex_error *err);
int yvex_deepseek_attention_state_transaction_commit(
    yvex_deepseek_attention_state_transaction *transaction,
    yvex_deepseek_attention_failure *failure,
    yvex_error *err);
int yvex_deepseek_attention_state_transaction_abort(
    yvex_deepseek_attention_state_transaction *transaction,
    yvex_deepseek_attention_failure *failure,
    yvex_error *err);
const yvex_deepseek_attention_component_span *
yvex_deepseek_attention_memory_sink_committed_component(
    const yvex_deepseek_attention_memory_sink *sink,
    yvex_deepseek_attention_component_kind kind);
const char *yvex_deepseek_attention_memory_sink_identity(
    const yvex_deepseek_attention_memory_sink *sink);

void yvex_deepseek_attention_cpu_options_default(
    yvex_deepseek_attention_cpu_options *options);
void yvex_deepseek_attention_execution_trace_release(
    yvex_deepseek_attention_execution_trace *trace);
int yvex_deepseek_attention_cpu_probe_execute(
    const yvex_deepseek_attention_plan *plan,
    const yvex_deepseek_v4_ir *ir,
    yvex_materialization_session *session,
    const yvex_runtime_descriptor *descriptor,
    const yvex_deepseek_attention_cpu_options *options,
    yvex_deepseek_attention_cpu_result *result,
    yvex_deepseek_attention_failure *failure,
    yvex_error *err);
int yvex_deepseek_attention_cpu_first_token_execute(
    const yvex_deepseek_attention_plan *plan,
    const yvex_deepseek_v4_ir *ir,
    yvex_materialization_session *session,
    const yvex_runtime_descriptor *descriptor,
    const yvex_deepseek_attention_cpu_options *options,
    yvex_deepseek_attention_cpu_result *result,
    yvex_deepseek_attention_failure *failure,
    yvex_error *err);
int yvex_deepseek_attention_cuda_token_execute(
    const yvex_deepseek_attention_plan *plan,
    const yvex_deepseek_v4_ir *ir,
    yvex_materialization_session *session,
    const yvex_runtime_descriptor *descriptor,
    yvex_backend *backend,
    const yvex_deepseek_attention_cpu_options *options,
    yvex_deepseek_attention_cpu_result *result,
    yvex_deepseek_attention_failure *failure,
    yvex_error *err);
int yvex_deepseek_attention_cpu_chunk_execute(
    const yvex_deepseek_attention_plan *plan,
    const yvex_deepseek_v4_ir *ir,
    yvex_materialization_session *session,
    const yvex_runtime_descriptor *descriptor,
    const yvex_deepseek_attention_cpu_options *options,
    yvex_deepseek_attention_cpu_result *result,
    yvex_deepseek_attention_failure *failure,
    yvex_error *err);

#endif
