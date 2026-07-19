/* Owner: src/generation
 * Owns: bounded greedy sampling over the existing diagnostic logits buffer, sampling summary defaults, checksum
 *   mixing, and sampler execution.
 * Does not own: command dispatch, input parsing, report construction, text rendering, stdout/stderr output, graph
 *   guard orchestration, model reference resolution, token input parsing, stochastic sampling,
 *   generation, eval, benchmark, or release decisions.
 * Invariants: this file does not print and does not claim real vocabulary sampling.
 * Boundary: bounded greedy sampling is diagnostic sampling only; it is not runtime generation or full-model
 *   sampling support.
 * Purpose: Select one deterministic diagnostic token from an admitted logits buffer.
 * Inputs: Immutable logits, greedy sampling policy, and caller-owned result storage.
 * Effects: Writes only the selected token, score, and evidence checksum.
 * Failure: Invalid or non-finite inputs refuse without a selected-token claim. */
#include <yvex/internal/core.h>
#include <yvex/internal/generation.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Purpose: Return the canonical diagnostic label for sampling strategy name.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Does not mutate caller-visible or owner state.
 * Failure: Returns the canonical unknown or zero sentinel for an invalid typed value.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
const char *yvex_sampling_strategy_name(yvex_sampling_strategy strategy)
{
    return strategy == YVEX_SAMPLING_STRATEGY_GREEDY ? "greedy" : "unknown";
}

static const yvex_sampling_summary sampling_summary_default = {
    .sampler_kind = "bounded-diagnostic",
    .sampling_phase = "preflight",
    .sampling_strategy = "greedy",
    .sampling_source = "bounded-logits-buffer",
    .backend_name = "cpu",
    .logits_buffer_kind = "none",
    .logits_phase = "not-started",
    .tie_break = "lowest-index",
    .cleanup_status = "not-needed",
    .generation_status = "unsupported"
};

#define SAMPLE_FIELD(destination_, source_)                                             \
    {                                                                                   \
        offsetof(yvex_sampling_summary, destination_),                                  \
        offsetof(yvex_logits_buffer_summary, source_),                                  \
        sizeof(((yvex_logits_buffer_summary *)0)->source_) +                            \
            0u * sizeof(char[sizeof(((yvex_sampling_summary *)0)->destination_) ==       \
                                    sizeof(((yvex_logits_buffer_summary *)0)->source_)   \
                                ? 1                                                     \
                                : -1])                                                  \
    }

static const yvex_generation_field_projection sampling_logits_fields[] = {
    SAMPLE_FIELD(logits_buffer_created, logits_buffer_created),
    SAMPLE_FIELD(logits_count, logits_count),
    SAMPLE_FIELD(logits_checksum, logits_checksum),
    SAMPLE_FIELD(logits_min, logits_min),
    SAMPLE_FIELD(logits_max, logits_max),
};

#undef SAMPLE_FIELD

/* Purpose: Implement the canonical sampling summary defaults mechanism owned by the generation boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
void yvex_sampling_summary_defaults(yvex_sampling_summary *out,
                             const yvex_sampling_options *options)
{
    const yvex_logits_buffer_options *logits_options;
    const yvex_decode_step_options *decode_options;

    if (!out) {
        return;
    }
    *out = sampling_summary_default;
    logits_options = options ? options->logits_options : NULL;
    decode_options = logits_options ? logits_options->decode_options : NULL;
    out->sampling_strategy =
        options ? yvex_sampling_strategy_name(options->strategy) : "greedy";
    out->backend_name = decode_options && decode_options->backend_name
                            ? decode_options->backend_name
                            : "cpu";
}

/* Purpose: Copy sample copy logits summary between compatible admitted ranges without changing semantic identity. */
static void sample_copy_logits_summary(yvex_sampling_summary *out,
                                       const yvex_logits_buffer_summary *logits)
{
    if (!out || !logits) {
        return;
    }
    yvex_generation_project_fields(
        out, logits, sampling_logits_fields,
        sizeof(sampling_logits_fields) / sizeof(sampling_logits_fields[0]));
    out->backend_name =
        logits->backend_name ? logits->backend_name : out->backend_name;
    out->logits_buffer_kind = logits->logits_buffer_kind
                                  ? logits->logits_buffer_kind
                                  : "bounded-diagnostic";
    out->logits_phase = logits->logits_phase ? logits->logits_phase : "unknown";
}

