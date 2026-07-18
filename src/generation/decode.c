/*
 * Owner: generation.decode (generation).
 * Owns: the reusable-algorithm boundary consumed by runtime,cli.
 * Does not own: unrelated subsystem policy or unsupported higher-stage claims.
 * Invariants: scope=generic and visibility=private match config/source_owners.tsv.
 * Boundary: reusable-algorithm; moving this contract requires an ownership-manifest change.
 *
 * decode.c - Decode-step runtime boundary.
 *
 * This file owns one bounded diagnostic decode-state step over prefill/KV
 * state summaries. It does not produce logits, sample, generate, benchmark, or
 * claim full model decode.
 */

#include <yvex/decode.h>

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int decode_test_env_enabled(const char *name)
{
    const char *value = getenv(name);

    return value && value[0] != '\0' && strcmp(value, "0") != 0;
}

static unsigned long long decode_mix_u64(unsigned long long hash,
                                         unsigned long long value)
{
    unsigned int i;

    for (i = 0; i < 8u; ++i) {
        hash ^= (value >> (i * 8u)) & 0xffull;
        hash *= 1099511628211ull;
    }
    return hash;
}

static int decode_add_ull(unsigned long long a,
                          unsigned long long b,
                          unsigned long long *out)
{
    if (!out || a > ULLONG_MAX - b) {
        return 0;
    }
    *out = a + b;
    return 1;
}

void yvex_decode_step_summary_init(yvex_decode_step_summary *out,
                                   const yvex_decode_step_options *options)
{
    if (!out) {
        return;
    }
    memset(out, 0, sizeof(*out));
    out->decode_step_kind = "bounded-diagnostic";
    out->decode_phase = "preflight";
    out->decode_execution_mode = "prefill-summary-advance";
    out->backend_name = options && options->backend_name ? options->backend_name : "cpu";
    out->segment_name = options && options->segment_name ? options->segment_name : "embedding-rmsnorm";
    out->context_boundary_status = "unchecked";
    out->prefill_state_kind = "none";
    out->prefill_phase = "not-started";
    out->kv_binding_source = "none";
    out->kv_status = "not-requested";
    out->decode_state_source = "prefill-aggregate";
    out->cleanup_status = "not-needed";
    out->generation_status = "unsupported";
}

static void decode_copy_prefill_summary(yvex_decode_step_summary *out,
                                        const yvex_prefill_state_summary *prefill)
{
    if (!out || !prefill) {
        return;
    }
    out->input_token_count = prefill->token_count;
    out->prefill_tokens_processed = prefill->tokens_processed;
    out->prefill_position_start = prefill->position_start;
    out->prefill_position_end = prefill->position_end;
    out->context_length = prefill->context_length;
    out->context_boundary_status = prefill->context_boundary_status
                                       ? prefill->context_boundary_status
                                       : "unchecked";
    out->prefill_state_created = prefill->prefill_state_created;
    out->prefill_state_kind = prefill->prefill_state_kind
                                  ? prefill->prefill_state_kind
                                  : "segment-summary";
    out->prefill_phase = prefill->prefill_phase ? prefill->prefill_phase : "unknown";
    out->prefill_aggregate_checksum = prefill->aggregate_checksum;
    out->prefill_final_token_checksum = prefill->final_token_checksum;
    out->kv_bound_to_prefill = prefill->kv_bound_to_prefill;
    out->kv_binding_source = prefill->kv_bound_to_prefill && prefill->kv_binding_source
                                 ? prefill->kv_binding_source
                                 : (prefill->kv_binding_source ? prefill->kv_binding_source : "none");
    out->kv_status = prefill->kv_status ? prefill->kv_status : "not-requested";
    out->kv_positions_written = prefill->kv_positions_written;
    out->kv_read_checksum = prefill->kv_read_checksum;
    out->cleanup_attempted = prefill->cleanup_attempted;
    out->cleanup_status = prefill->cleanup_status ? prefill->cleanup_status : "not-needed";
}

static unsigned long long decode_state_checksum(const yvex_prefill_state_summary *prefill,
                                                unsigned long long decode_position)
{
    unsigned long long hash = 1469598103934665603ull;

    hash = decode_mix_u64(hash, prefill->aggregate_checksum);
    hash = decode_mix_u64(hash, prefill->final_token_checksum);
    hash = decode_mix_u64(hash, prefill->layer_final_checksum);
    hash = decode_mix_u64(hash, prefill->kv_read_checksum);
    hash = decode_mix_u64(hash, decode_position);
    hash = decode_mix_u64(hash, prefill->token_count);
    hash = decode_mix_u64(hash, prefill->context_length);
    return hash;
}

