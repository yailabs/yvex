/* Owner: generation.prefill.
 * Owns: bounded diagnostic segment/layer prefill orchestration and temporary KV binding.
 * Does not own: transformer attention, persistent KV, decode, logits, sampling, or generation.
 * Invariants: token order, failure checkpoints, summary fields, and cleanup order are deterministic.
 * Boundary: consumes admitted engine/graph/KV APIs and publishes only a diagnostic prefill summary.
 * Purpose: execute the implemented segment-summary prefill foundation under bounded ownership.
 * Inputs: admitted engine, validated token input, immutable prefill options, caller output storage.
 * Effects: may allocate chunk scratch and temporary KV storage and execute admitted graph fixtures.
 * Failure: typed admission, bounds, allocation, state, graph, backend, and cleanup failures. */

#include <yvex/runtime.h>
#include <yvex/internal/core.h>
#include <yvex/internal/generation.h>
#include <yvex/internal/runtime.h>

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define PREFILL_KV_FIELD(destination_, source_)                                         \
    {                                                                                   \
        offsetof(yvex_prefill_state_summary, destination_),                             \
        offsetof(yvex_kv_summary, source_),                                             \
        sizeof(((yvex_kv_summary *)0)->source_) +                                       \
            0u * sizeof(char[sizeof(((yvex_prefill_state_summary *)0)->destination_) ==  \
                                    sizeof(((yvex_kv_summary *)0)->source_)              \
                                ? 1                                                     \
                                : -1])                                                  \
    }

static const yvex_generation_field_projection prefill_kv_fields[] = {
    PREFILL_KV_FIELD(kv_owner, owner),
    PREFILL_KV_FIELD(kv_dtype, dtype),
    PREFILL_KV_FIELD(kv_layers, layer_count),
    PREFILL_KV_FIELD(kv_heads, kv_head_count),
    PREFILL_KV_FIELD(kv_head_dim, head_dim),
    PREFILL_KV_FIELD(kv_capacity, context_length),
    PREFILL_KV_FIELD(kv_values_per_position, values_per_position),
    PREFILL_KV_FIELD(kv_bytes_per_position, bytes_per_position),
    PREFILL_KV_FIELD(kv_planned_bytes, bytes),
    PREFILL_KV_FIELD(kv_allocated_bytes, allocated_bytes),
    PREFILL_KV_FIELD(kv_positions_written, written_positions),
    PREFILL_KV_FIELD(kv_append_count, append_count),
    PREFILL_KV_FIELD(kv_read_count, read_count),
    PREFILL_KV_FIELD(kv_overflow_status, overflow_status),
    PREFILL_KV_FIELD(session_kv_owned, session_owned),
};

#undef PREFILL_KV_FIELD

/* Purpose: derive the stable diagnostic checksum for an ordered float range.
 * Inputs: optional float range and element count.
 * Effects: reads values and returns a deterministic hash.
 * Failure: a null range hashes as the empty prefix without allocation.
 * Boundary: diagnostic identity only; not a cryptographic artifact identity. */
static unsigned long long prefill_checksum_f32_values(const float *values,
                                                      unsigned long long count)
{
    unsigned long long hash = 1469598103934665603ull;
    unsigned long long i;

    for (i = 0; values && i < count; ++i) {
        uint32_t raw = 0u;
        memcpy(&raw, &values[i], sizeof(raw));
        hash = yvex_core_hash_mix_u64(hash, (unsigned long long)raw);
        hash = yvex_core_hash_mix_u64(hash, i);
    }
    return hash;
}

/* Purpose: derive the exact number of bounded token chunks.
 * Inputs: token count and nonzero chunk size.
 * Effects: returns a pure integer result.
 * Failure: returns zero when either input is zero.
 * Boundary: planning arithmetic only. */
static unsigned long long prefill_chunk_count(unsigned long long token_count,
                                              unsigned long long chunk_size)
{
    if (token_count == 0ull || chunk_size == 0ull) {
        return 0ull;
    }
    return (token_count + chunk_size - 1ull) / chunk_size;
}

/* Purpose: derive one deterministic diagnostic KV position from the selected sample.
 * Inputs: destination range, optional source sample, token identity, and absolute position.
 * Effects: overwrites exactly value_count caller-owned floats.
 * Failure: a null destination is a no-op; no allocation or I/O.
 * Boundary: diagnostic KV fixture values, not real attention-backed KV. */
static void fill_prefill_kv_values(float *values,
                                   unsigned long long value_count,
                                   const float *source_values,
                                   unsigned long long source_count,
                                   unsigned int token_id,
                                   unsigned long long position)
{
    unsigned long long i;

    for (i = 0; values && i < value_count; ++i) {
        float base = 0.0f;
        if (source_values && source_count > 0ull) {
            base = source_values[i % source_count];
        }
        values[i] = base +
                    (float)(position * 0.001) +
                    (float)(token_id * 0.000001) +
                    (float)(i * 0.0000001);
    }
}

/* Purpose: release the optional chunk scratch owner and record its cleanup result.
 * Inputs: owned scratch slot and mutable summary.
 * Effects: frees and nulls scratch, then updates cleanup facts.
 * Failure: missing ownership inputs are a no-op.
 * Boundary: owns only the host diagnostic scratch buffer. */
