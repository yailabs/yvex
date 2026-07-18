/*
 * Owner: abi.engine (abi).
 * Owns: the public-abi boundary consumed by repository.
 * Does not own: unrelated subsystem policy or unsupported higher-stage claims.
 * Invariants: scope=generic and visibility=public match config/source_owners.tsv.
 * Boundary: public-abi; moving this contract requires an ownership-manifest change.
 *
 * YVEX - Engine runtime object
 *
 * File: include/yvex/engine.h
 * Layer: public runtime API
 *
 * Purpose:
 *   Defines the engine/session layer engine object that assembles artifact, GGUF, tensor table,
 *   model descriptor, tokenizer, and optional graph planning state. The
 *   engine is inspectable runtime structure; it does not execute inference.
 *
 * Owns:
 *   - yvex_engine
 *   - engine lifecycle
 *   - engine summary and diagnostics
 *
 * Does not own:
 *   - sessions
 *   - sampler/runtime generation
 *   - server/provider behavior
 *
 * Used by:
 *   - yvex engine
 *   - yvex session
 *   - engine/session layer runtime tests
 *
 * Validation:
 *   - make test-core
 *   - build/tests/test_engine
 */
#ifndef YVEX_ENGINE_H
#define YVEX_ENGINE_H

#include <yvex/graph.h>
#include <yvex/kv.h>
#include <yvex/model.h>
#include <yvex/tensor.h>
#include <yvex/token_input.h>
#include <yvex/tokenizer.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct yvex_engine yvex_engine;

typedef enum {
    YVEX_ENGINE_STATUS_EMPTY = 0,
    YVEX_ENGINE_STATUS_LOADED,
    YVEX_ENGINE_STATUS_PARTIAL,
    YVEX_ENGINE_STATUS_UNSUPPORTED,
    YVEX_ENGINE_STATUS_FAILED
} yvex_engine_status;

typedef struct {
    const char *model_path;
    int load_tokenizer;
    int build_descriptor;
    int build_default_graph;
    int attach_weights;
    const char *backend_name;
    int require_all_weights;
} yvex_engine_options;

typedef struct {
    yvex_engine_status status;
    const char *model_path;
    const char *model_name;
    const char *architecture;
    unsigned long long metadata_count;
    unsigned long long tensor_count;
    unsigned long long known_tensor_bytes;
    unsigned long long unsupported_tensor_accounting;
    const char *tokenizer_model;
    const char *tokenizer_support;
    const char *graph_status;
    int weights_attached;
    const char *weights_backend;
    unsigned long long weight_tensor_count;
    unsigned long long weight_total_bytes;
    unsigned long long weight_backend_allocated_bytes;
    int graph_execution_ready;
} yvex_engine_summary;

#define YVEX_FIXTURE_GRAPH_MAX_OUTPUT_VALUES 16u
#define YVEX_PARTIAL_GRAPH_MAX_OUTPUT_VALUES 16u
#define YVEX_SEGMENT_GRAPH_MAX_OUTPUT_VALUES 16u
#define YVEX_PREFILL_KV_MAX_SAMPLE_VALUES 8u

typedef struct {
    unsigned int token_id;
} yvex_fixture_graph_options;

typedef struct {
    int executed;
    const char *graph_integrity_guard;
    const char *graph_execution_phase;
    const char *graph_kind;
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
    const char *backend_name;
    const char *op_name;
    const char *weight_name;
    unsigned int token_id;
    unsigned long long node_count;
    unsigned long long output_count;
    unsigned long long output_bytes;
    unsigned long long output_checksum;
    unsigned long long output_value_count;
    float output_values[YVEX_FIXTURE_GRAPH_MAX_OUTPUT_VALUES];
    int execution_ready;
    int graph_execution_ready;
} yvex_fixture_graph_result;

typedef struct {
    unsigned int token_id;
} yvex_partial_graph_options;

