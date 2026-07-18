/*
 * private.h - graph-domain private structures.
 *
 * Owner:
 *   src/graph
 *
 * Owns:
 *   private graph, memory-plan, and plan state shared by graph-domain files.
 *
 * Does not own:
 *   CLI input parsing, command dispatch, rendering, stdout/stderr output,
 *   runtime generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   private structures are shared only inside graph-domain implementation
 *   files; no CLI/operator headers are included here.
 *
 * Boundary:
 *   graph private state is not graph execution or generation readiness.
 */
#ifndef YVEX_GRAPH_PRIVATE_H
#define YVEX_GRAPH_PRIVATE_H

#include <stddef.h>
#include <yvex/artifact.h>
#include <yvex/backend.h>
#include <yvex/engine.h>
#include <yvex/error.h>
#include <yvex/gguf.h>
#include <yvex/graph.h>
#include <yvex/memory_plan.h>
#include <yvex/model.h>
#include <yvex/op.h>
#include <yvex/planner.h>
#include <yvex/tensor.h>
#include <yvex/token_input.h>
#include <yvex/tokenizer.h>
#include <yvex/api.h>

typedef struct {
    unsigned int input_ids[4];
    unsigned int output_ids[4];
} yvex_graph_op_edges;

struct yvex_graph {
    yvex_graph_status status;
    char *architecture;
    char *model_name;
    unsigned long long sequence_length;
    unsigned long long context_length;
    yvex_graph_value_info *values;
    unsigned long long value_count;
    unsigned long long value_cap;
    yvex_graph_op_info *ops;
    yvex_graph_op_edges *edges;
    unsigned long long op_count;
    unsigned long long op_cap;
    yvex_graph_missing_required *missing;
    unsigned long long missing_count;
    unsigned long long missing_cap;
};

struct yvex_memory_plan {
    yvex_memory_plan_status status;
    yvex_memory_plan_summary summary;
};

struct yvex_plan {
    char *backend_name;
    char *backend_status;
    int backend_tensor_alloc;
    int backend_tensor_read_write;
    int backend_op_embed;
    int backend_op_matmul;
    int backend_op_mlp;
    int backend_op_rms_norm;
    int backend_op_rope;
    int backend_op_attention;
    yvex_graph *graph;
    yvex_memory_plan *memory;
};

typedef struct {
    const char *guard_status;
    const char *phase;
    const char *graph_kind;
    const char *integrity_status;
    const char *identity_status;
    const char *metadata_status;
    const char *shape_status;
    const char *range_status;
    const char *slice_range_status;
    const char *backend_status;
    const char *backend_op_status;
    int dispatch_attempted;
    int reference_read_attempted;
    int output_allocation_attempted;
    int cleanup_attempted;
    const char *cleanup_status;
    unsigned long long output_bytes_planned;
    unsigned long long output_bytes_allocated;
    unsigned long long reference_bytes_planned;
} yvex_cli_graph_guard_report;

typedef struct {
    const char *backend_name;
    unsigned long long layers;
    unsigned long long seq_len;
    unsigned long long position;
    unsigned long long hidden_dim;
    unsigned long long head_dim;
    unsigned long long ffn_dim;
    const float *initial_position_values;
    unsigned long long initial_position_value_count;
} yvex_cli_layer_fixture_options;

typedef struct {
    int executed;
    const char *status;
    const char *graph_integrity_guard;
    const char *graph_execution_phase;
    const char *backend_status;
    const char *backend_op_status;
    unsigned long long layers;
    unsigned long long total_op_count;
    unsigned long long output_bytes;
    unsigned long long scratch_bytes;
    unsigned long long final_output_checksum;
    unsigned long long final_reference_checksum;
    double final_max_abs_diff;
    unsigned long long output_value_count;
    float output_values[YVEX_SEGMENT_GRAPH_MAX_OUTPUT_VALUES];
    int cleanup_attempted;
    const char *cleanup_status;
} yvex_cli_layer_fixture_result;

char *yvex_graph_strdup(const char *text);
void yvex_graph_value_clear(yvex_graph_value_info *value);
void yvex_graph_op_clear(yvex_graph_op_info *op);
void yvex_graph_missing_clear(yvex_graph_missing_required *missing);

int yvex_graph_add_value(yvex_graph *graph,
                         yvex_value_kind kind,
                         const char *name,
                         unsigned int rank,
                         const unsigned long long *dims,
                         yvex_dtype dtype,
                         yvex_residency residency,
                         const char *source_tensor_name,
                         unsigned int *out_id,
                         yvex_error *err);
int yvex_graph_add_op(yvex_graph *graph,
                      yvex_op_kind kind,
                      yvex_op_status status,
                      const char *name,
                      const unsigned int *inputs,
                      unsigned int input_count,
                      const unsigned int *outputs,
                      unsigned int output_count,
                      const char *reason,
                      yvex_error *err);
