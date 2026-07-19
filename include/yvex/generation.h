/* Owner: public generation ABI.
 * Owns: token inputs, decode results, logits, and sampling contracts.
 * Does not own: engine ownership, tokenizer implementation, or full generation readiness.
 * Invariants: declarations are format-stable, externally consumable, and independently includable.
 * Boundary: generation-stage value and operation contracts.
 * Purpose: Expose generation-stage value and operation contracts.
 * Inputs: Typed caller-owned values and immutable borrowed views as declared below.
 * Effects: Only functions with explicit lifecycle or I/O contracts mutate external state.
 * Failure: Typed status and error outputs remain authoritative; declarations add no capability. */
#ifndef YVEX_GENERATION_H
#define YVEX_GENERATION_H

#include <yvex/core.h>
#include <yvex/model.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct yvex_engine yvex_engine;

/* Token input. */
#define YVEX_TOKEN_INPUT_MAX_TOKENS 1024ull

typedef enum {
    YVEX_TOKEN_INPUT_EXPLICIT = 1,
    YVEX_TOKEN_INPUT_PROMPT_TEXT = 2
} yvex_token_input_kind;

typedef struct {
    yvex_token_input_kind kind;
    unsigned long long token_count;
    unsigned int tokens[YVEX_TOKEN_INPUT_MAX_TOKENS];
    unsigned long long max_tokens;
    int token_bounds_checked;
    int token_bounds_valid;
    unsigned long long vocab_size;
} yvex_token_input;

void yvex_token_input_init(yvex_token_input *input, yvex_token_input_kind kind);

const char *yvex_token_input_kind_name(yvex_token_input_kind kind);

int yvex_token_input_parse_explicit(const char *text,
                                    yvex_token_input *out,
                                    yvex_error *err);

int yvex_token_input_from_ids(yvex_token_input_kind kind,
                              const unsigned int *ids,
                              unsigned long long count,
                              yvex_token_input *out,
                              yvex_error *err);

int yvex_token_input_validate_bounds(yvex_token_input *input,
                                     unsigned long long vocab_size,
                                     yvex_error *err);

int yvex_token_input_select(const yvex_token_input *input,
                            unsigned long long token_index,
                            unsigned int *out_token,
                            yvex_error *err);

/* Decode step. */
#define YVEX_DECODE_STATE_MAX_VALUES 16u

typedef struct {
    const yvex_token_input *token_input;
    const char *segment_name;
    const char *backend_name;
    unsigned long long position_start;
    unsigned long long chunk_size;
    unsigned long long context_length;
    int attach_kv;
    yvex_kv_shape kv_shape;
    unsigned long long layer_count;
    unsigned long long layer_hidden_dim;
    unsigned long long layer_head_dim;
    unsigned long long layer_ffn_dim;
} yvex_decode_step_options;

typedef struct {
    int decode_state_created;
    int decode_step_executed;
    const char *decode_step_kind;
    const char *decode_phase;
    const char *decode_execution_mode;
    const char *backend_name;
    const char *segment_name;
    unsigned long long input_token_count;
    unsigned long long prefill_tokens_processed;
    unsigned long long prefill_position_start;
    unsigned long long prefill_position_end;
    unsigned long long decode_position;
    unsigned long long context_length;
    const char *context_boundary_status;
    int prefill_invoked;
    int prefill_state_created;
    const char *prefill_state_kind;
    const char *prefill_phase;
    unsigned long long prefill_aggregate_checksum;
    unsigned long long prefill_final_token_checksum;
    int kv_bound_to_prefill;
    const char *kv_binding_source;
    const char *kv_status;
    unsigned long long kv_positions_written;
    unsigned long long kv_read_checksum;
    const char *decode_state_source;
    unsigned long long decode_state_checksum;
    unsigned long long decode_state_value_count;
    float decode_state_values[YVEX_DECODE_STATE_MAX_VALUES];
    int cleanup_attempted;
    const char *cleanup_status;
    int real_model_decode;
    int full_model_decode_ready;
    int logits_ready;
    int sampling_ready;
    int generation_ready;
    const char *generation_status;
} yvex_decode_step_summary;

