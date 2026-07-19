/* Owner: generation.decode (generation).
 * Owns: the typed diagnostic state transition consumed by runtime and reports.
 * Does not own: model policy, graph admission, generation readiness, or higher-stage claims.
 * Invariants: diagnostic state never promotes model-backed generation capability.
 * Boundary: this owner exposes typed facts only at its admitted subsystem stage.
 * Purpose: Advance one bounded diagnostic decode-state transition over admitted session summaries.
 * Inputs: Prefill facts, session state, KV summary, step index, and caller-owned result storage.
 * Effects: Updates only the diagnostic decode result and accepted session counters.
 * Failure: Overflow and invalid lifecycle state refuse without logits or token publication. */

#include <yvex/internal/core.h>
#include <yvex/internal/generation.h>
#include <yvex/generation.h>
#include <yvex/runtime.h>

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const yvex_decode_step_summary decode_summary_default = {
    .decode_step_kind = "bounded-diagnostic",
    .decode_phase = "preflight",
    .decode_execution_mode = "prefill-summary-advance",
    .backend_name = "cpu",
    .segment_name = "embedding-rmsnorm",
    .context_boundary_status = "unchecked",
    .prefill_state_kind = "none",
    .prefill_phase = "not-started",
    .kv_binding_source = "none",
    .kv_status = "not-requested",
    .decode_state_source = "prefill-aggregate",
    .cleanup_status = "not-needed",
    .generation_status = "unsupported"
};

#define DECODE_FIELD(destination_, source_)                                             \
    {                                                                                   \
        offsetof(yvex_decode_step_summary, destination_),                               \
        offsetof(yvex_prefill_state_summary, source_),                                  \
        sizeof(((yvex_prefill_state_summary *)0)->source_) +                            \
            0u * sizeof(char[sizeof(((yvex_decode_step_summary *)0)->destination_) ==    \
                                    sizeof(((yvex_prefill_state_summary *)0)->source_)   \
                                ? 1                                                     \
                                : -1])                                                  \
    }

static const yvex_generation_field_projection decode_prefill_fields[] = {
    DECODE_FIELD(input_token_count, token_count),
    DECODE_FIELD(prefill_tokens_processed, tokens_processed),
    DECODE_FIELD(prefill_position_start, position_start),
    DECODE_FIELD(prefill_position_end, position_end),
    DECODE_FIELD(context_length, context_length),
    DECODE_FIELD(prefill_state_created, prefill_state_created),
    DECODE_FIELD(prefill_aggregate_checksum, aggregate_checksum),
    DECODE_FIELD(prefill_final_token_checksum, final_token_checksum),
    DECODE_FIELD(kv_bound_to_prefill, kv_bound_to_prefill),
    DECODE_FIELD(kv_positions_written, kv_positions_written),
    DECODE_FIELD(kv_read_checksum, kv_read_checksum),
    DECODE_FIELD(cleanup_attempted, cleanup_attempted),
};

#undef DECODE_FIELD

#define PREFILL_OPTION_FIELD(field_)                                                    \
    {                                                                                   \
        offsetof(yvex_prefill_state_options, field_),                                   \
        offsetof(yvex_decode_step_options, field_),                                     \
        sizeof(((yvex_decode_step_options *)0)->field_) +                               \
            0u * sizeof(char[sizeof(((yvex_prefill_state_options *)0)->field_) ==        \
                                    sizeof(((yvex_decode_step_options *)0)->field_)      \
                                ? 1                                                     \
                                : -1])                                                  \
    }

static const yvex_generation_field_projection decode_prefill_option_fields[] = {
    PREFILL_OPTION_FIELD(token_input),
    PREFILL_OPTION_FIELD(segment_name),
    PREFILL_OPTION_FIELD(position_start),
    PREFILL_OPTION_FIELD(chunk_size),
    PREFILL_OPTION_FIELD(context_length),
    PREFILL_OPTION_FIELD(attach_kv),
    PREFILL_OPTION_FIELD(kv_shape),
    PREFILL_OPTION_FIELD(layer_count),
    PREFILL_OPTION_FIELD(layer_hidden_dim),
    PREFILL_OPTION_FIELD(layer_head_dim),
    PREFILL_OPTION_FIELD(layer_ffn_dim),
};

#undef PREFILL_OPTION_FIELD