int yvex_graph_add_missing(yvex_graph *graph,
                           yvex_tensor_role role,
                           const char *reason,
                           yvex_error *err);

const yvex_graph_op_edges *yvex_graph_op_edges_at(const yvex_graph *graph,
                                                  unsigned long long index);
int yvex_graph_backend_valid(const char *name);
yvex_backend_kind yvex_graph_backend_kind_from_name(const char *name);
int yvex_graph_exit_for_status(int status);

void print_token_input_summary(const yvex_token_input *input,
                               const char *status,
                               const char *bounds_status,
                               unsigned long long selected_index,
                               unsigned int selected_token,
                               int selected_seen);

void yvex_graph_guard_report_init(yvex_cli_graph_guard_report *report);
int yvex_graph_preflight(const yvex_model_ref *model_ref,
                          const char *backend_name,
                          int execute_fixture,
                          int execute_segment,
                          unsigned int requested_token,
                          yvex_cli_graph_guard_report *report,
                          yvex_error *err);
int yvex_cli_graph_execute_layer_fixture(const yvex_cli_layer_fixture_options *options,
                                         yvex_cli_layer_fixture_result *out,
                                         yvex_error *err);


#include "src/artifact/materialize.h"
#include "src/model/families.h"
#include "src/model/runtime_descriptor.h"


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
    YVEX_DEEPSEEK_ATTENTION_FAILURE_BACKEND
} yvex_attention_failure_code;

typedef enum {
    YVEX_DEEPSEEK_ATTENTION_DELTA_EMPTY = 0,
    YVEX_DEEPSEEK_ATTENTION_DELTA_PENDING,
    YVEX_DEEPSEEK_ATTENTION_DELTA_COMMITTED,
    YVEX_DEEPSEEK_ATTENTION_DELTA_ABORTED
} yvex_attention_delta_status;

typedef enum {
    YVEX_DEEPSEEK_ATTENTION_ROLLING_NONE = 0,
    YVEX_DEEPSEEK_ATTENTION_ROLLING_MAIN,
    YVEX_DEEPSEEK_ATTENTION_ROLLING_INDEXER
} yvex_attention_rolling_kind;

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
} yvex_attention_component_kind;

typedef enum {
    YVEX_DEEPSEEK_ATTENTION_COMPONENT_STORAGE_F32 = 1
} yvex_attention_component_storage;

typedef enum {
    YVEX_DEEPSEEK_ATTENTION_TRANSACTION_EMPTY = 0,
    YVEX_DEEPSEEK_ATTENTION_TRANSACTION_BEGUN,
    YVEX_DEEPSEEK_ATTENTION_TRANSACTION_ABORTED,
    YVEX_DEEPSEEK_ATTENTION_TRANSACTION_COMMITTED
} yvex_attention_transaction_status;

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
} yvex_attention_rolling_state_view;

typedef struct {
    int present;
    unsigned int schema_version;
    yvex_attention_rolling_kind kind;
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
    yvex_attention_rolling_state_output next_main_rolling_state;
    yvex_attention_rolling_state_output next_indexer_rolling_state;
} yvex_attention_execution_trace;

typedef struct {
    yvex_attention_delta_status status;
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
    yvex_attention_rolling_state_output next_main_rolling_state;
    yvex_attention_rolling_state_output next_indexer_rolling_state;
    int published;
} yvex_attention_state_delta;

typedef struct {
    yvex_attention_component_kind kind;
    yvex_attention_component_storage storage;
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
} yvex_attention_component_span;

typedef struct {
    yvex_attention_component_kind fail_acquire_kind;
    yvex_attention_component_kind fail_seal_kind;
    int fail_begin;
    int fail_commit;
    int fail_abort;
} yvex_attention_memory_sink_options;

typedef struct {
    int initialized;
    int has_committed;
    yvex_attention_memory_sink_options options;
    yvex_attention_component_span committed[
        YVEX_DEEPSEEK_ATTENTION_COMPONENT_COUNT];
    char committed_identity[YVEX_DEEPSEEK_ATTENTION_IDENTITY_CAP];
} yvex_attention_memory_sink;

typedef struct {
    yvex_attention_transaction_status status;
    yvex_attention_memory_sink *sink;
    unsigned long long layer_index;
    yvex_deepseek_v4_attention_class attention_class;
    unsigned long long token_position;
    unsigned long long token_count;
    char previous_state_identity[YVEX_DEEPSEEK_ATTENTION_IDENTITY_CAP];
    char transaction_identity[YVEX_DEEPSEEK_ATTENTION_IDENTITY_CAP];
    yvex_attention_component_span components[
        YVEX_DEEPSEEK_ATTENTION_COMPONENT_COUNT];
} yvex_attention_state_transaction;

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
} yvex_attention_layer_plan;

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
    int history_contract_ready;
    int state_delta_contract_ready;
    int cpu_reference_ready;
    int cuda_execution_ready;
    int full_execution_ready;
} yvex_attention_summary;