static void prefill_cleanup_scratch(float **scratch,
                                    yvex_prefill_state_summary *out)
{
    if (!scratch || !*scratch || !out) {
        return;
    }
    free(*scratch);
    *scratch = NULL;
    out->prefill_scratch_cleanup_attempted = 1;
    out->prefill_scratch_cleanup_status = "pass";
}

/* Purpose: project canonical KV lifecycle facts into the prefill summary.
 * Inputs: immutable KV summary and mutable prefill summary.
 * Effects: overwrites the KV-related public fields only.
 * Failure: missing inputs are a no-op.
 * Boundary: projection only; it neither owns nor mutates the KV cache. */
static void prefill_summary_apply_kv(const yvex_kv_summary *kv,
                                     yvex_prefill_state_summary *out)
{
    if (!kv || !out) {
        return;
    }
    yvex_generation_project_fields(
        out, kv, prefill_kv_fields,
        sizeof(prefill_kv_fields) / sizeof(prefill_kv_fields[0]));
    out->kv_status = yvex_kv_status_name(kv->status);
}

/* Purpose: clear an owned temporary KV cache and record cleanup evidence.
 * Inputs: live KV cache and mutable prefill summary.
 * Effects: invokes cache clear and updates cleanup status.
 * Failure: returns false for missing inputs or a typed KV clear failure.
 * Boundary: does not close the cache handle. */
static int prefill_cleanup_kv(yvex_kv_cache *kv,
                              yvex_prefill_state_summary *out)
{
    yvex_error cleanup_err;

    if (!kv || !out) {
        return 0;
    }
    yvex_error_clear(&cleanup_err);
    out->cleanup_attempted = 1;
    if (yvex_kv_cache_clear(kv, &cleanup_err) == YVEX_OK) {
        out->cleanup_status = "pass";
        out->kv_cleanup_status = "pass";
        return 1;
    }
    out->cleanup_status = "fail";
    out->kv_cleanup_status = "fail";
    return 0;
}

/* Purpose: unwind all temporary KV, staging, and scratch ownership after failure.
 * Inputs: owned handle/buffer slots and mutable summary.
 * Effects: clears/closes KV, frees buffers, nulls slots, records cleanup.
 * Failure: cleanup refusal is reflected in the summary; the routine always completes.
 * Boundary: releases only resources acquired by one prefill invocation. */
static void prefill_cleanup_runtime(yvex_kv_cache **kv,
                                    float **kv_values,
                                    float **kv_read_values,
                                    float **scratch,
                                    yvex_prefill_state_summary *out)
{
    if (kv && *kv) {
        (void)prefill_cleanup_kv(*kv, out);
        yvex_kv_cache_close(*kv);
        *kv = NULL;
    }
    if (kv_values && *kv_values) {
        free(*kv_values);
        *kv_values = NULL;
    }
    if (kv_read_values && *kv_read_values) {
        free(*kv_read_values);
        *kv_read_values = NULL;
    }
    prefill_cleanup_scratch(scratch, out);
}

typedef struct {
    yvex_engine *engine;
    const yvex_prefill_state_options *options;
    yvex_prefill_state_summary *out;
    yvex_error *err;
    const yvex_token_input *input;
    const char *segment_name;
    yvex_kv_cache *kv;
    float *scratch;
    float *kv_values;
    float *kv_read_values;
    float layer_seed[YVEX_SEGMENT_GRAPH_MAX_OUTPUT_VALUES];
    unsigned long long aggregate;
    unsigned long long chunk_checksum;
    unsigned long long effective_chunk_size;
    unsigned long long scratch_slots;
    unsigned long long kv_value_count;
    int layer_requested;
    int chunked;
} prefill_run;

/* Purpose: record and return one typed prefill failure.
 * Inputs: active run, status code, and stable message.
 * Effects: mutates only caller error storage.
 * Failure: returns the supplied status even when error storage is absent.
 * Boundary: does not clean resources or mutate summary lifecycle fields. */
static int prefill_error(prefill_run *run, yvex_status status, const char *message)
{
    yvex_error_set(run ? run->err : NULL, status, "yvex_engine_create_prefill_state", message);
    return status;
}

/* Purpose: identify the token/chunk at which execution refused.
 * Inputs: active run plus logical token and chunk indexes.
 * Effects: updates only failure indexes in the summary.
 * Failure: none for an admitted run.
 * Boundary: preserves the unchunked public convention of chunk index zero. */
static void prefill_mark_failed(prefill_run *run,
                                unsigned long long token,
                                unsigned long long chunk)
{
    run->out->failed_token_index = token;
    run->out->failed_chunk_index = run->chunked ? chunk : 0ull;
}

/* Purpose: release every temporary resource owned by an active run.
 * Inputs: active run with optional KV and staging owners.
 * Effects: clears/closes KV and frees/nulls all temporary buffers.
 * Failure: cleanup status is retained in the summary.
 * Boundary: delegates to the canonical prefill cleanup lifecycle. */
static void prefill_run_cleanup(prefill_run *run)
{
    prefill_cleanup_runtime(&run->kv, &run->kv_values, &run->kv_read_values,
                            &run->scratch, run->out);
}

/* Purpose: apply the established cleanup path for execution-stage refusal.
 * Inputs: active run with optional KV ownership.
 * Effects: unwinds runtime owners or records successful scratch cleanup.
 * Failure: cleanup refusal remains represented by canonical KV cleanup facts.
 * Boundary: preserves legacy summary semantics at fault-injection checkpoints. */