/* Purpose: Construct the admitted decode step summary init state only after its identities and resources are valid.
 * Inputs: A validated configuration, checked resource limits, and caller-owned result storage.
 * Effects: Mutates only the admitted destination or transaction after every precondition passes.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
void yvex_decode_step_summary_init(yvex_decode_step_summary *out,
                                   const yvex_decode_step_options *options)
{
    if (!out) {
        return;
    }
    *out = decode_summary_default;
    out->backend_name = options && options->backend_name ? options->backend_name : "cpu";
    out->segment_name = options && options->segment_name ? options->segment_name : "embedding-rmsnorm";
}

/* Purpose: Copy decode copy prefill summary between compatible admitted ranges without changing semantic identity.
 * Inputs: Typed admitted handles, immutable source ranges, checked dimensions, and an explicit destination.
 * Effects: Mutates only the admitted destination or transaction after every precondition passes.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
static void decode_copy_prefill_summary(yvex_decode_step_summary *out,
                                        const yvex_prefill_state_summary *prefill)
{
    if (!out || !prefill) {
        return;
    }
    yvex_generation_project_fields(
        out, prefill, decode_prefill_fields,
        sizeof(decode_prefill_fields) / sizeof(decode_prefill_fields[0]));
    out->context_boundary_status = prefill->context_boundary_status
                                       ? prefill->context_boundary_status
                                       : "unchecked";
    out->prefill_state_kind = prefill->prefill_state_kind
                                  ? prefill->prefill_state_kind
                                  : "segment-summary";
    out->prefill_phase = prefill->prefill_phase ? prefill->prefill_phase : "unknown";
    out->kv_binding_source = prefill->kv_bound_to_prefill && prefill->kv_binding_source
                                 ? prefill->kv_binding_source
                                 : (prefill->kv_binding_source ? prefill->kv_binding_source : "none");
    out->kv_status = prefill->kv_status ? prefill->kv_status : "not-requested";
    out->cleanup_status = prefill->cleanup_status ? prefill->cleanup_status : "not-needed";
}

/* Purpose: Decode decode state checksum according to its pinned numeric representation.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Mutates only the admitted destination or transaction after every precondition passes.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
static unsigned long long decode_state_checksum(const yvex_prefill_state_summary *prefill,
                                                unsigned long long decode_position)
{
    unsigned long long hash = 1469598103934665603ull;

    hash = yvex_core_hash_mix_u64(hash, prefill->aggregate_checksum);
    hash = yvex_core_hash_mix_u64(hash, prefill->final_token_checksum);
    hash = yvex_core_hash_mix_u64(hash, prefill->layer_final_checksum);
    hash = yvex_core_hash_mix_u64(hash, prefill->kv_read_checksum);
    hash = yvex_core_hash_mix_u64(hash, decode_position);
    hash = yvex_core_hash_mix_u64(hash, prefill->token_count);
    hash = yvex_core_hash_mix_u64(hash, prefill->context_length);
    return hash;
}

/* Purpose: Decode decode fill state values according to its pinned numeric representation.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Mutates only the admitted destination or transaction after every precondition passes.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
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
        hash = yvex_core_hash_mix_u64(hash, out->decode_position + i);
        word = (hash >> ((i % 4ull) * 12ull)) & 0xffffull;
        out->decode_state_values[i] =
            (float)((double)word / 65536.0 +
                    (double)out->decode_position * 0.0001 +
                    (double)i * 0.00001);
    }
}

/* Purpose: Decode engine decode step according to its pinned numeric representation.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Mutates only the admitted destination or transaction after every precondition passes.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
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
        return yvex_generation_refuse(err, YVEX_ERR_INVALID_ARG,
                                      "yvex_engine_decode_step",
                                      "engine, options, token input, and out are required");
    }

    yvex_decode_step_summary_init(out, options);
    out->input_token_count = options->token_input->token_count;
    if (options->token_input->token_count == 0ull) {
        return yvex_generation_refuse(err, YVEX_ERR_INVALID_ARG,
                                      "yvex_engine_decode_step",
                                      "decode requires at least one input token");
    }

    memset(&prefill_options, 0, sizeof(prefill_options));
    memset(&prefill_summary, 0, sizeof(prefill_summary));
    yvex_generation_project_fields(
        &prefill_options, options, decode_prefill_option_fields,
        sizeof(decode_prefill_option_fields) / sizeof(decode_prefill_option_fields[0]));
    if (prefill_options.context_length == 0ull) {
        if (!yvex_core_u64_add(options->position_start,
                                     options->token_input->token_count,
                                     &default_context_length) ||
            !yvex_core_u64_add(default_context_length, 1ull,
                                     &default_context_length)) {
            out->decode_phase = "context-boundary";
            out->context_boundary_status = "fail";
            return yvex_generation_refuse(err, YVEX_ERR_BOUNDS,
                                          "yvex_engine_decode_step",
                                          "decode context length overflow");
        }
        prefill_options.context_length = default_context_length;
    }
    out->prefill_invoked = 1;
    out->decode_phase = "prefill";
    rc = yvex_engine_create_prefill_state(engine, &prefill_options, &prefill_summary, err);
    decode_copy_prefill_summary(out, &prefill_summary);
    if (rc != YVEX_OK) {
        return rc;
    }
    if (!prefill_summary.prefill_state_created) {
        return yvex_generation_refuse(err, YVEX_ERR_STATE,
                                      "yvex_engine_decode_step",
                                      "prefill state was not created");
    }

    if (!yvex_core_u64_add(prefill_summary.position_end, 1ull, &decode_position)) {
        out->decode_phase = "context-boundary";
        out->context_boundary_status = "fail";
        return yvex_generation_refuse(err, YVEX_ERR_BOUNDS,
                                      "yvex_engine_decode_step",
                                      "decode position overflow");
    }
    out->decode_position = decode_position;
    if (decode_position >= prefill_summary.context_length) {
        out->decode_phase = "context-boundary";
        out->context_boundary_status = "fail";
        return yvex_generation_refuse(err, YVEX_ERR_BOUNDS,
                                      "yvex_engine_decode_step",
                                      "decode position exceeds context length");
    }
    out->context_boundary_status = "pass";

    rc = yvex_generation_test_refuse(
        "YVEX_TEST_FAIL_DECODE_AFTER_PREFILL", err, YVEX_ERR_BACKEND,
        "yvex_engine_decode_step", "test decode failure after prefill");
    if (rc != YVEX_OK) {
        out->decode_phase = "after-prefill";
        out->cleanup_attempted = 1;
        out->cleanup_status = "pass";
        return rc;
    }

    out->decode_phase = "state-allocation";
    rc = yvex_generation_test_refuse(
        "YVEX_TEST_FAIL_DECODE_AFTER_STATE_ALLOC", err, YVEX_ERR_NOMEM,
        "yvex_engine_decode_step", "test decode state allocation failure");
    if (rc != YVEX_OK) return rc;

    out->decode_state_checksum = decode_state_checksum(&prefill_summary, decode_position);
    decode_fill_state_values(out);
    out->decode_state_created = 1;
    out->decode_step_executed = 1;
    out->decode_phase = "complete";
    yvex_error_clear(err);
    return YVEX_OK;
}
