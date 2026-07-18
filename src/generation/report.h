/*
 * report.h - diagnostic generation request/report facts.
 *
 * Owner:
 *   src/generation
 *
 * Owns:
 *   typed diagnostic generation request and report shapes.
 *
 * Does not own:
 *   CLI argument parsing, command dispatch, rendering, graph ownership,
 *   tokenizer training, provider generation, eval, benchmark, or release
 *   decisions.
 *
 * Invariants:
 *   reports carry bounded diagnostic facts only; trace counters are computed
 *   before rendering; no field implies full-model generation readiness.
 *
 * Boundary:
 *   diagnostic generation reports are not runtime generation support, eval
 *   evidence, benchmark evidence, throughput, or release readiness.
 */
#ifndef YVEX_GENERATION_REPORT_H
#define YVEX_GENERATION_REPORT_H

#include <yvex/api.h>

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
 *   benchmark, throughput, or release readiness.
 */
int yvex_generation_run_diagnostic(const yvex_generation_request *request,
                                   yvex_generation_report *report,
                                   yvex_error *err);

const char *yvex_generation_trace_level_name(yvex_generation_trace_level level);

#endif