/* Purpose: Select sample checksum deterministically from admitted logits and sampling policy. */
static unsigned long long sample_checksum(const yvex_sampling_summary *summary)
{
    unsigned long long hash = 1469598103934665603ull;

    hash = yvex_core_hash_mix_u64(hash, summary->logits_checksum);
    hash = yvex_core_hash_mix_u64(hash, summary->selected_logit_index);
    hash = yvex_core_hash_mix_u64(hash, (unsigned long long)summary->selected_token_id);
    hash = yvex_generation_hash_float(hash, summary->selected_logit);
    return hash;
}

/* Purpose: Select engine sample token deterministically from admitted logits and sampling policy.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Mutates only the admitted destination or transaction after every precondition passes.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
int yvex_engine_sample_token(yvex_engine *engine,
                             const yvex_sampling_options *options,
                             yvex_sampling_summary *out,
                             yvex_error *err)
{
    yvex_logits_buffer_summary logits_summary;
    unsigned long long i;
    unsigned long long candidate_count;
    double best;
    int rc;

    if (!engine || !options || !options->logits_options || !out) {
        return yvex_generation_refuse(
            err, YVEX_ERR_INVALID_ARG, "yvex_engine_sample_token",
            "engine, options, logits options, and out are required");
    }
    yvex_sampling_summary_defaults(out, options);
    if (options->strategy != YVEX_SAMPLING_STRATEGY_GREEDY) {
        return yvex_generation_refuse(err, YVEX_ERR_INVALID_ARG,
                                      "yvex_engine_sample_token",
                                      "only greedy sampling is implemented");
    }

    memset(&logits_summary, 0, sizeof(logits_summary));
    out->logits_invoked = 1;
    out->sampling_phase = "logits";
    rc = yvex_engine_create_logits_buffer(engine,
                                          options->logits_options,
                                          &logits_summary,
                                          err);
    sample_copy_logits_summary(out, &logits_summary);
    if (rc != YVEX_OK) {
        return rc;
    }
    if (!logits_summary.logits_buffer_created) {
        return yvex_generation_refuse(err, YVEX_ERR_STATE,
                                      "yvex_engine_sample_token",
                                      "logits buffer was not created");
    }

    rc = yvex_generation_test_refuse(
        "YVEX_TEST_FAIL_SAMPLE_AFTER_LOGITS", err, YVEX_ERR_BACKEND,
        "yvex_engine_sample_token", "test sampling failure after logits");
    if (rc != YVEX_OK) {
        out->sampling_phase = "after-logits";
        return rc;
    }

    out->sampling_phase = "select";
    candidate_count = logits_summary.logits_sample_count;
    if (candidate_count == 0ull) {
        return yvex_generation_refuse(err, YVEX_ERR_STATE,
                                      "yvex_engine_sample_token",
                                      "logits buffer has no sample values to scan");
    }
    best = (double)logits_summary.logits_sample_values[0];
    out->selected_logit_index = 0ull;
    for (i = 1ull; i < candidate_count; ++i) {
        double value = (double)logits_summary.logits_sample_values[i];
        if (value > best) {
            best = value;
            out->selected_logit_index = i;
        }
    }
    out->candidates_considered = candidate_count;
    out->selected_token_id = (unsigned int)out->selected_logit_index;
    out->selected_logit = best;
    out->sample_created = 1;
    rc = yvex_generation_test_refuse(
        "YVEX_TEST_FAIL_SAMPLE_AFTER_SELECT", err, YVEX_ERR_BACKEND,
        "yvex_engine_sample_token", "test sampling failure after select");
    if (rc != YVEX_OK) {
        out->sampling_phase = "after-select";
        return rc;
    }

    out->sample_checksum = sample_checksum(out);
    out->sample_executed = 1;
    out->bounded_sampling_ready = 1;
    out->sampling_phase = "complete";
    yvex_error_clear(err);
    return YVEX_OK;
}