static void prefill_default_cleanup(prefill_run *run)
{
    if (run->kv) {
        prefill_run_cleanup(run);
        return;
    }
    run->out->cleanup_attempted = 1;
    run->out->cleanup_status = "pass";
    prefill_cleanup_scratch(&run->scratch, run->out);
}

/* Purpose: preserve a graph result's cleanup facts while releasing prefill owners.
 * Inputs: active run plus graph-owned cleanup evidence.
 * Effects: unwinds KV or scratch ownership and records the exact graph cleanup state.
 * Failure: cleanup refusal remains visible through the existing summary fields.
 * Boundary: common prefill failure lifecycle only. */
static void prefill_result_cleanup(prefill_run *run,
                                   int attempted,
                                   const char *status)
{
    if (run->kv) {
        prefill_run_cleanup(run);
        return;
    }
    run->out->cleanup_attempted = attempted;
    run->out->cleanup_status = status ? status : (attempted ? "pass" : "not-needed");
    prefill_cleanup_scratch(&run->scratch, run->out);
}

/* Purpose: initialize every visible prefill summary field before admission.
 * Inputs: active run with immutable options and writable summary.
 * Effects: zeroes and populates baseline lifecycle/capability facts.
 * Failure: none after public argument validation.
 * Boundary: initialization only; no graph execution, allocation, or I/O. */
static const yvex_prefill_state_summary prefill_summary_default = {
    .prefill_state_kind = "segment-summary",
    .sequence_execution_mode = "independent-token-segments",
    .prefill_phase = "preflight",
    .backend_name = "none",
    .segment_name = "embedding-rmsnorm",
    .cleanup_status = "not-needed",
    .generation_status = "unsupported",
    .chunk_execution_mode = "token-loop",
    .context_boundary_status = "unchecked",
    .prefill_scratch_kind = "host-diagnostic-reuse",
    .prefill_scratch_cleanup_status = "not-needed",
    .kv_binding_kind = "none",
    .kv_binding_source = "segment-output-sample",
    .kv_status = "not-requested",
    .kv_owner = "none",
    .kv_dtype = "none",
    .kv_overflow_status = "not-checked",
    .kv_cleanup_status = "not-needed",
    .layer_execution_kind = "none",
    .layer_input_projection = "none",
    .layer_handoff = "none",
    .layer_sequence_rebuild = "none"
};

/* Purpose: Construct the admitted prefill summary init state only after its identities and resources are valid.
 * Inputs: A validated configuration, checked resource limits, and caller-owned result storage.
 * Effects: Mutates only the admitted destination or transaction after every precondition passes.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
static void prefill_summary_init(prefill_run *run)
{
    yvex_prefill_state_summary *out = run->out;

    *out = prefill_summary_default;
    if (run->layer_requested) {
        out->prefill_state_kind = "layer-backed-segment-summary";
        out->sequence_execution_mode = "segment-then-controlled-layer-fixture";
        out->kv_binding_source = "layer-final-sample";
        out->layer_execution_kind = "controlled-layer-fixture";
        out->layer_input_projection = "segment-sample-prefix";
        out->layer_handoff = "selected-position-row";
        out->layer_sequence_rebuild = "deterministic-with-previous-position-row";
    }
    out->chunked_prefill_requested = run->chunked;
    out->chunk_execution_mode = run->chunked ? "bounded-token-chunks" : "token-loop";
    out->chunk_size = run->options->chunk_size;
    out->prefill_scratch_reuse = run->chunked;
    out->kv_binding_kind = run->options->attach_kv ? "minimal-diagnostic" : "none";
    out->kv_status = run->options->attach_kv ? "planned" : "not-requested";
    out->layer_prefill_requested = run->layer_requested;
    out->layer_count = run->options->layer_count;
}

/* Purpose: admit immutable segment, layer, token, position, and context facts.
 * Inputs: initialized run and validated-token metadata.
 * Effects: records admitted context bounds and selected segment.
 * Failure: typed invalid-argument, state, or bounds refusal with no allocations.
 * Boundary: preflight only; it does not execute or read model payload. */