typedef struct {
    int executed;
    const char *graph_integrity_guard;
    const char *graph_execution_phase;
    const char *graph_kind;
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
    const char *backend_name;
    const char *segment_name;
    const char *weight_name;
    const char *weight_dtype;
    const char *output_dtype;
    unsigned int token_id;
    unsigned long long node_count;
    unsigned long long output_count;
    unsigned long long output_bytes;
    unsigned long long output_checksum;
    unsigned long long reference_checksum;
    double max_abs_diff;
    unsigned long long output_value_count;
    float output_values[YVEX_PARTIAL_GRAPH_MAX_OUTPUT_VALUES];
    int execution_ready;
    int graph_execution_ready;
} yvex_partial_graph_result;

typedef struct {
    unsigned int token_id;
    const char *segment_name;
} yvex_segment_graph_options;

typedef struct {
    int executed;
    const char *graph_integrity_guard;
    const char *graph_execution_phase;
    const char *graph_kind;
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
    const char *backend_name;
    const char *segment_name;
    const char *token_tensor_name;
    const char *token_tensor_dtype;
    const char *rmsnorm_tensor_name;
    const char *rmsnorm_tensor_dtype;
    const char *rmsnorm_epsilon_key;
    double rmsnorm_epsilon;
    unsigned int token_id;
    unsigned long long node_count;
    unsigned long long hidden_size;
    unsigned long long vocab_size;
    unsigned long long segment_ops;
    unsigned long long segment_intermediate_count;
    unsigned long long segment_intermediate_bytes;
    unsigned long long segment_output_count;
    unsigned long long segment_output_bytes;
    unsigned long long segment_scratch_bytes;
    unsigned long long segment_reference_bytes;
    unsigned long long output_checksum;
    unsigned long long reference_checksum;
    double max_abs_diff;
    unsigned long long output_value_count;
    float output_values[YVEX_SEGMENT_GRAPH_MAX_OUTPUT_VALUES];
    int execution_ready;
    int graph_execution_ready;
} yvex_segment_graph_result;

typedef struct {
    const yvex_token_input *token_input;
    const char *segment_name;
    unsigned long long position_start;
    unsigned long long chunk_size;
    unsigned long long context_length;
    int attach_kv;
    yvex_kv_shape kv_shape;
    unsigned long long layer_count;
    unsigned long long layer_hidden_dim;
    unsigned long long layer_head_dim;
    unsigned long long layer_ffn_dim;
} yvex_prefill_state_options;