typedef struct yvex_attention_plan yvex_attention_plan;

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
    const yvex_attention_history_view *history;
    yvex_attention_execution_trace *trace;
} yvex_attention_cpu_options;

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
} yvex_attention_cpu_result;

const char *yvex_attention_status_name(
    yvex_attention_status status);
const char *yvex_attention_failure_name(
    yvex_attention_failure_code code);
int yvex_attention_execute_supported(const char **reason);
int yvex_attention_state_delta_begin(
    const yvex_attention_layer_plan *layer,
    unsigned long long token_position,
    yvex_attention_state_delta *delta,
    yvex_attention_failure *failure,
    yvex_error *err);
int yvex_attention_state_delta_commit(
    yvex_attention_state_delta *delta,
    yvex_attention_failure *failure,
    yvex_error *err);
int yvex_attention_state_delta_abort(
    yvex_attention_state_delta *delta,
    yvex_attention_failure *failure,
    yvex_error *err);
void yvex_attention_memory_sink_options_default(
    yvex_attention_memory_sink_options *options);
int yvex_attention_memory_sink_init(
    yvex_attention_memory_sink *sink,
    const yvex_attention_memory_sink_options *options,
    yvex_attention_failure *failure,
    yvex_error *err);
void yvex_attention_memory_sink_release(
    yvex_attention_memory_sink *sink);
int yvex_attention_state_transaction_begin(
    yvex_attention_memory_sink *sink,
    const yvex_attention_layer_plan *layer,
    const yvex_attention_history_view *history,
    unsigned long long token_position,
    unsigned long long token_count,
    yvex_attention_state_transaction *transaction,
    yvex_attention_failure *failure,
    yvex_error *err);
int yvex_attention_state_transaction_acquire(
    yvex_attention_state_transaction *transaction,
    yvex_attention_component_kind kind,
    yvex_attention_component_span *out,
    yvex_attention_failure *failure,
    yvex_error *err);
int yvex_attention_state_transaction_write(
    yvex_attention_state_transaction *transaction,
    yvex_attention_component_kind kind,
    const void *bytes,
    size_t byte_count,
    yvex_attention_failure *failure,
    yvex_error *err);
int yvex_attention_state_transaction_seal(
    yvex_attention_state_transaction *transaction,
    yvex_attention_component_kind kind,
    unsigned long long produced_elements,
    yvex_attention_failure *failure,
    yvex_error *err);
int yvex_attention_state_transaction_commit(
    yvex_attention_state_transaction *transaction,
    yvex_attention_failure *failure,
    yvex_error *err);
int yvex_attention_state_transaction_abort(
    yvex_attention_state_transaction *transaction,
    yvex_attention_failure *failure,
    yvex_error *err);
const yvex_attention_component_span *
yvex_attention_memory_sink_committed_component(
    const yvex_attention_memory_sink *sink,
    yvex_attention_component_kind kind);
const char *yvex_attention_memory_sink_identity(
    const yvex_attention_memory_sink *sink);

typedef struct {
    int (*plan_build)(yvex_attention_plan **out,
                      const yvex_deepseek_v4_ir *ir,
                      const yvex_materialization_session *session,
                      const yvex_runtime_descriptor *descriptor,
                      yvex_attention_failure *failure,
                      yvex_error *err);
    void (*plan_close)(yvex_attention_plan *plan);
    const yvex_attention_summary *(*plan_summary)(
        const yvex_attention_plan *plan);
    unsigned long long (*plan_layer_count)(const yvex_attention_plan *plan);
    const yvex_attention_layer_plan *(*plan_layer_at)(
        const yvex_attention_plan *plan, unsigned long long index);
    int (*history_validate)(const yvex_attention_layer_plan *layer,
                            const yvex_attention_history_view *history,
                            yvex_attention_failure *failure,
                            yvex_error *err);
    int (*rolling_state_validate)(
        const yvex_attention_layer_plan *layer,
        const yvex_attention_rolling_state_view *state,
        yvex_attention_rolling_kind kind,
        yvex_attention_failure *failure,
        yvex_error *err);
    int (*rolling_state_step_cpu)(
        const yvex_attention_layer_plan *layer,
        const yvex_attention_rolling_state_view *before,
        const float *token_kv,
        const float *token_score,
        const float *ape_row,
        yvex_attention_rolling_state_output *after,
        float *compressed_out,
        unsigned long long compressed_out_count,
        int *emitted,
        yvex_attention_failure *failure,
        yvex_error *err);
    void (*cpu_options_default)(yvex_attention_cpu_options *options);
    void (*execution_trace_release)(yvex_attention_execution_trace *trace);
    int (*cpu_probe_execute)(
        const yvex_attention_plan *plan,
        const yvex_deepseek_v4_ir *ir,
        yvex_materialization_session *session,
        const yvex_runtime_descriptor *descriptor,
        const yvex_attention_cpu_options *options,
        yvex_attention_cpu_result *result,
        yvex_attention_failure *failure,
        yvex_error *err);
    int (*cpu_first_token_execute)(
        const yvex_attention_plan *plan,
        const yvex_deepseek_v4_ir *ir,
        yvex_materialization_session *session,
        const yvex_runtime_descriptor *descriptor,
        const yvex_attention_cpu_options *options,
        yvex_attention_cpu_result *result,
        yvex_attention_failure *failure,
        yvex_error *err);
    int (*cuda_token_execute)(
        const yvex_attention_plan *plan,
        const yvex_deepseek_v4_ir *ir,
        yvex_materialization_session *session,
        const yvex_runtime_descriptor *descriptor,
        yvex_backend *backend,
        const yvex_attention_cpu_options *options,
        yvex_attention_cpu_result *result,
        yvex_attention_failure *failure,
        yvex_error *err);
    int (*cpu_chunk_execute)(
        const yvex_attention_plan *plan,
        const yvex_deepseek_v4_ir *ir,
        yvex_materialization_session *session,
        const yvex_runtime_descriptor *descriptor,
        const yvex_attention_cpu_options *options,
        yvex_attention_cpu_result *result,
        yvex_attention_failure *failure,
        yvex_error *err);
} yvex_graph_family_api;