static int prefill_admit(prefill_run *run)
{
    unsigned long long context_length = 0ull;
    unsigned long long default_context_length = 0ull;

    if (run->options->segment_name) {
        run->segment_name = run->options->segment_name;
    }
    if (strcmp(run->segment_name, "embedding-rmsnorm") != 0) {
        return prefill_error(run, YVEX_ERR_INVALID_ARG,
                             "unsupported prefill segment; expected embedding-rmsnorm");
    }
    if (run->layer_requested &&
        (run->options->layer_count > 16ull || run->options->layer_hidden_dim == 0ull ||
         run->options->layer_head_dim == 0ull || run->options->layer_ffn_dim == 0ull)) {
        return prefill_error(
            run, YVEX_ERR_INVALID_ARG,
            "layer-backed prefill requires 1 <= layers <= 16 and positive layer dimensions");
    }
    if (run->layer_requested &&
        run->options->layer_hidden_dim > YVEX_SEGMENT_GRAPH_MAX_OUTPUT_VALUES) {
        return prefill_error(
            run, YVEX_ERR_BOUNDS,
            "layer-backed prefill hidden dimension exceeds segment sample capacity");
    }
    run->out->segment_name = run->segment_name;
    run->out->token_count = run->input->token_count;
    run->out->position_start = run->options->position_start;
    run->out->position_end = run->options->position_start;
    if (run->input->token_count == 0ull) {
        return prefill_error(run, YVEX_ERR_INVALID_ARG, "token-list-empty");
    }
    if (!run->input->token_bounds_checked || !run->input->token_bounds_valid) {
        return prefill_error(run, YVEX_ERR_STATE,
                             "validated token input is required before prefill state creation");
    }
    run->out->prefill_phase = "context-boundary";
    if (run->options->chunk_size > run->input->token_count) {
        return prefill_error(run, YVEX_ERR_INVALID_ARG, "chunk size cannot exceed token count");
    }
    if (run->input->token_count > ULLONG_MAX - run->options->position_start) {
        run->out->context_boundary_status = "fail";
        run->out->position_end = 0ull;
        return prefill_error(run, YVEX_ERR_BOUNDS, "prefill position range overflow");
    }
    run->out->position_end = run->options->position_start + run->input->token_count - 1ull;
    if (run->options->context_length > 0ull) {
        context_length = run->options->context_length;
    } else if (run->engine->model && yvex_model_context_length(run->engine->model) > 0ull) {
        context_length = yvex_model_context_length(run->engine->model);
    } else if (!yvex_core_u64_add(run->options->position_start,
                                        run->input->token_count,
                                        &default_context_length)) {
        run->out->context_boundary_status = "fail";
        run->out->position_end = 0ull;
        return prefill_error(run, YVEX_ERR_BOUNDS, "default prefill context length overflow");
    } else {
        context_length = default_context_length;
    }
    run->out->context_length = context_length;
    if (run->input->token_count > context_length ||
        run->options->position_start > context_length - run->input->token_count) {
        run->out->context_boundary_status = "fail";
        return prefill_error(run, YVEX_ERR_BOUNDS,
                             "prefill token positions exceed context length");
    }
    run->out->context_boundary_status = "pass";
    return YVEX_OK;
}

/* Purpose: allocate the one reusable bounded chunk scratch buffer.
 * Inputs: admitted run and chunk policy.
 * Effects: allocates scratch, records capacity/count, and exercises fault seams.
 * Failure: typed bounds, allocation, or injected backend failure with cleanup.
 * Boundary: owns host diagnostic scratch only. */
static int prefill_scratch_prepare(prefill_run *run)
{
    if (!run->chunked) {
        return YVEX_OK;
    }
    run->out->chunk_count = prefill_chunk_count(run->input->token_count,
                                                run->options->chunk_size);
    run->effective_chunk_size = run->options->chunk_size;
    if (yvex_core_test_flag("YVEX_TEST_FAIL_PREFILL_CHUNK_SCRATCH_ALLOC")) {
        run->out->prefill_phase = "scratch-allocation";
        return prefill_error(run, YVEX_ERR_NOMEM,
                             "test prefill chunk scratch allocation failure");
    }
    run->out->prefill_phase = "scratch-allocation";
    run->scratch_slots = run->effective_chunk_size > 0ull ? run->effective_chunk_size : 1ull;
    if (run->scratch_slots > (unsigned long long)(SIZE_MAX / sizeof(float))) {
        return prefill_error(run, YVEX_ERR_BOUNDS, "prefill scratch byte count overflow");
    }
    run->scratch = (float *)calloc((size_t)run->scratch_slots, sizeof(float));
    if (!run->scratch) {
        return prefill_error(run, YVEX_ERR_NOMEM, "failed to allocate prefill chunk scratch");
    }
    run->out->prefill_scratch_allocations = 1ull;
    if (yvex_core_test_flag("YVEX_TEST_FAIL_PREFILL_CHUNK_AFTER_SCRATCH_ALLOC")) {
        prefill_cleanup_scratch(&run->scratch, run->out);
        return prefill_error(run, YVEX_ERR_BACKEND,
                             "test prefill chunk failure after scratch allocation");
    }
    return YVEX_OK;
}

/* Purpose: create the optional temporary diagnostic KV cache and staging buffers.
 * Inputs: admitted run and immutable KV shape.
 * Effects: allocates cache/buffers and projects the canonical KV summary.
 * Failure: typed capacity, allocation, KV lifecycle, or injected failure; unwinds owners.
 * Boundary: temporary diagnostic KV, not persistent attention-backed runtime KV. */
