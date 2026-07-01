/*
 * YVEX - Decode step boundary
 *
 * File: include/yvex/decode.h
 * Layer: public runtime API
 *
 * Purpose:
 *   Defines one bounded diagnostic decode-state step over existing prefill and
 *   KV state summaries. This API does not produce logits, sample, generate, or
 *   claim full model decode.
 */
#ifndef YVEX_DECODE_H
#define YVEX_DECODE_H

#include <yvex/engine.h>

#ifdef __cplusplus
extern "C" {
#endif

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

int yvex_engine_decode_step(yvex_engine *engine,
                            const yvex_decode_step_options *options,
                            yvex_decode_step_summary *out,
                            yvex_error *err);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_DECODE_H */
