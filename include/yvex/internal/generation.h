/* Owner: generation.internal (generation).
 * Owns: generation, KV, sampling, and trace report contracts.
 * Does not own: runtime execution, rendering, or capability promotion.
 * Invariants: declarations have one owner, stable ordering, and no hidden capability promotion.
 * Boundary: typed generation reporting and request facts.
 * Purpose: provide the canonical typed generation reporting and request facts contract.
 * Inputs: typed immutable facts and explicitly owned mutable lifecycle objects.
 * Effects: only declared lifecycle, allocation, I/O, and publication operations mutate state.
 * Failure: typed refusals leave outputs defined and preserve caller-owned state. */
#ifndef INCLUDE_YVEX_INTERNAL_GENERATION_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_GENERATION_H_INCLUDED

#include <stddef.h>

#include <yvex/core.h>
#include <yvex/generation.h>
#include <yvex/registry.h>
#include <yvex/runtime.h>
#include <yvex/internal/core.h>

#ifdef __cplusplus
extern "C" {
#endif

unsigned long long yvex_generation_hash_float(unsigned long long hash,
                                              double value);

typedef struct {
    size_t destination_offset;
    size_t source_offset;
    size_t size;
} yvex_generation_field_projection;

typedef enum {
    YVEX_GENERATION_EXIT_KV = 0,
    YVEX_GENERATION_EXIT_KV_OWNERSHIP,
    YVEX_GENERATION_EXIT_SAMPLING
} yvex_generation_exit_policy;

int yvex_generation_refuse(yvex_error *err,
                           yvex_status status,
                           const char *where,
                           const char *message);
int yvex_generation_test_refuse(const char *flag,
                                yvex_error *err,
                                yvex_status status,
                                const char *where,
                                const char *message);
int yvex_generation_exit_code(yvex_status status,
                              yvex_generation_exit_policy policy);
void yvex_generation_project_fields(
    void *destination,
    const void *source,
    const yvex_generation_field_projection *fields,
    size_t count);

typedef enum {
    YVEX_GENERATION_IDENTITY_MODEL_REF = 0,
    YVEX_GENERATION_IDENTITY_SAMPLING_ALIAS
} yvex_generation_identity_policy;

typedef enum {
    YVEX_GENERATION_ADMISSION_EMPTY = 0,
    YVEX_GENERATION_ADMISSION_RESOLVE,
    YVEX_GENERATION_ADMISSION_IDENTITY,
    YVEX_GENERATION_ADMISSION_TOKENS,
    YVEX_GENERATION_ADMISSION_GRAPH,
    YVEX_GENERATION_ADMISSION_ENGINE,
    YVEX_GENERATION_ADMISSION_READY
} yvex_generation_admission_stage;

typedef struct {
    yvex_model_ref model_ref;
    yvex_token_input token_input;
    const char *graph_guard_status;
    const char *graph_guard_phase;
    yvex_engine *engine;
    yvex_generation_admission_stage stage;
} yvex_generation_admission;

int yvex_generation_admission_prepare(
    yvex_generation_admission *admission,
    const char *model_arg,
    const char *tokens_text,
    yvex_generation_identity_policy identity_policy,
    yvex_error *err);
int yvex_generation_admission_engine_open(
    yvex_generation_admission *admission,
    const char *backend_name,
    yvex_error *err);
void yvex_generation_admission_close(yvex_generation_admission *admission);

/* Report contract. */
typedef enum {
    YVEX_GENERATION_TRACE_NONE = 0,
    YVEX_GENERATION_TRACE_TOKENS,
    YVEX_GENERATION_TRACE_STEPS,
    YVEX_GENERATION_TRACE_KV,
    YVEX_GENERATION_TRACE_LOGITS,
    YVEX_GENERATION_TRACE_SAMPLING,
    YVEX_GENERATION_TRACE_FULL
} yvex_generation_trace_level;
typedef struct {
    int attempted;
    unsigned long long index;
    unsigned long long decode_position;
    const char *decode_status;
    const char *logits_status;
    const char *sample_status;
    const char *append_status;
    unsigned int selected_token_id;
    unsigned int appended_token_id;
    unsigned int candidate_token_id;
    double candidate_logit;
    unsigned long long position_after_append;
    const char *stop_reason;
    const char *stop_timing;
    unsigned long long logits_checksum;
    double logits_min;
    double logits_max;
    unsigned long long sample_checksum;
    unsigned long long accepted_token_count;
    unsigned long long generated_token_count;
    unsigned long long total_token_count;
    unsigned long long sequence_checksum;
} yvex_generation_trace_step;
typedef struct {
    unsigned long long state_id;
    const char *lifecycle_status;
    const char *generation_state;
    int state_dirty;
    int active_step_seen;
    unsigned long long active_step;
    int last_completed_step_seen;
    unsigned long long last_completed_step;
    int cancel_supported;
    int cancel_after_steps_seen;
    unsigned long long cancel_after_steps;
    int cancel_requested;
    const char *cancel_reason;
    int cancel_step_seen;
    unsigned long long cancel_step;
    const char *cancel_timing;
    const char *cancel_safe_point;
    int partial_output_available;
    int cleanup_idempotent;
    int cleanup_repeated;
    int cleanup_owned_state_released;
    int failure_preserved;
    int partial_output_preserved;
} yvex_generation_state;
typedef struct {
    const char *model_arg;
    const char *backend_name;
    const char *segment_name;
    const char *tokens_text;
    unsigned long long max_new_tokens;
    unsigned long long position_start;
    unsigned long long context_length;
    unsigned long long logits_count;
    unsigned long long cancel_after_steps;
    int context_length_seen;
    int cancel_after_steps_seen;
    int attach_kv;
    yvex_kv_shape kv_shape;
    int layer_count_seen;
    unsigned long long layer_count;
    unsigned long long layer_hidden_dim;
    unsigned long long layer_head_dim;
    unsigned long long layer_ffn_dim;
    int chunk_size_seen;
    unsigned long long chunk_size;
    yvex_generation_trace_level trace_level;
} yvex_generation_request;
typedef struct yvex_generation_report {
    const char *model_arg;
    const char *backend_name;
    const char *segment_name;
    int loop_created;
    int loop_executed;
    const char *phase;
    const char *status;
    yvex_generation_state state;
    const char *token_input_status;
    unsigned int prompt_tokens[YVEX_TOKEN_INPUT_MAX_TOKENS];
    unsigned int generated_tokens[YVEX_TOKEN_INPUT_MAX_TOKENS];
    unsigned long long prompt_token_count;
    unsigned long long prefill_token_count;
    unsigned long long max_new_tokens;
    unsigned long long generated_token_count;
    unsigned long long accepted_token_count;
    unsigned long long total_token_count;
    unsigned long long position_start;
    unsigned long long prefill_position_end;
    unsigned long long current_decode_position;
    int prefill_invoked;
    unsigned long long decode_steps;
    unsigned long long logits_steps;
    unsigned long long sample_steps;
    unsigned long long append_steps;
    int candidate_token_seen;
    unsigned int candidate_token_id;
    double candidate_logit;
    unsigned int last_selected_token_id;
    double last_selected_logit;
    int last_appended_token_seen;
    unsigned int last_appended_token_id;
    const char *append_status;
    const char *append_failure;
    unsigned long long context_length;
    const char *stop_policy;
    int stop_requested;
    const char *stop_reason;
    const char *stop_phase;
    unsigned long long stop_step;
    const char *stop_timing;
    int stop_after_append;
    int stop_before_append;
    int failure_stop;
    int unsupported_stop_feature;
    const char *eos_policy;
    const char *stop_token_policy;
    yvex_generation_trace_level trace_level;
    const char *trace_level_name;
    int trace_enabled;
    const char *trace_status;
    unsigned long long trace_records;
    unsigned long long trace_tokens;
    unsigned long long trace_steps;
    unsigned long long trace_kv;
    unsigned long long trace_logits;
    unsigned long long trace_sampling;
    unsigned long long trace_append;
    unsigned long long trace_stop;
    unsigned long long trace_cancel;
    unsigned long long trace_cleanup;
    unsigned long long trace_failures;
    yvex_generation_trace_step trace_step_records[YVEX_TOKEN_INPUT_MAX_TOKENS];
    unsigned long long trace_step_count;
    int trace_kv_requested;
    yvex_kv_shape trace_kv_shape;
    unsigned long long generation_checksum;
    unsigned long long sequence_checksum;
    int cleanup_attempted;
    const char *cleanup_status;
    const char *failed_phase;
    unsigned long long failed_step;
    unsigned long long partial_generated_token_count;
    unsigned long long last_successful_position;
} yvex_generation_report;
/*
 * yvex_generation_run_diagnostic()
 *
 * Purpose:
 *   execute the bounded diagnostic generation loop from a typed request and
 *   fill one report with loop, trace, cleanup, and boundary facts.
 *
 * Inputs:
 *   request is borrowed; report receives by-value diagnostic fields.
 *
 * Effects:
 *   may resolve a model reference, validate explicit tokens, open/close an
 *   engine, sample bounded diagnostic tokens, and account trace records.
 *
 * Failure:
 *   returns YVEX status codes through err while preserving any report fields
 *   available up to failure.
 *
 * Boundary:
 *   this is diagnostic generation only, not full-model generation, eval,
 *   benchmark, throughput, or release readiness. */
int yvex_generation_run_diagnostic(const yvex_generation_request *request,
                                   yvex_generation_report *report,
                                   yvex_error *err);
const char *yvex_generation_trace_level_name(yvex_generation_trace_level level);

/* Kv Report contract. */
typedef enum {
    YVEX_KV_REPORT_MODE_NORMAL = 0,
    YVEX_KV_REPORT_MODE_TABLE,
    YVEX_KV_REPORT_MODE_AUDIT
} yvex_kv_report_mode;
typedef enum {
    YVEX_KV_REQUEST_REPORT = 0,
    YVEX_KV_REQUEST_OWNERSHIP
} yvex_kv_request_kind;
typedef struct {
    yvex_kv_request_kind kind;
    const char *model;
    const char *family;
    const char *backend;
    const char *registry_path;
    int include_attention;
    int include_context;
    int include_residency;
    int include_blockers;
    yvex_kv_shape shape;
    int append_demo;
    int read_requested;
    unsigned long long read_position;
    yvex_kv_report_mode report_mode;
} yvex_kv_report_request;
typedef struct {
    const char *name;
    const char *status;
} yvex_kv_phase_fact;
typedef struct {
    const char *name;
    const char *status;
    const char *blocker_class;
} yvex_kv_blocker_fact;
typedef struct {
    yvex_kv_request_kind kind;
    const char *status;
    const char *report_name;
    const char *model;
    const char *model_resolved_path;
    const char *target_id;
    const char *target_class;
    const char *backend;
    const char *family;
    const char *family_detected;
    const char *family_requested;
    const char *family_runtime_status;
    const char *attention_class_status;
    const char *kv_class_status;
    const char *kv_stage;
    const char *kv_support_status;
    const char *runtime_claim;
    const char *generation;
    const char *benchmark_status;
    int diagnostic_kv_available;
    const char *diagnostic_kv_boundary;
    int real_attention_kv;
    int real_attention_kv_write_ready;
    int real_attention_kv_read_ready;
    int decode_kv_consumer_ready;
    int kv_required;
    const char *kv_source;
    const char *kv_layout;
    const char *kv_layout_status;
    const char *kv_dtype;
    const char *kv_dtype_status;
    const char *kv_layers;
    const char *kv_layers_status;
    const char *kv_heads;
    const char *kv_heads_status;
    const char *kv_head_dim;
    const char *kv_head_dim_status;
    const char *kv_positions;
    const char *kv_capacity;
    const char *kv_capacity_status;
    const char *kv_indexing;
    const char *context_length_source;
    unsigned long long max_context;
    int max_context_seen;
    const char *attention_dependency_status;
    int attention_q_required;
    int attention_k_required;
    int attention_v_required;
    const char *attention_q_status;
    const char *attention_k_status;
    const char *attention_v_status;
    const char *attention_o_status;
    const char *tensor_inventory_status;
    unsigned long long tensor_count;
    unsigned long long tensor_bytes;
    const char *role_token_embedding_status;
    const char *role_attention_norm_status;
    const char *role_q_projection_status;
    const char *role_k_projection_status;
    const char *role_v_projection_status;
    const char *role_o_projection_status;
    const char *role_output_head_status;
    yvex_kv_summary ownership_summary;
    int kv_created;
    int session_owned;
    unsigned long long last_appended_position;
    int read_requested;
    unsigned long long read_position;
    unsigned long long read_value_count;
    unsigned long long read_checksum;
    unsigned long long read_sample_count;
    float read_sample_values[8];
    int cleanup_attempted;
    const char *cleanup_status;
    yvex_kv_phase_fact phases[24];
    unsigned long phase_count;
    yvex_kv_blocker_fact blockers[32];
    unsigned long blocker_count;
    const char *next_required_rows;
    int exit_code;
    int include_attention;
    int include_context;
    int include_residency;
    int include_blockers;
    const char *top_blocker;
    const char *source_artifact_class;
    const char *target_artifact_class;
    const char *reason;
    const char *kv_layer_indexing;
    const char *kv_head_indexing;
    const char *kv_position_indexing;
    const char *kv_token_order_policy;
    const char *kv_residency_class;
    const char *kv_residency_status;
    const char *kv_cpu_bytes_estimate;
    const char *kv_cuda_bytes_estimate;
    const char *kv_host_staged_bytes_estimate;
    const char *kv_ssd_staged_status;
    const char *kv_ssd_streamed_status;
    const char *kv_paged_status;
    const char *kv_chunked_status;
    const char *kv_quantized_status;
    const char *kv_managed_memory_status;
    const char *context_required;
    const char *requested_context;
    const char *context_capacity_status;
    const char *context_overflow_policy;
    const char *attention_runtime_ready;
    const char *full_transformer_attention_ready;
    const char *prefill_kv_write_required;
    const char *prefill_kv_write_ready;
    const char *decode_kv_read_required;
    const char *decode_kv_read_ready;
    const char *qkv_role_coverage;
    int backend_allocation_attempted;
    int full_kv_allocation_proof;
    int cuda_full_kv_allocation_proof;
    int paged_kv_implementation;
    int chunked_kv_runtime_implementation;
    int ssd_backed_kv;
    int quantized_kv_runtime;
    int full_transformer_prefill_ready;
    int decode_ready;
    int logits_ready;
    int sampling_ready;
    int runtime_execution_ready;
    int generation_ready;
    char model_resolved_path_storage[YVEX_PATH_CAP];
    char target_id_storage[128];
    char family_storage[64];
    char family_detected_storage[64];
    char blocker_storage[256];
    char reason_storage[256];
} yvex_kv_report;
int yvex_kv_report_build(const yvex_kv_report_request *request,
                         yvex_kv_report *report,
                         yvex_error *err);
int yvex_kv_ownership_report_build(const yvex_kv_report_request *request,
                                   yvex_kv_report *report,
                                   yvex_error *err);

/* Sampling Report contract. */
typedef enum {
    YVEX_SAMPLING_REPORT_NORMAL = 0,
    YVEX_SAMPLING_REPORT_AUDIT
} yvex_sampling_report_mode;
typedef struct {
    const char *model_arg;
    const char *backend_name;
    const char *segment_name;
    const char *tokens_text;
    yvex_sampling_strategy strategy;
    unsigned long long logits_count;
    unsigned long long position_start;
    unsigned long long context_length;
    int context_length_seen;
    int attach_kv;
    yvex_kv_shape kv_shape;
    int layer_count_seen;
    unsigned long long layer_count;
    unsigned long long layer_hidden_dim;
    unsigned long long layer_head_dim;
    unsigned long long layer_ffn_dim;
    int chunk_size_seen;
    unsigned long long chunk_size;
} yvex_sampling_report_request;
typedef struct {
    const char *status;
    const char *model_arg;
    const char *backend_name;
    const char *segment_name;
    const char *token_input_status;
    unsigned long long input_token_count;
    yvex_sampling_summary summary;
    int graph_guard_rendered;
    const char *graph_guard_status;
    const char *graph_guard_phase;
    int cleanup_attempted;
    const char *cleanup_status;
    const char *runtime_claim;
    const char *generation;
    const char *benchmark_status;
    int real_vocab_sampling;
    int real_model_sampling;
    int sampling_ready;
    int generation_ready;
    int exit_code;
} yvex_sampling_report;
int yvex_sampling_report_build(const yvex_sampling_report_request *request,
                               yvex_sampling_report *report,
                               yvex_error *err);

/* Trace contract. */
int yvex_generation_trace_wants_tokens(yvex_generation_trace_level level);
int yvex_generation_trace_wants_steps(yvex_generation_trace_level level);
int yvex_generation_trace_wants_kv(yvex_generation_trace_level level);
int yvex_generation_trace_wants_logits(yvex_generation_trace_level level);
int yvex_generation_trace_wants_sampling(yvex_generation_trace_level level);
void yvex_generation_trace_account(yvex_generation_report *report);

/* Private contract. */
void yvex_kv_fill_demo_values(float *values,
                         unsigned long long value_count,
                         unsigned long long position);
unsigned long long yvex_kv_checksum_values(const float *values,
                                      unsigned long long value_count);
void yvex_sampling_summary_defaults(yvex_sampling_summary *out,
                             const yvex_sampling_options *options);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_YVEX_INTERNAL_GENERATION_H_INCLUDED */