/* Returns the process-lifetime immutable DeepSeek graph recipe. */
const yvex_graph_family_api *yvex_graph_lower_deepseek_v4(void);


/* Attention internals share this single graph-private contract. */
#include "src/core/sha256.h"
#include "src/gguf/quant_numeric.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct yvex_attention_plan {
    yvex_attention_layer_plan *layers;
    unsigned long long layer_count;
    yvex_attention_summary summary;
};

int yvex_attention_reject(yvex_attention_failure *failure,
                     yvex_attention_failure_code code,
                     const yvex_runtime_tensor_binding *binding,
                     unsigned long long layer_index,
                     yvex_tensor_role role,
                     unsigned long long expected,
                     unsigned long long actual,
                     yvex_error *err,
                     yvex_status err_code,
                     const char *reason);
int yvex_attention_hash_u64(yvex_sha256 *hash, unsigned long long value);
int yvex_attention_hash_text(yvex_sha256 *hash, const char *text);
int yvex_attention_checked_mul_u64(unsigned long long left,
                              unsigned long long right,
                              unsigned long long *out);
int yvex_attention_checked_add_u64(unsigned long long left,
                              unsigned long long right,
                              unsigned long long *out);
int yvex_attention_checked_size(unsigned long long count,
                           unsigned long long width,
                           size_t *out);
unsigned long long yvex_attention_min_u64(unsigned long long a,
                                     unsigned long long b);
void *yvex_attention_calloc_array(unsigned long long count,
                             unsigned long long width);
int yvex_attention_context_validate(
    const yvex_attention_plan *plan,
    const yvex_deepseek_v4_ir *ir,
    const yvex_materialization_session *session,
    const yvex_runtime_descriptor *descriptor,
    yvex_attention_failure *failure,
    yvex_error *err);
int yvex_attention_hadamard_cpu(
    const float *input,
    unsigned long long length,
    float scale,
    int reject_nonfinite,
    float *output,
    yvex_attention_failure *failure,
    yvex_error *err);
int yvex_attention_topk_select(
    const float *scores,
    const unsigned long long *ordinals,
    unsigned long long candidate_count,
    unsigned long long k,
    unsigned long long *selected_indices,
    unsigned long long *selected_count,
    yvex_attention_failure *failure,
    yvex_error *err);
unsigned char yvex_attention_ue8m0_encode_scale(float value);
float yvex_attention_ue8m0_decode_scale(unsigned char code);
unsigned char yvex_attention_fp4_e2m1_encode(float value);
float yvex_attention_fp4_e2m1_decode(unsigned char code);
unsigned char yvex_attention_fp8_e4m3fn_encode(float value);
float yvex_attention_fp8_e4m3fn_decode(unsigned char code);
int yvex_attention_fp8_fake_quant_block(
    const float *input,
    unsigned long long count,
    float *dequantized,
    unsigned char *codes,
    unsigned char *scale_code,
    yvex_attention_failure *failure,
    yvex_error *err);
int yvex_attention_fp4_fake_quant_block(
    const float *input,
    unsigned long long count,
    float *dequantized,
    unsigned char *codes,
    unsigned char *scale_code,
    yvex_attention_failure *failure,
    yvex_error *err);

#endif