static int prefill_kv_prepare(prefill_run *run)
{
    yvex_kv_summary summary;
    int rc;

    if (!run->options->attach_kv) {
        return YVEX_OK;
    }
    run->out->prefill_phase = "kv-preflight";
    run->out->kv_layers = run->options->kv_shape.layer_count;
    run->out->kv_heads = run->options->kv_shape.kv_head_count;
    run->out->kv_head_dim = run->options->kv_shape.head_dim;
    run->out->kv_capacity = run->options->kv_shape.capacity;
    if (run->options->kv_shape.capacity < run->input->token_count) {
        run->out->kv_overflow_status = "capacity-too-small";
        prefill_cleanup_scratch(&run->scratch, run->out);
        return prefill_error(run, YVEX_ERR_BOUNDS, "KV capacity is smaller than token count");
    }
    if (yvex_core_test_flag("YVEX_TEST_FAIL_PREFILL_KV_ALLOC")) {
        run->out->prefill_phase = "kv-allocation";
        run->out->kv_status = "fail";
        prefill_cleanup_scratch(&run->scratch, run->out);
        return prefill_error(run, YVEX_ERR_NOMEM, "test prefill KV allocation failure");
    }
    run->out->prefill_phase = "kv-allocation";
    rc = yvex_kv_cache_create_shape(&run->kv, &run->options->kv_shape, run->err);
    if (rc != YVEX_OK) {
        run->out->kv_status = "fail";
        prefill_cleanup_scratch(&run->scratch, run->out);
        return rc;
    }
    rc = yvex_kv_cache_get_summary(run->kv, &summary, run->err);
    if (rc != YVEX_OK) {
        prefill_run_cleanup(run);
        return rc;
    }
    prefill_summary_apply_kv(&summary, run->out);
    run->kv_value_count = yvex_kv_cache_position_value_count(run->kv);
    run->kv_values = (float *)calloc((size_t)run->kv_value_count, sizeof(float));
    run->kv_read_values = (float *)calloc((size_t)run->kv_value_count, sizeof(float));
    if (!run->kv_values || !run->kv_read_values) {
        prefill_run_cleanup(run);
        return prefill_error(run, YVEX_ERR_NOMEM,
                             "failed to allocate prefill KV diagnostic buffers");
    }
    return YVEX_OK;
}

/* Purpose: execute one admitted segment and account its exact contribution.
 * Inputs: active run plus token/chunk indexes and writable segment result.
 * Effects: executes graph work, updates counters/checksums, and reuses chunk scratch.
 * Failure: graph or accounting refusal records indexes and deterministically cleans owners.
 * Boundary: one token's segment stage only. */
static int prefill_segment_execute(prefill_run *run,
                                   unsigned long long token_index,
                                   unsigned long long chunk_index,
                                   unsigned long long chunk_start,
                                   yvex_segment_graph_result *result)
{
    yvex_segment_graph_options options;
    int rc;

    memset(&options, 0, sizeof(options));
    memset(result, 0, sizeof(*result));
    options.segment_name = run->segment_name;
    options.token_id = run->input->tokens[token_index];
    run->out->prefill_phase = run->chunked ? "chunk-execution" : "token-execution";
    rc = yvex_engine_execute_segment_graph(run->engine, &options, result, run->err);
    if (rc != YVEX_OK) {
        prefill_mark_failed(run, token_index, chunk_index);
        prefill_result_cleanup(run, result->cleanup_attempted, result->cleanup_status);
        return rc;
    }
    run->out->segment_graph_executions += 1ull;
    run->out->tokens_processed += 1ull;
    run->out->segment_output_count = result->segment_output_count;
    run->out->segment_output_bytes = result->segment_output_bytes;
    if (run->chunked && run->scratch) {
        run->scratch[(token_index - chunk_start) % run->scratch_slots] =
            (float)((double)run->input->tokens[token_index] +
                    ((double)result->output_checksum * 0.000000000000001));
        run->out->prefill_scratch_reuse_count += 1ull;
    }
    if (!yvex_core_u64_add(run->out->total_output_bytes,
                                 result->segment_output_bytes,
                                 &run->out->total_output_bytes) ||
        !yvex_core_u64_add(run->out->scratch_bytes, result->segment_scratch_bytes,
                                 &run->out->scratch_bytes)) {
        prefill_mark_failed(run, token_index, chunk_index);
        prefill_default_cleanup(run);
        return prefill_error(run, YVEX_ERR_BOUNDS, "prefill byte accounting overflow");
    }
    run->out->final_token_checksum = result->output_checksum;
    run->aggregate = yvex_core_hash_mix_u64(
        run->aggregate, (unsigned long long)run->input->tokens[token_index]);
    run->aggregate = yvex_core_hash_mix_u64(run->aggregate, result->output_checksum);
    run->aggregate = yvex_core_hash_mix_u64(run->aggregate, result->reference_checksum);
    if (run->chunked) {
        run->chunk_checksum = yvex_core_hash_mix_u64(
            run->chunk_checksum, (unsigned long long)run->input->tokens[token_index]);
        run->chunk_checksum = yvex_core_hash_mix_u64(run->chunk_checksum,
                                                     result->output_checksum);
    }
    if (result->max_abs_diff > run->out->max_abs_diff) {
        run->out->max_abs_diff = result->max_abs_diff;
    }
    return YVEX_OK;
}

/* Purpose: execute the optional controlled layer fixture for one token.
 * Inputs: active run, indexes, admitted segment result, writable layer result.
 * Effects: seeds, executes, accounts, and hashes the layer fixture result.
 * Failure: shape, accounting, graph, state, or injected failure with exact cleanup facts.
 * Boundary: diagnostic layer fixture only; no full transformer prefill. */
