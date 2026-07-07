/*
 * yvex_sampling.c - bounded diagnostic sampling domain.
 *
 * Owner:
 *   src/generation
 *
 * Owns:
 *   bounded greedy sampling over the existing diagnostic logits buffer,
 *   sampling summary defaults, checksum mixing, and sampler execution.
 *
 * Does not own:
 *   command dispatch, input parsing, report construction, text rendering,
 *   stdout/stderr output, graph guard orchestration, model reference
 *   resolution, token input parsing, stochastic sampling, generation, eval,
 *   benchmark, or release decisions.
 *
 * Invariants:
 *   this file does not print and does not claim real vocabulary sampling.
 *
 * Boundary:
 *   bounded greedy sampling is diagnostic sampling only; it is not runtime
 *   generation or full-model sampling support.
 */
#include "yvex_sampling_private.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int sample_test_env_enabled(const char *name)
{
    const char *value = getenv(name);

    return value && value[0] != '\0' && strcmp(value, "0") != 0;
}

static unsigned long long sample_mix_u64(unsigned long long hash,
                                         unsigned long long value)
{
    unsigned int i;

    for (i = 0; i < 8u; ++i) {
        hash ^= (value >> (i * 8u)) & 0xffull;
        hash *= 1099511628211ull;
    }
    return hash;
}

static unsigned long long sample_mix_float(unsigned long long hash,
                                           double value)
{
    float narrowed = (float)value;
    uint32_t bits = 0u;

    memcpy(&bits, &narrowed, sizeof(bits));
    return sample_mix_u64(hash, (unsigned long long)bits);
}

const char *yvex_sampling_strategy_name(yvex_sampling_strategy strategy)
{
    switch (strategy) {
    case YVEX_SAMPLING_STRATEGY_GREEDY:
        return "greedy";
    }
    return "unknown";
}

void sample_summary_defaults(yvex_sampling_summary *out,
                             const yvex_sampling_options *options)
{
    const yvex_logits_buffer_options *logits_options;
    const yvex_decode_step_options *decode_options;

    if (!out) {
        return;
    }
    memset(out, 0, sizeof(*out));
    logits_options = options ? options->logits_options : NULL;
    decode_options = logits_options ? logits_options->decode_options : NULL;
    out->sampler_kind = "bounded-diagnostic";
    out->sampling_phase = "preflight";
    out->sampling_strategy =
        options ? yvex_sampling_strategy_name(options->strategy) : "greedy";
    out->sampling_source = "bounded-logits-buffer";
    out->backend_name = decode_options && decode_options->backend_name
                            ? decode_options->backend_name
                            : "cpu";
    out->logits_buffer_kind = "none";
    out->logits_phase = "not-started";
    out->tie_break = "lowest-index";
    out->cleanup_status = "not-needed";
    out->generation_status = "unsupported";
}

static void sample_copy_logits_summary(yvex_sampling_summary *out,
                                       const yvex_logits_buffer_summary *logits)
{
    if (!out || !logits) {
        return;
    }
    out->backend_name =
        logits->backend_name ? logits->backend_name : out->backend_name;
    out->logits_buffer_created = logits->logits_buffer_created;
    out->logits_buffer_kind = logits->logits_buffer_kind
                                  ? logits->logits_buffer_kind
                                  : "bounded-diagnostic";
    out->logits_phase = logits->logits_phase ? logits->logits_phase : "unknown";
    out->logits_count = logits->logits_count;
    out->logits_checksum = logits->logits_checksum;
    out->logits_min = logits->logits_min;
    out->logits_max = logits->logits_max;
}

static unsigned long long sample_checksum(const yvex_sampling_summary *summary)
{
    unsigned long long hash = 1469598103934665603ull;

    hash = sample_mix_u64(hash, summary->logits_checksum);
    hash = sample_mix_u64(hash, summary->selected_logit_index);
    hash = sample_mix_u64(hash, (unsigned long long)summary->selected_token_id);
    hash = sample_mix_float(hash, summary->selected_logit);
    return hash;
}

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
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_engine_sample_token",
                       "engine, options, logits options, and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    sample_summary_defaults(out, options);
    if (options->strategy != YVEX_SAMPLING_STRATEGY_GREEDY) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_engine_sample_token",
                       "only greedy sampling is implemented");
        return YVEX_ERR_INVALID_ARG;
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
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_engine_sample_token",
                       "logits buffer was not created");
        return YVEX_ERR_STATE;
    }

    if (sample_test_env_enabled("YVEX_TEST_FAIL_SAMPLE_AFTER_LOGITS")) {
        out->sampling_phase = "after-logits";
        yvex_error_set(err, YVEX_ERR_BACKEND, "yvex_engine_sample_token",
                       "test sampling failure after logits");
        return YVEX_ERR_BACKEND;
    }

    out->sampling_phase = "select";
    candidate_count = logits_summary.logits_sample_count;
    if (candidate_count == 0ull) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_engine_sample_token",
                       "logits buffer has no sample values to scan");
        return YVEX_ERR_STATE;
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
    if (sample_test_env_enabled("YVEX_TEST_FAIL_SAMPLE_AFTER_SELECT")) {
        out->sampling_phase = "after-select";
        yvex_error_set(err, YVEX_ERR_BACKEND, "yvex_engine_sample_token",
                       "test sampling failure after select");
        return YVEX_ERR_BACKEND;
    }

    out->sample_checksum = sample_checksum(out);
    out->sample_executed = 1;
    out->bounded_sampling_ready = 1;
    out->sampling_phase = "complete";
    yvex_error_clear(err);
    return YVEX_OK;
}