static void decode_fill_state_values(yvex_decode_step_summary *out)
{
    unsigned long long hash;
    unsigned long long i;

    if (!out) {
        return;
    }
    hash = out->decode_state_checksum;
    out->decode_state_value_count = YVEX_DECODE_STATE_MAX_VALUES;
    for (i = 0; i < out->decode_state_value_count; ++i) {
        unsigned long long word;
        hash = decode_mix_u64(hash, out->decode_position + i);
        word = (hash >> ((i % 4ull) * 12ull)) & 0xffffull;
        out->decode_state_values[i] =
            (float)((double)word / 65536.0 +
                    (double)out->decode_position * 0.0001 +
                    (double)i * 0.00001);
    }
}

int yvex_engine_decode_step(yvex_engine *engine,
                            const yvex_decode_step_options *options,
                            yvex_decode_step_summary *out,
                            yvex_error *err)
{
    yvex_prefill_state_options prefill_options;
    yvex_prefill_state_summary prefill_summary;
    unsigned long long default_context_length = 0ull;
    unsigned long long decode_position = 0ull;
    int rc;

    if (!engine || !options || !options->token_input || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_engine_decode_step",
                       "engine, options, token input, and out are required");
        return YVEX_ERR_INVALID_ARG;
    }

    yvex_decode_step_summary_init(out, options);
    out->input_token_count = options->token_input->token_count;
    if (options->token_input->token_count == 0ull) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_engine_decode_step",
                       "decode requires at least one input token");
        return YVEX_ERR_INVALID_ARG;
    }

    memset(&prefill_options, 0, sizeof(prefill_options));
    memset(&prefill_summary, 0, sizeof(prefill_summary));
    prefill_options.token_input = options->token_input;
    prefill_options.segment_name = options->segment_name;
    prefill_options.position_start = options->position_start;
    prefill_options.chunk_size = options->chunk_size;
    prefill_options.context_length = options->context_length;
    if (prefill_options.context_length == 0ull) {
        if (!decode_add_ull(options->position_start,
                            options->token_input->token_count,
                            &default_context_length) ||
            !decode_add_ull(default_context_length, 1ull, &default_context_length)) {
            out->decode_phase = "context-boundary";
            out->context_boundary_status = "fail";
            yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_engine_decode_step",
                           "decode context length overflow");
            return YVEX_ERR_BOUNDS;
        }
        prefill_options.context_length = default_context_length;
    }
    prefill_options.attach_kv = options->attach_kv;
    prefill_options.kv_shape = options->kv_shape;
    prefill_options.layer_count = options->layer_count;
    prefill_options.layer_hidden_dim = options->layer_hidden_dim;
    prefill_options.layer_head_dim = options->layer_head_dim;
    prefill_options.layer_ffn_dim = options->layer_ffn_dim;

    out->prefill_invoked = 1;
    out->decode_phase = "prefill";
    rc = yvex_engine_create_prefill_state(engine, &prefill_options, &prefill_summary, err);
    decode_copy_prefill_summary(out, &prefill_summary);
    if (rc != YVEX_OK) {
        return rc;
    }
    if (!prefill_summary.prefill_state_created) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_engine_decode_step",
                       "prefill state was not created");
        return YVEX_ERR_STATE;
    }

    if (!decode_add_ull(prefill_summary.position_end, 1ull, &decode_position)) {
        out->decode_phase = "context-boundary";
        out->context_boundary_status = "fail";
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_engine_decode_step",
                       "decode position overflow");
        return YVEX_ERR_BOUNDS;
    }
    out->decode_position = decode_position;
    if (decode_position >= prefill_summary.context_length) {
        out->decode_phase = "context-boundary";
        out->context_boundary_status = "fail";
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_engine_decode_step",
                       "decode position exceeds context length");
        return YVEX_ERR_BOUNDS;
    }
    out->context_boundary_status = "pass";

    if (decode_test_env_enabled("YVEX_TEST_FAIL_DECODE_AFTER_PREFILL")) {
        out->decode_phase = "after-prefill";
        out->cleanup_attempted = 1;
        out->cleanup_status = "pass";
        yvex_error_set(err, YVEX_ERR_BACKEND, "yvex_engine_decode_step",
                       "test decode failure after prefill");
        return YVEX_ERR_BACKEND;
    }

    out->decode_phase = "state-allocation";
    if (decode_test_env_enabled("YVEX_TEST_FAIL_DECODE_AFTER_STATE_ALLOC")) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_engine_decode_step",
                       "test decode state allocation failure");
        return YVEX_ERR_NOMEM;
    }

    out->decode_state_checksum = decode_state_checksum(&prefill_summary, decode_position);
    decode_fill_state_values(out);
    out->decode_state_created = 1;
    out->decode_step_executed = 1;
    out->decode_phase = "complete";
    yvex_error_clear(err);
    return YVEX_OK;
}