static int prefill_layer_execute(prefill_run *run,
                                 unsigned long long token_index,
                                 unsigned long long chunk_index,
                                 const yvex_segment_graph_result *segment,
                                 yvex_graph_layer_fixture_result *result)
{
    yvex_graph_layer_fixture_options options;
    unsigned long long i;
    int rc;

    if (segment->output_value_count < run->options->layer_hidden_dim) {
        prefill_mark_failed(run, token_index, chunk_index);
        prefill_default_cleanup(run);
        return prefill_error(run, YVEX_ERR_BOUNDS,
                             "segment output sample is too small for layer-backed prefill");
    }
    for (i = 0; i < run->options->layer_hidden_dim; ++i) {
        run->layer_seed[i] = segment->output_values[i];
    }
    memset(&options, 0, sizeof(options));
    memset(result, 0, sizeof(*result));
    options.backend_name = run->out->backend_name;
    options.layers = run->options->layer_count;
    options.seq_len = run->input->token_count;
    options.position = token_index;
    options.hidden_dim = run->options->layer_hidden_dim;
    options.head_dim = run->options->layer_head_dim;
    options.ffn_dim = run->options->layer_ffn_dim;
    options.initial_position_values = run->layer_seed;
    options.initial_position_value_count = run->options->layer_hidden_dim;
    run->out->prefill_phase = "layer-execution";
    rc = yvex_graph_execute_layer_fixture(&options, result, run->err);
    run->out->layer_graph_executions += 1ull;
    run->out->layer_block_executions += result->layers;
    run->out->layer_total_op_count += result->total_op_count;
    run->out->layer_output_count = result->output_value_count;
    run->out->layer_output_bytes = result->output_bytes;
    run->out->layer_final_checksum = result->final_output_checksum;
    run->out->layer_final_reference_checksum = result->final_reference_checksum;
    run->out->layer_max_abs_diff = result->final_max_abs_diff;
    run->out->layer_output_sample_count = result->output_value_count;
    memcpy(run->out->layer_output_sample_values, result->output_values,
           (size_t)result->output_value_count * sizeof(result->output_values[0]));
    if (!yvex_core_u64_add(run->out->layer_total_output_bytes, result->output_bytes,
                                 &run->out->layer_total_output_bytes) ||
        !yvex_core_u64_add(run->out->layer_total_scratch_bytes,
                                 result->scratch_bytes,
                                 &run->out->layer_total_scratch_bytes) ||
        !yvex_core_u64_add(run->out->total_output_bytes, result->output_bytes,
                                 &run->out->total_output_bytes) ||
        !yvex_core_u64_add(run->out->scratch_bytes, result->scratch_bytes,
                                 &run->out->scratch_bytes)) {
        prefill_mark_failed(run, token_index, chunk_index);
        prefill_default_cleanup(run);
        return prefill_error(run, YVEX_ERR_BOUNDS,
                             "layer-backed prefill byte accounting overflow");
    }
    if (rc != 0) {
        yvex_status status = yvex_error_code(run->err);
        prefill_mark_failed(run, token_index, chunk_index);
        prefill_result_cleanup(run, result->cleanup_attempted, result->cleanup_status);
        if (status == YVEX_OK) {
            status = prefill_error(run, YVEX_ERR_STATE,
                                   "layer-backed prefill fixture execution failed");
        }
        return status;
    }
    run->aggregate = yvex_core_hash_mix_u64(run->aggregate, result->final_output_checksum);
    run->aggregate = yvex_core_hash_mix_u64(run->aggregate,
                                            result->final_reference_checksum);
    if (run->chunked) {
        run->chunk_checksum = yvex_core_hash_mix_u64(run->chunk_checksum,
                                                     result->final_output_checksum);
    }
    run->out->final_token_checksum = result->final_output_checksum;
    if (result->final_max_abs_diff > run->out->max_abs_diff) {
        run->out->max_abs_diff = result->final_max_abs_diff;
    }
    if (yvex_core_test_flag("YVEX_TEST_FAIL_PREFILL_LAYERS_AFTER_LAYER_EXECUTION")) {
        prefill_mark_failed(run, token_index, chunk_index);
        run->out->prefill_phase = "layer-execution-complete";
        prefill_result_cleanup(run, result->cleanup_attempted,
                               result->cleanup_status ? result->cleanup_status : "pass");
        return prefill_error(run, YVEX_ERR_BACKEND,
                             "test prefill layer failure after layer execution");
    }
    return YVEX_OK;
}

/* Purpose: append one exact diagnostic position to the optional KV owner.
 * Inputs: active run, indexes, and selected segment/layer sample.
 * Effects: fills staging, appends KV, updates checksum and lifecycle summary.
 * Failure: typed KV/state/injected failure with complete runtime cleanup.
 * Boundary: one temporary KV append; no persistent cache publication. */
static int prefill_kv_append(prefill_run *run,
                             unsigned long long token_index,
                             unsigned long long chunk_index,
                             const float *source,
                             unsigned long long source_count)
{
    yvex_kv_summary summary;
    unsigned long long position = 0ull;
    int rc;

    if (!run->kv) {
        return YVEX_OK;
    }
    run->out->prefill_phase = "kv-append";
    fill_prefill_kv_values(run->kv_values, run->kv_value_count, source, source_count,
                           run->input->tokens[token_index],
                           run->options->position_start + token_index);
    rc = yvex_kv_cache_append_position_f32(run->kv, run->kv_values, run->kv_value_count,
                                           &position, run->err);
    if (rc != YVEX_OK) {
        prefill_mark_failed(run, token_index, chunk_index);
        prefill_run_cleanup(run);
        return rc;
    }
    if (position != token_index) {
        prefill_mark_failed(run, token_index, chunk_index);
        prefill_run_cleanup(run);
        return prefill_error(run, YVEX_ERR_STATE,
                             "KV append position did not match token index");
    }
    if (run->chunked) {
        run->chunk_checksum = yvex_core_hash_mix_u64(run->chunk_checksum, position);
    }
    rc = yvex_kv_cache_get_summary(run->kv, &summary, run->err);
    if (rc != YVEX_OK) {
        prefill_mark_failed(run, token_index, chunk_index);
        prefill_run_cleanup(run);
        return rc;
    }
    prefill_summary_apply_kv(&summary, run->out);
    if (yvex_core_test_flag("YVEX_TEST_FAIL_PREFILL_KV_AFTER_APPEND_0") &&
        token_index == 0ull) {
        prefill_mark_failed(run, token_index + 1ull, chunk_index);
        prefill_run_cleanup(run);
        return prefill_error(run, YVEX_ERR_BACKEND,
                             "test prefill KV failure after append 0");
    }
    return YVEX_OK;
}