void yvex_decode_step_summary_init(yvex_decode_step_summary *out,
                                   const yvex_decode_step_options *options);

int yvex_engine_decode_step(yvex_engine *engine,
                            const yvex_decode_step_options *options,
                            yvex_decode_step_summary *out,
                            yvex_error *err);

/* Logits buffers. */
typedef struct yvex_logits yvex_logits;

typedef enum {
    YVEX_LOGITS_STATUS_EMPTY = 0,
    YVEX_LOGITS_STATUS_UNAVAILABLE,
    YVEX_LOGITS_STATUS_ALLOCATED
} yvex_logits_status;

typedef struct {
    yvex_logits_status status;
    unsigned long long vocab_size;
    unsigned long long bytes;
} yvex_logits_summary;

#define YVEX_LOGITS_MAX_SAMPLE_VALUES 16u

typedef struct {
    const yvex_decode_step_options *decode_options;
    unsigned long long logits_count;
} yvex_logits_buffer_options;

typedef struct {
    int logits_buffer_created;
    const char *logits_buffer_kind;
    const char *logits_phase;
    const char *logits_source;
    const char *backend_name;
    int decode_invoked;
    int decode_state_created;
    int decode_step_executed;
    const char *decode_step_kind;
    const char *decode_phase;
    unsigned long long decode_position;
    unsigned long long decode_state_checksum;
    unsigned long long logits_count;
    unsigned long long logits_bytes;
    unsigned long long logits_checksum;
    double logits_min;
    double logits_max;
    double logits_sum;
    unsigned long long logits_sample_count;
    float logits_sample_values[YVEX_LOGITS_MAX_SAMPLE_VALUES];
    int cleanup_attempted;
    const char *cleanup_status;
    int real_model_logits;
    int real_model_output_head;
    int bounded_logits_ready;
    int logits_ready;
    int sampling_ready;
    int generation_ready;
    const char *generation_status;
} yvex_logits_buffer_summary;

int yvex_logits_count_valid(unsigned long long count);
void yvex_logits_buffer_summary_init(yvex_logits_buffer_summary *out,
                                     const yvex_logits_buffer_options *options);

int yvex_logits_create(yvex_logits **out,
                       const yvex_model_descriptor *model,
                       yvex_error *err);
void yvex_logits_close(yvex_logits *logits);

yvex_logits_status yvex_logits_status_of(const yvex_logits *logits);
const char *yvex_logits_status_name(yvex_logits_status status);

int yvex_logits_get_summary(const yvex_logits *logits,
                            yvex_logits_summary *out,
                            yvex_error *err);

int yvex_engine_create_logits_buffer(yvex_engine *engine,
                                     const yvex_logits_buffer_options *options,
                                     yvex_logits_buffer_summary *out,
                                     yvex_error *err);

/* Sampling. */
typedef enum {
    YVEX_SAMPLING_STRATEGY_GREEDY = 0
} yvex_sampling_strategy;

typedef struct {
    const yvex_logits_buffer_options *logits_options;
    yvex_sampling_strategy strategy;
} yvex_sampling_options;

typedef struct {
    int sample_created;
    int sample_executed;
    const char *sampler_kind;
    const char *sampling_phase;
    const char *sampling_strategy;
    const char *sampling_source;
    const char *backend_name;
    int logits_invoked;
    int logits_buffer_created;
    const char *logits_buffer_kind;
    const char *logits_phase;
    unsigned long long logits_count;
    unsigned long long logits_checksum;
    double logits_min;
    double logits_max;
    unsigned long long candidates_considered;
    const char *tie_break;
    unsigned long long selected_logit_index;
    unsigned int selected_token_id;
    double selected_logit;
    unsigned long long sample_checksum;
    int cleanup_attempted;
    const char *cleanup_status;
    int bounded_sampling_ready;
    int real_vocab_sampling;
    int real_model_sampling;
    int sampling_ready;
    int generation_ready;
    const char *generation_status;
} yvex_sampling_summary;

const char *yvex_sampling_strategy_name(yvex_sampling_strategy strategy);

int yvex_engine_sample_token(yvex_engine *engine,
                             const yvex_sampling_options *options,
                             yvex_sampling_summary *out,
                             yvex_error *err);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_GENERATION_H */