typedef struct {
    int prefill_state_created;
    const char *prefill_state_kind;
    const char *sequence_execution_mode;
    const char *prefill_phase;
    const char *backend_name;
    const char *segment_name;
    unsigned long long token_count;
    unsigned long long tokens_processed;
    unsigned long long position_start;
    unsigned long long position_end;
    unsigned long long failed_token_index;
    int chunked_prefill_requested;
    const char *chunk_execution_mode;
    unsigned long long chunk_size;
    unsigned long long chunk_count;
    unsigned long long chunks_processed;
    unsigned long long failed_chunk_index;
    unsigned long long current_chunk_start;
    unsigned long long current_chunk_end;
    unsigned long long final_chunk_checksum;
    const char *context_boundary_status;
    unsigned long long context_length;
    const char *prefill_scratch_kind;
    int prefill_scratch_reuse;
    unsigned long long prefill_scratch_allocations;
    unsigned long long prefill_scratch_reuse_count;
    int prefill_scratch_cleanup_attempted;
    const char *prefill_scratch_cleanup_status;
    unsigned long long segment_graph_executions;
    unsigned long long segment_output_count;
    unsigned long long segment_output_bytes;
    unsigned long long total_output_bytes;
    unsigned long long scratch_bytes;
    unsigned long long aggregate_checksum;
    unsigned long long final_token_checksum;
    double max_abs_diff;
    int layer_prefill_requested;
    const char *layer_execution_kind;
    const char *layer_input_projection;
    const char *layer_handoff;
    const char *layer_sequence_rebuild;
    int model_layer_execution;
    unsigned long long layer_count;
    unsigned long long layer_graph_executions;
    unsigned long long layer_block_executions;
    unsigned long long layer_total_op_count;
    unsigned long long layer_output_count;
    unsigned long long layer_output_bytes;
    unsigned long long layer_total_output_bytes;
    unsigned long long layer_total_scratch_bytes;
    unsigned long long layer_final_checksum;
    unsigned long long layer_final_reference_checksum;
    double layer_max_abs_diff;
    unsigned long long layer_output_sample_count;
    float layer_output_sample_values[YVEX_SEGMENT_GRAPH_MAX_OUTPUT_VALUES];
    int cleanup_attempted;
    const char *cleanup_status;
    int cuda_parity;
    int kv_ready;
    int session_kv_owned;
    int kv_bound_to_prefill;
    const char *kv_binding_kind;
    const char *kv_binding_source;
    const char *kv_status;
    const char *kv_owner;
    const char *kv_dtype;
    unsigned long long kv_layers;
    unsigned long long kv_heads;
    unsigned long long kv_head_dim;
    unsigned long long kv_capacity;
    unsigned long long kv_values_per_position;
    unsigned long long kv_bytes_per_position;
    unsigned long long kv_planned_bytes;
    unsigned long long kv_allocated_bytes;
    unsigned long long kv_positions_written;
    unsigned long long kv_append_count;
    unsigned long long kv_read_count;
    unsigned long long kv_read_position;
    unsigned long long kv_read_value_count;
    unsigned long long kv_read_checksum;
    unsigned long long kv_read_sample_count;
    float kv_read_sample_values[YVEX_PREFILL_KV_MAX_SAMPLE_VALUES];
    const char *kv_overflow_status;
    const char *kv_cleanup_status;
    int full_transformer_prefill_ready;
    int decode_ready;
    int logits_ready;
    int generation_ready;
    const char *generation_status;
} yvex_prefill_state_summary;

int yvex_engine_open(yvex_engine **out,
                     const yvex_engine_options *options,
                     yvex_error *err);
int yvex_engine_open_path(yvex_engine **out,
                          const char *model_path,
                          yvex_error *err);
void yvex_engine_close(yvex_engine *engine);

yvex_engine_status yvex_engine_status_of(const yvex_engine *engine);
const char *yvex_engine_status_name(yvex_engine_status status);

const char *yvex_engine_model_path(const yvex_engine *engine);
const yvex_model_descriptor *yvex_engine_model(const yvex_engine *engine);
const yvex_tensor_table *yvex_engine_tensors(const yvex_engine *engine);
const yvex_tokenizer *yvex_engine_tokenizer(const yvex_engine *engine);
const yvex_graph *yvex_engine_graph(const yvex_engine *engine);

int yvex_engine_get_summary(const yvex_engine *engine,
                            yvex_engine_summary *out,
                            yvex_error *err);

int yvex_engine_execute_fixture_graph(yvex_engine *engine,
                                      const yvex_fixture_graph_options *options,
                                      yvex_fixture_graph_result *out,
                                      yvex_error *err);
int yvex_engine_execute_partial_graph(yvex_engine *engine,
                                      const yvex_partial_graph_options *options,
                                      yvex_partial_graph_result *out,
                                      yvex_error *err);
int yvex_engine_execute_segment_graph(yvex_engine *engine,
                                      const yvex_segment_graph_options *options,
                                      yvex_segment_graph_result *out,
                                      yvex_error *err);
int yvex_engine_create_prefill_state(yvex_engine *engine,
                                     const yvex_prefill_state_options *options,
                                     yvex_prefill_state_summary *out,
                                     yvex_error *err);

const char *yvex_engine_diagnostic_reason(const yvex_engine *engine);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_ENGINE_H */