/* Purpose: exercise post-token fault seams at their historical lifecycle point.
 * Inputs: active run plus completed token/chunk indexes.
 * Effects: may mark failure and unwind all temporary ownership.
 * Failure: returns the exact injected backend refusal.
 * Boundary: test seam only; successful execution is unchanged. */
static int prefill_post_token_faults(prefill_run *run,
                                     unsigned long long token_index,
                                     unsigned long long chunk_index)
{
    if (run->layer_requested &&
        yvex_core_test_flag("YVEX_TEST_FAIL_PREFILL_LAYERS_AFTER_TOKEN_0") &&
        token_index == 0ull) {
        prefill_mark_failed(run, token_index + 1ull, chunk_index);
        run->out->prefill_phase = "layer-token-complete";
        prefill_default_cleanup(run);
        return prefill_error(run, YVEX_ERR_BACKEND,
                             "test prefill layer failure after token 0");
    }
    if (yvex_core_test_flag("YVEX_TEST_FAIL_PREFILL_AFTER_TOKEN_0") &&
        token_index == 0ull) {
        prefill_mark_failed(run, token_index + 1ull, chunk_index);
        prefill_default_cleanup(run);
        return prefill_error(run, YVEX_ERR_BACKEND, "test prefill failure after token 0");
    }
    return YVEX_OK;
}

/* Purpose: compose every admitted per-token prefill stage in order.
 * Inputs: active run and token/chunk coordinates.
 * Effects: executes segment, optional layer, optional KV, then fault seams.
 * Failure: propagates the first typed stage refusal after its own cleanup.
 * Boundary: one token transaction; chunk commit occurs separately. */
static int prefill_token_execute(prefill_run *run,
                                 unsigned long long token_index,
                                 unsigned long long chunk_index,
                                 unsigned long long chunk_start)
{
    yvex_segment_graph_result segment;
    yvex_graph_layer_fixture_result layer;
    const float *kv_source;
    unsigned long long kv_source_count;
    int rc;

    rc = prefill_segment_execute(run, token_index, chunk_index, chunk_start, &segment);
    if (rc != YVEX_OK) {
        return rc;
    }
    kv_source = segment.output_values;
    kv_source_count = segment.output_value_count;
    if (run->layer_requested) {
        rc = prefill_layer_execute(run, token_index, chunk_index, &segment, &layer);
        if (rc != YVEX_OK) {
            return rc;
        }
        kv_source = layer.output_values;
        kv_source_count = layer.output_value_count;
    }
    rc = prefill_kv_append(run, token_index, chunk_index, kv_source, kv_source_count);
    if (rc != YVEX_OK) {
        return rc;
    }
    return prefill_post_token_faults(run, token_index, chunk_index);
}

/* Purpose: execute one deterministic bounded token chunk.
 * Inputs: active run, first token index, and chunk index.
 * Effects: executes tokens in order and commits chunk checksum/counter facts.
 * Failure: propagates token refusal or injected post-chunk failure with cleanup.
 * Boundary: bounded scheduling only; it does not retain per-chunk payload. */
static int prefill_chunk_execute(prefill_run *run,
                                 unsigned long long chunk_start,
                                 unsigned long long chunk_index)
{
    unsigned long long chunk_end = chunk_start + run->effective_chunk_size - 1ull;
    unsigned long long i;
    int rc;

    if (chunk_end >= run->input->token_count) {
        chunk_end = run->input->token_count - 1ull;
    }
    if (run->chunked) {
        run->out->current_chunk_start = run->options->position_start + chunk_start;
        run->out->current_chunk_end = run->options->position_start + chunk_end;
        run->chunk_checksum = 1469598103934665603ull;
        run->chunk_checksum = yvex_core_hash_mix_u64(run->chunk_checksum, chunk_index);
        run->chunk_checksum = yvex_core_hash_mix_u64(run->chunk_checksum,
                                                     run->out->current_chunk_start);
        run->chunk_checksum = yvex_core_hash_mix_u64(run->chunk_checksum,
                                                     run->out->current_chunk_end);
    }
    for (i = chunk_start; i <= chunk_end; ++i) {
        rc = prefill_token_execute(run, i, chunk_index, chunk_start);
        if (rc != YVEX_OK) {
            return rc;
        }
    }
    if (!run->chunked) {
        return YVEX_OK;
    }
    run->out->final_chunk_checksum = run->chunk_checksum;
    run->aggregate = yvex_core_hash_mix_u64(run->aggregate, run->chunk_checksum);
    run->out->chunks_processed += 1ull;
    if (yvex_core_test_flag("YVEX_TEST_FAIL_PREFILL_CHUNK_AFTER_CHUNK_0") &&
        chunk_index == 0ull) {
        run->out->failed_chunk_index = 1ull;
        run->out->prefill_phase = "chunk-execution";
        prefill_default_cleanup(run);
        return prefill_error(run, YVEX_ERR_BACKEND,
                             "test prefill chunk failure after chunk 0");
    }
    return YVEX_OK;
}

