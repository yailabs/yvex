/* Owner: graph private ABI.
 * Owns: graph state, reports, attention protocols, family composition, and shared algorithms.
 * Does not own: CLI policy, backend kernels, persistent KV, generation, or capability promotion.
 * Invariants: declarations are graph-local and preserve immutable identity and transaction boundaries.
 * Boundary: this header is never installed or included outside graph production and focused tests.
 * Purpose: share the minimum typed contracts required by independent graph translation units.
 * Inputs: public and admitted internal domain types.
 * Effects: declarations only; owning translation units define mutation, I/O, and cleanup.
 * Failure: typed graph and attention failures remain authoritative. */
#ifndef YVEX_GRAPH_PRIVATE_H
#define YVEX_GRAPH_PRIVATE_H
#include <stddef.h>
#include <yvex/artifact.h>
#include <yvex/backend.h>
#include <yvex/runtime.h>
#include <yvex/core.h>
#include <yvex/gguf.h>
#include <yvex/graph.h>
#include <yvex/model.h>
#include <yvex/registry.h>
#include <yvex/generation.h>
#include <yvex/tokenizer.h>
#include <yvex/internal/core.h>
#include <yvex/internal/artifact.h>
#include <yvex/internal/graph.h>
#include <yvex/internal/quant_numeric.h>
#include <yvex/internal/runtime.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef enum {
    YVEX_GRAPH_REPORT_KIND_GRAPH = 0, YVEX_GRAPH_REPORT_KIND_MEMORY_PLAN, YVEX_GRAPH_REPORT_KIND_PLAN,
    YVEX_GRAPH_REPORT_KIND_PRIMITIVE, YVEX_GRAPH_REPORT_KIND_GUARD
} yvex_graph_report_kind;
typedef enum {
    YVEX_GRAPH_REPORT_MODE_NORMAL = 0, YVEX_GRAPH_REPORT_MODE_TABLE, YVEX_GRAPH_REPORT_MODE_AUDIT
} yvex_graph_report_mode;
typedef enum {
    YVEX_GRAPH_ACTION_DUMP = 0, YVEX_GRAPH_ACTION_CHECK, YVEX_GRAPH_ACTION_EXECUTE_FIXTURE,
    YVEX_GRAPH_ACTION_EXECUTE_PARTIAL, YVEX_GRAPH_ACTION_EXECUTE_SEGMENT, YVEX_GRAPH_ACTION_EXECUTE_OP,
    YVEX_GRAPH_ACTION_EXECUTE_BLOCK, YVEX_GRAPH_ACTION_EXECUTE_LAYERS
} yvex_graph_action;
typedef struct yvex_graph_report_request {
    yvex_graph_report_kind kind;
    yvex_graph_action action;
    yvex_graph_report_mode mode;
    const char *model, *backend, *segment, *tokens_text, *op, *block, *suite, *activation;
    unsigned long long sequence_length, context_length, fixture_token, partial_token, token_index, position,
        head_dim, seq_len, m, k, n, hidden_dim, ffn_dim, experts, expert_id, layers;
    int execute_fixture, execute_partial, execute_segment, execute_op, execute_block, execute_layers, causal, gated,
        use_expert, tokens_seen, fixture_token_seen, partial_token_seen, token_index_seen;
} yvex_graph_report_request;
typedef struct yvex_graph_report {
    yvex_graph_report_kind kind;
    const char *status, *graph_status, *architecture, *model_name, *backend, *backend_status;
    int backend_capability_tensor_alloc, backend_capability_tensor_read_write, backend_capability_op_embed,
        backend_capability_op_matmul, backend_capability_op_mlp, backend_capability_op_rms_norm,
        backend_capability_op_rope, backend_capability_op_attention;
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
int yvex_graph_report_build(const yvex_graph_report_request *request, yvex_graph_report *report, yvex_error *err);
int yvex_graph_primitive_report_build(const yvex_graph_report_request *request, yvex_graph_report *report,
    yvex_error *err);
typedef struct {
    unsigned int input_ids[4], output_ids[4];
} yvex_graph_op_edges;
struct yvex_graph {
    yvex_graph_status status;
    char *architecture, *model_name;
    unsigned long long sequence_length, context_length;
    yvex_graph_value_info *values;
    unsigned long long value_count, value_cap;
    yvex_graph_op_info *ops;
    yvex_graph_op_edges *edges;
    unsigned long long op_count, op_cap;
    yvex_graph_missing_required *missing;
    unsigned long long missing_count, missing_cap;
};
struct yvex_memory_plan {
    yvex_memory_plan_status status;
    yvex_memory_plan_summary summary;
};
struct yvex_plan {
    char *backend_name, *backend_status;
    int backend_tensor_alloc, backend_tensor_read_write, backend_op_embed, backend_op_matmul, backend_op_mlp,
        backend_op_rms_norm, backend_op_rope, backend_op_attention;
    yvex_graph *graph;
    yvex_memory_plan *memory;
};

const yvex_graph_op_edges *yvex_graph_op_edges_at(const yvex_graph *graph, unsigned long long index);
yvex_backend_kind yvex_graph_backend_kind_from_name(const char *name);
int yvex_graph_exit_for_status(int status);
typedef enum {
    YVEX_DEEPSEEK_ATTENTION_COMPONENT_ATTENTION_OUTPUT = 0, YVEX_DEEPSEEK_ATTENTION_COMPONENT_RAW_LOCAL_KV,
    YVEX_DEEPSEEK_ATTENTION_COMPONENT_COMPRESSED_MAIN_KV, YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_KV,
    YVEX_DEEPSEEK_ATTENTION_COMPONENT_MAIN_KV_STATE, YVEX_DEEPSEEK_ATTENTION_COMPONENT_MAIN_SCORE_STATE,
    YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_KV_STATE, YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_SCORE_STATE,
    YVEX_DEEPSEEK_ATTENTION_COMPONENT_COUNT
} yvex_attention_component_kind;
typedef enum {
    YVEX_DEEPSEEK_ATTENTION_COMPONENT_STORAGE_F32 = 1
} yvex_attention_component_storage;
typedef enum {
    YVEX_DEEPSEEK_ATTENTION_TRANSACTION_EMPTY = 0, YVEX_DEEPSEEK_ATTENTION_TRANSACTION_BEGUN,
    YVEX_DEEPSEEK_ATTENTION_TRANSACTION_ABORTED, YVEX_DEEPSEEK_ATTENTION_TRANSACTION_COMMITTED
} yvex_attention_transaction_status;
typedef struct {
    yvex_attention_component_kind kind;
    yvex_attention_component_storage storage;
    unsigned int rank;
    unsigned long long dims[4], stride, expected_elements, produced_elements, byte_extent, position_start,
        position_count;
    void *data;
    int required, acquired, written, sealed;
} yvex_attention_component_span;
typedef struct {
    yvex_attention_component_kind fail_acquire_kind;
    yvex_attention_component_kind fail_seal_kind;
    int fail_begin, fail_commit, fail_abort;
} yvex_attention_memory_sink_options;
typedef struct {
    int initialized, has_committed;
    yvex_attention_memory_sink_options options;
    yvex_attention_component_span committed[
        YVEX_DEEPSEEK_ATTENTION_COMPONENT_COUNT];
    char committed_identity[YVEX_DEEPSEEK_ATTENTION_IDENTITY_CAP];
} yvex_attention_memory_sink;
typedef struct {
    yvex_attention_transaction_status status;
    yvex_attention_memory_sink *sink;
    unsigned long long layer_index;
    yvex_attention_class attention_class;
    unsigned long long token_position, token_count;
    char previous_state_identity[YVEX_DEEPSEEK_ATTENTION_IDENTITY_CAP],
        transaction_identity[YVEX_DEEPSEEK_ATTENTION_IDENTITY_CAP];
    yvex_attention_component_span components[
        YVEX_DEEPSEEK_ATTENTION_COMPONENT_COUNT];
} yvex_attention_state_transaction;
typedef struct {
    unsigned long long limit_bytes;
    size_t live_bytes, peak_bytes;
} yvex_attention_scratch_budget;
typedef struct {
    unsigned char *owned[YVEX_BACKEND_ATTENTION_WEIGHT_COUNT];
    unsigned long long payload_bytes_read;
} attention_cuda_weights;
int yvex_attention_execute_supported(const char **reason);
int yvex_attention_cancel_check(const yvex_attention_cancellation *cancellation,
    unsigned long long layer_index, const char *safe_point, yvex_attention_failure *failure, yvex_error *err);
int yvex_attention_class_geometry_validate(const yvex_attention_layer_plan *layer,
    unsigned long long csa_ratio, unsigned long long hca_ratio,
    yvex_attention_failure *failure, yvex_error *err);
int yvex_attention_history_validate(const yvex_attention_layer_plan *layer,
    const yvex_attention_history_view *history, yvex_attention_failure *failure, yvex_error *err);
int yvex_attention_rolling_state_step_cpu(const yvex_attention_layer_plan *layer,
    const yvex_attention_rolling_state_view *before, const float *token_kv, const float *token_score,
    const float *ape_row, yvex_attention_rolling_state_output *after, float *compressed_out,
    unsigned long long compressed_out_count, int *emitted, yvex_attention_failure *failure, yvex_error *err);
int yvex_attention_memory_sink_init(yvex_attention_memory_sink *sink,
    const yvex_attention_memory_sink_options *options, yvex_attention_failure *failure, yvex_error *err);
void yvex_attention_memory_sink_release(yvex_attention_memory_sink *sink);
int yvex_attention_state_transaction_begin(yvex_attention_memory_sink *sink, const yvex_attention_layer_plan *layer,
    const yvex_attention_history_view *history, unsigned long long token_position, unsigned long long token_count,
    yvex_attention_state_transaction *transaction, yvex_attention_failure *failure, yvex_error *err);
int yvex_attention_state_transaction_acquire(yvex_attention_state_transaction *transaction,
    yvex_attention_component_kind kind, yvex_attention_component_span *out, yvex_attention_failure *failure,
    yvex_error *err);
int yvex_attention_state_transaction_seal(yvex_attention_state_transaction *transaction,
    yvex_attention_component_kind kind, unsigned long long produced_elements, yvex_attention_failure *failure,
    yvex_error *err);
int yvex_attention_state_transaction_commit(yvex_attention_state_transaction *transaction,
    yvex_attention_failure *failure, yvex_error *err);
int yvex_attention_state_transaction_abort(yvex_attention_state_transaction *transaction,
    yvex_attention_failure *failure, yvex_error *err);
const yvex_attention_component_span *
yvex_attention_memory_sink_committed_component(const yvex_attention_memory_sink *sink,
    yvex_attention_component_kind kind);
const char *yvex_attention_memory_sink_identity(const yvex_attention_memory_sink *sink);

/* Attention internals share this single graph-private contract. */
struct yvex_attention_plan {
    yvex_attention_layer_plan *layers;
    unsigned long long layer_count;
    yvex_attention_summary summary;
};
int yvex_attention_plan_build(yvex_attention_plan **out, const yvex_attention_recipe *recipe,
    const yvex_materialization_session *session, const yvex_runtime_descriptor *descriptor,
    yvex_attention_failure *failure, yvex_error *err);
void yvex_attention_plan_close(yvex_attention_plan *plan);
const yvex_attention_summary *yvex_attention_plan_summary(const yvex_attention_plan *plan);
unsigned long long yvex_attention_plan_layer_count(const yvex_attention_plan *plan);
const yvex_attention_layer_plan *yvex_attention_plan_layer_at(const yvex_attention_plan *plan,
    unsigned long long index);
int yvex_attention_reject(yvex_attention_failure *failure, yvex_attention_failure_code code,
    const yvex_runtime_tensor_binding *binding, unsigned long long layer_index, yvex_tensor_role role,
    unsigned long long expected, unsigned long long actual, yvex_error *err, yvex_status err_code,
    const char *reason);
int yvex_attention_accept(yvex_attention_failure *failure, yvex_error *err);
int yvex_attention_hash_u64(yvex_sha256 *hash, unsigned long long value);
int yvex_attention_hash_text(yvex_sha256 *hash, const char *text);
int yvex_attention_checked_size(unsigned long long count, unsigned long long width, size_t *out);
unsigned long long yvex_attention_min_u64(unsigned long long a, unsigned long long b);
void *yvex_attention_calloc_array(unsigned long long count, unsigned long long width);
int yvex_attention_scratch_reserve(yvex_attention_scratch_budget *budget,
    unsigned long long count, size_t element_size, size_t *bytes_out);
void yvex_attention_scratch_release(yvex_attention_scratch_budget *budget,
    size_t bytes);
int yvex_attention_context_validate(const yvex_attention_plan *plan, const char *logical_identity,
    const yvex_materialization_session *session, const yvex_runtime_descriptor *descriptor,
    yvex_attention_failure *failure, yvex_error *err);
void yvex_attention_result_reset(yvex_attention_cpu_result *result);
const yvex_runtime_tensor_binding *yvex_attention_binding_find(const yvex_runtime_descriptor *descriptor,
    yvex_tensor_role role, unsigned long long layer_index);
int yvex_attention_row_geometry(const yvex_materialized_tensor_binding *binding,
    unsigned long long *row_bytes, unsigned long long *row_count,
    yvex_attention_failure *failure, yvex_error *err);
int yvex_attention_payload_account(yvex_attention_cpu_result *result,
    unsigned long long bytes, const yvex_runtime_tensor_binding *binding,
    yvex_attention_failure *failure, yvex_error *err);
int yvex_attention_decode_row(yvex_materialization_session *session, const yvex_runtime_tensor_binding *binding,
    unsigned long long row, float *out, unsigned long long elements, yvex_attention_scratch_budget *scratch,
    yvex_attention_cpu_result *result, yvex_attention_failure *failure, yvex_error *err);
int yvex_attention_dot_batch(yvex_materialization_session *session, const yvex_runtime_tensor_binding *binding,
    unsigned long long start_row, const float *vectors, unsigned long long token_count,
    unsigned long long vector_stride, unsigned long long vector_len, unsigned long long max_rows, float *out,
    unsigned long long output_stride, unsigned long long *rows, yvex_attention_scratch_budget *scratch,
    yvex_attention_cpu_result *result,
    yvex_attention_failure *failure, yvex_error *err);
int yvex_attention_decode_flat(yvex_materialization_session *session, const yvex_runtime_tensor_binding *binding,
    float *out, unsigned long long elements, yvex_attention_scratch_budget *scratch,
    yvex_attention_cpu_result *result, yvex_attention_failure *failure, yvex_error *err);
double yvex_attention_checksum(const float *values, unsigned long long count);
int yvex_attention_rms_norm(float *values, unsigned long long count, const float *weights, double epsilon);
int yvex_attention_unit_rms_norm(float *values, unsigned long long count, double epsilon);
int yvex_attention_compute_round(yvex_attention_compute_contract contract,
    float *values, unsigned long long count);
int yvex_attention_rope_apply(float *values, unsigned long long width, unsigned long long rope_width,
    unsigned long long position, const yvex_attention_position_policy *position_spec, int inverse);
int yvex_attention_activation_apply(const yvex_attention_activation_policy *policy, float *values,
    unsigned long long count, unsigned long long layer_index, yvex_tensor_role role,
    yvex_attention_scratch_budget *scratch, yvex_attention_failure *failure, yvex_error *err);
const float *yvex_attention_segment_row(const float *history, unsigned long long history_count,
    unsigned long long history_stride, const float *current, unsigned long long current_count,
    unsigned long long current_stride, unsigned long long index);
unsigned long long yvex_attention_segment_position(const unsigned long long *history,
    unsigned long long history_count, const unsigned long long *current, unsigned long long current_count,
    unsigned long long index);
int yvex_attention_csa_select(const yvex_attention_layer_plan *layer, const yvex_attention_history_view *history,
    const float *current_indexer, unsigned long long current_count, unsigned long long current_stride,
    const unsigned long long *current_positions, const float *query, const float *weights,
    unsigned long long absolute, unsigned long long *selected, unsigned long long *selected_count,
    unsigned long long *valid_count, yvex_attention_scratch_budget *scratch,
    yvex_attention_failure *failure, yvex_error *err);
int yvex_attention_reduce_chunk(const yvex_attention_layer_plan *layer,
    const float *query, const yvex_attention_history_view *history, const float *current_kv,
    unsigned long long current_kv_stride, const float *current_compressed,
    unsigned long long current_compressed_count, unsigned long long current_compressed_stride,
    const unsigned long long *current_compressed_positions, const float *current_indexer,
    unsigned long long current_indexer_count, unsigned long long current_indexer_stride,
    const unsigned long long *current_indexer_positions, const float *index_query,
    unsigned long long index_query_stride, const float *index_weights, unsigned long long index_weight_stride,
    const float *sinks, unsigned long long token_count, unsigned long long token_position, float *out,
    unsigned long long *trace_topk_counts, unsigned long long *trace_topk_positions,
    unsigned long long trace_topk_stride, yvex_attention_scratch_budget *scratch,
    yvex_attention_cpu_result *result, yvex_attention_failure *failure, yvex_error *err);
int yvex_attention_output_project(yvex_materialization_session *session, const yvex_runtime_tensor_binding *out_a,
    const yvex_runtime_tensor_binding *out_b, const float *values, unsigned long long token_count,
    unsigned long long value_stride, unsigned long long groups, unsigned long long group_width,
    unsigned long long rank, unsigned long long hidden_width,
    yvex_attention_compute_contract compute_contract, float *out,
    unsigned long long output_stride,
    yvex_attention_scratch_budget *scratch, yvex_attention_cpu_result *result,
    yvex_attention_failure *failure, yvex_error *err);
int yvex_attention_rolling_storage_allocate(const yvex_attention_layer_plan *layer,
    yvex_attention_rolling_kind kind, unsigned long long token_position, float **kv, float **score,
    yvex_attention_rolling_state_view *view, yvex_attention_failure *failure, yvex_error *err);
int yvex_attention_rolling_geometry(const yvex_attention_layer_plan *layer, yvex_attention_rolling_kind kind,
    unsigned long long *ratio, unsigned long long *head_dimension, unsigned long long *state_width,
    unsigned long long *state_slots, int *overlap, int *rotated);
void yvex_attention_rolling_output_bind(const yvex_attention_rolling_state_output *output,
    yvex_attention_rolling_state_view *view);
void yvex_attention_execution_trace_release(yvex_attention_execution_trace *trace);
int yvex_attention_trace_capture(yvex_attention_execution_trace *trace, unsigned long long layer_index,
    yvex_attention_class attention_class, unsigned long long token_position,
    unsigned long long token_count, unsigned long long hidden_width, unsigned long long q_rank,
    unsigned long long query_width, unsigned long long kv_width, const float *input, const float *q_low,
    const float *query, const float *raw_kv, const float *compressed_kv, unsigned long long compressed_count,
    unsigned long long compressed_stride, const unsigned long long *compressed_positions, const float *indexer_kv,
    unsigned long long indexer_count, unsigned long long indexer_stride, const unsigned long long *indexer_positions,
    const float *index_query, unsigned long long index_query_stride, const float *index_weights,
    unsigned long long index_weight_stride, const float *attention_values, const float *output,
    const unsigned long long *topk_counts, const unsigned long long *topk_positions, unsigned long long topk_stride,
    const yvex_attention_rolling_state_output *main_state, const float *main_kv, const float *main_score,
    const yvex_attention_rolling_state_output *index_state, const float *index_kv, const float *index_score);
void yvex_attention_cuda_weights_release(attention_cuda_weights *weights);
int yvex_attention_cuda_role_load(yvex_materialization_session *session, const yvex_runtime_descriptor *descriptor,
    unsigned long long layer_index, yvex_tensor_role role, yvex_backend_attention_weight_slot slot,
    attention_cuda_weights *owned, yvex_backend_attention_job *job, yvex_attention_failure *failure,
    yvex_error *err);
void yvex_attention_cuda_activation_project(const yvex_attention_activation_policy *source,
    yvex_backend_attention_activation *out);
int yvex_attention_cuda_rolling_project(const yvex_attention_rolling_state_view *source,
    yvex_backend_attention_rolling *out);
int yvex_attention_cuda_trace_allocate(yvex_attention_execution_trace *trace,
    const yvex_attention_layer_plan *layer, const yvex_attention_history_view *history,
    unsigned long long token_position, const float *input, unsigned long long limit_bytes,
    unsigned long long *owned_bytes, yvex_attention_failure *failure, yvex_error *err);
void yvex_attention_cuda_rolling_commit(const yvex_attention_rolling_state_view *before,
    unsigned long long token_position, yvex_attention_rolling_state_output *after);
double yvex_attention_cuda_checksum(const float *values, unsigned long long count);
int yvex_attention_cuda_output_identity(const yvex_attention_plan *plan,
    const yvex_attention_execution_trace *trace, char out[YVEX_DEEPSEEK_ATTENTION_IDENTITY_CAP]);
int yvex_attention_hadamard_cpu(const float *input, unsigned long long length, float scale, int reject_nonfinite,
    float *output, yvex_attention_scratch_budget *scratch, yvex_attention_failure *failure, yvex_error *err);
int yvex_attention_topk_select(const float *scores, const unsigned long long *ordinals,
    unsigned long long candidate_count, unsigned long long k, unsigned long long *selected_indices,
    unsigned long long *selected_count, yvex_attention_failure *failure, yvex_error *err);
unsigned char yvex_attention_fp4_e2m1_encode(float value);
float yvex_attention_fp4_e2m1_decode(unsigned char code);
unsigned char yvex_attention_fp8_e4m3fn_encode(float value);
float yvex_attention_fp8_e4m3fn_decode(unsigned char code);
int yvex_attention_fp8_fake_quant_block(const float *input, unsigned long long count, float *dequantized,
    unsigned char *codes, unsigned char *scale_code, yvex_attention_failure *failure, yvex_error *err);
int yvex_attention_fp4_fake_quant_block(const float *input, unsigned long long count, float *dequantized,
    unsigned char *codes, unsigned char *scale_code, yvex_attention_failure *failure, yvex_error *err);

#endif