/* Purpose: traverse every chunk in the canonical token order.
 * Inputs: active admitted run with effective chunk size.
 * Effects: invokes each chunk transaction exactly once.
 * Failure: stops at and propagates the first typed chunk refusal.
 * Boundary: scheduling wrapper with no independent ownership. */
static int prefill_execute(prefill_run *run)
{
    unsigned long long chunk_start;
    unsigned long long chunk_index;
    int rc;

    for (chunk_start = 0ull, chunk_index = 0ull;
         chunk_start < run->input->token_count;
         chunk_start += run->effective_chunk_size, ++chunk_index) {
        rc = prefill_chunk_execute(run, chunk_start, chunk_index);
        if (rc != YVEX_OK) {
            return rc;
        }
    }
    return YVEX_OK;
}

/* Purpose: read back position zero and finalize optional diagnostic KV evidence.
 * Inputs: completed run with optional live KV owner.
 * Effects: copies bounded samples, updates summary, closes KV, frees staging.
 * Failure: typed KV read/summary refusal with deterministic full cleanup.
 * Boundary: final diagnostic KV observation; no persistent state escapes. */
static int prefill_kv_finish(prefill_run *run)
{
    yvex_kv_summary summary;
    unsigned long long sample_count;
    int rc;

    if (!run->kv) {
        return YVEX_OK;
    }
    run->out->prefill_phase = "kv-readback";
    rc = yvex_kv_cache_read_position_f32(run->kv, 0ull, run->kv_read_values,
                                         run->kv_value_count, run->err);
    if (rc != YVEX_OK) {
        prefill_run_cleanup(run);
        return rc;
    }
    run->out->kv_read_position = 0ull;
    run->out->kv_read_value_count = run->kv_value_count;
    run->out->kv_read_checksum = prefill_checksum_f32_values(run->kv_read_values,
                                                             run->kv_value_count);
    sample_count = run->kv_value_count < YVEX_PREFILL_KV_MAX_SAMPLE_VALUES
                       ? run->kv_value_count
                       : YVEX_PREFILL_KV_MAX_SAMPLE_VALUES;
    run->out->kv_read_sample_count = sample_count;
    memcpy(run->out->kv_read_sample_values, run->kv_read_values,
           (size_t)sample_count * sizeof(run->kv_read_values[0]));
    rc = yvex_kv_cache_get_summary(run->kv, &summary, run->err);
    if (rc != YVEX_OK) {
        prefill_run_cleanup(run);
        return rc;
    }
    prefill_summary_apply_kv(&summary, run->out);
    run->out->kv_ready = 1;
    run->out->kv_bound_to_prefill = 1;
    run->out->kv_cleanup_status = "pass";
    yvex_kv_cache_close(run->kv);
    run->kv = NULL;
    free(run->kv_values);
    free(run->kv_read_values);
    run->kv_values = NULL;
    run->kv_read_values = NULL;
    return YVEX_OK;
}

/* Purpose: construct the implemented bounded diagnostic prefill state.
 * Inputs: admitted engine, validated token input, immutable options, output summary.
 * Effects: executes segment/layer fixtures and optionally owns a temporary KV cache.
 * Failure: returns typed admission, allocation, bounds, state, or backend errors; cleans owners.
 * Boundary: does not implement transformer prefill, persistent KV, decode, or generation. */
int yvex_engine_create_prefill_state(yvex_engine *engine,
                                     const yvex_prefill_state_options *options,
                                     yvex_prefill_state_summary *out,
                                     yvex_error *err)
{
    prefill_run run;
    int rc;

    if (!engine || !options || !options->token_input || !out) {
        return yvex_generation_refuse(
            err, YVEX_ERR_INVALID_ARG, "yvex_engine_create_prefill_state",
            "engine, options, token input, and out are required");
    }
    memset(&run, 0, sizeof(run));
    run.engine = engine;
    run.options = options;
    run.out = out;
    run.err = err;
    run.input = options->token_input;
    run.segment_name = "embedding-rmsnorm";
    run.aggregate = 1469598103934665603ull;
    run.chunk_checksum = 1469598103934665603ull;
    run.effective_chunk_size = 1ull;
    run.layer_requested = options->layer_count > 0ull;
    run.chunked = options->chunk_size > 0ull;
    prefill_summary_init(&run);
    rc = prefill_admit(&run);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = prefill_scratch_prepare(&run);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = prefill_kv_prepare(&run);
    if (rc != YVEX_OK) {
        return rc;
    }
    if (engine->weight_backend) {
        out->backend_name = yvex_backend_kind_name(yvex_backend_kind_of(engine->weight_backend));
    }
    rc = prefill_execute(&run);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = prefill_kv_finish(&run);
    if (rc != YVEX_OK) {
        return rc;
    }
    prefill_cleanup_scratch(&run.scratch, out);
    out->aggregate_checksum = run.aggregate;
    out->prefill_state_created = 1;
    out->prefill_phase = "complete";
    out->cleanup_status = "not-needed";
    out->cuda_parity = out->backend_name && strcmp(out->backend_name, "cuda") == 0;
    yvex_error_clear(err);
    return YVEX_OK;
}
