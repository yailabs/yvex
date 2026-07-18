/*
 * Owner: generation.logits (generation).
 * Owns: the reusable-algorithm boundary consumed by runtime,cli.
 * Does not own: unrelated subsystem policy or unsupported higher-stage claims.
 * Invariants: scope=generic and visibility=private match config/source_owners.tsv.
 * Boundary: reusable-algorithm; moving this contract requires an ownership-manifest change.
 *
 * logits.c - Logits runtime boundary.
 *
 * This file owns the session logits skeleton and a bounded diagnostic logits
 * buffer over decode state. It does not run the real model output head, sample,
 * generate, or benchmark.
 */

#include <yvex/logits.h>

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct yvex_logits {
    yvex_logits_summary summary;
};

static int logits_test_env_enabled(const char *name)
{
    const char *value = getenv(name);

    return value && value[0] != '\0' && strcmp(value, "0") != 0;
}

static unsigned long long logits_mix_u64(unsigned long long hash,
                                         unsigned long long value)
{
    unsigned int i;

    for (i = 0; i < 8u; ++i) {
        hash ^= (value >> (i * 8u)) & 0xffull;
        hash *= 1099511628211ull;
    }
    return hash;
}

static unsigned long long logits_mix_float(unsigned long long hash, float value)
{
    uint32_t bits = 0u;

    memcpy(&bits, &value, sizeof(bits));
    return logits_mix_u64(hash, (unsigned long long)bits);
}

int yvex_logits_count_valid(unsigned long long count)
{
    return count >= 1ull && count <= 256ull;
}

void yvex_logits_buffer_summary_init(yvex_logits_buffer_summary *out,
                                     const yvex_logits_buffer_options *options)
{
    const yvex_decode_step_options *decode_options;

    if (!out) {
        return;
    }
    memset(out, 0, sizeof(*out));
    decode_options = options ? options->decode_options : NULL;
    out->logits_buffer_kind = "bounded-diagnostic";
    out->logits_phase = "preflight";
    out->logits_source = "decode-state";
    out->backend_name = decode_options && decode_options->backend_name
                            ? decode_options->backend_name
                            : "cpu";
    out->decode_step_kind = "none";
    out->decode_phase = "not-started";
    out->cleanup_status = "not-needed";
    out->generation_status = "unsupported";
}

static void logits_copy_decode_summary(yvex_logits_buffer_summary *out,
                                       const yvex_decode_step_summary *decode)
{
    if (!out || !decode) {
        return;
    }
    out->backend_name = decode->backend_name ? decode->backend_name : out->backend_name;
    out->decode_state_created = decode->decode_state_created;
    out->decode_step_executed = decode->decode_step_executed;
    out->decode_step_kind = decode->decode_step_kind ? decode->decode_step_kind
                                                     : "bounded-diagnostic";
    out->decode_phase = decode->decode_phase ? decode->decode_phase : "unknown";
    out->decode_position = decode->decode_position;
    out->decode_state_checksum = decode->decode_state_checksum;
}

static float logits_value_from_decode(const yvex_decode_step_summary *decode,
                                      unsigned long long index,
                                      unsigned long long seed)
{
    unsigned long long local;
    unsigned long long word;
    double basis = 0.0;
    double signed_offset;
    double value;

    local = logits_mix_u64(seed, index);
    local = logits_mix_u64(local, decode->decode_position);
    if (decode->decode_state_value_count > 0ull) {
        basis = (double)decode->decode_state_values[index % decode->decode_state_value_count];
    }
    word = local & 0xffffull;
    signed_offset = ((double)word / 65535.0 - 0.5) * 0.125;
    value = basis + signed_offset - (double)index * 0.0001;
    return (float)value;
}

static void logits_accumulate_summary(yvex_logits_buffer_summary *out,
                                      const float *values,
                                      unsigned long long count)
{
    unsigned long long i;
    unsigned long long checksum = 1469598103934665603ull;

    if (!out || !values || count == 0ull) {
        return;
    }
    out->logits_min = (double)values[0];
    out->logits_max = (double)values[0];
    out->logits_sum = 0.0;
    for (i = 0; i < count; ++i) {
        double value = (double)values[i];
        checksum = logits_mix_float(checksum, values[i]);
        checksum = logits_mix_u64(checksum, i);
        if (value < out->logits_min) {
            out->logits_min = value;
        }
        if (value > out->logits_max) {
            out->logits_max = value;
        }
        out->logits_sum += value;
        if (i < YVEX_LOGITS_MAX_SAMPLE_VALUES) {
            out->logits_sample_values[i] = values[i];
            out->logits_sample_count = i + 1ull;
        }
    }
    out->logits_checksum = checksum;
}

int yvex_engine_create_logits_buffer(yvex_engine *engine,
                                     const yvex_logits_buffer_options *options,
                                     yvex_logits_buffer_summary *out,
                                     yvex_error *err)
{
    yvex_decode_step_summary decode_summary;
    float *values = NULL;
    unsigned long long i;
    unsigned long long count;
    unsigned long long seed;
    int rc;

    if (!engine || !options || !options->decode_options || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_engine_create_logits_buffer",
                       "engine, options, decode options, and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    yvex_logits_buffer_summary_init(out, options);
    count = options->logits_count;
    if (!yvex_logits_count_valid(count)) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_engine_create_logits_buffer",
                       "logits count must be between 1 and 256");
        return YVEX_ERR_INVALID_ARG;
    }

    memset(&decode_summary, 0, sizeof(decode_summary));
    out->decode_invoked = 1;
    out->logits_phase = "decode";
    rc = yvex_engine_decode_step(engine, options->decode_options, &decode_summary, err);
    logits_copy_decode_summary(out, &decode_summary);
    if (rc != YVEX_OK) {
        return rc;
    }
    if (!decode_summary.decode_state_created) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_engine_create_logits_buffer",
                       "decode state was not created");
        return YVEX_ERR_STATE;
    }

    if (logits_test_env_enabled("YVEX_TEST_FAIL_LOGITS_AFTER_DECODE")) {
        out->logits_phase = "after-decode";
        yvex_error_set(err, YVEX_ERR_BACKEND, "yvex_engine_create_logits_buffer",
                       "test logits failure after decode");
        return YVEX_ERR_BACKEND;
    }

    out->logits_phase = "allocation";
    if (logits_test_env_enabled("YVEX_TEST_FAIL_LOGITS_AFTER_ALLOC")) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_engine_create_logits_buffer",
                       "test logits allocation failure");
        return YVEX_ERR_NOMEM;
    }
    if (count > ULLONG_MAX / (unsigned long long)sizeof(*values)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_engine_create_logits_buffer",
                       "logits byte count overflow");
        return YVEX_ERR_BOUNDS;
    }
    values = (float *)calloc((size_t)count, sizeof(*values));
    if (!values) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_engine_create_logits_buffer",
                       "failed to allocate bounded logits buffer");
        return YVEX_ERR_NOMEM;
    }

    out->logits_phase = "fill";
    seed = decode_summary.decode_state_checksum;
    for (i = 0; i < count; ++i) {
        values[i] = logits_value_from_decode(&decode_summary, i, seed);
    }
    if (logits_test_env_enabled("YVEX_TEST_FAIL_LOGITS_AFTER_FILL")) {
        free(values);
        out->cleanup_attempted = 1;
        out->cleanup_status = "pass";
        yvex_error_set(err, YVEX_ERR_BACKEND, "yvex_engine_create_logits_buffer",
                       "test logits failure after fill");
        return YVEX_ERR_BACKEND;
    }

    out->logits_count = count;
    out->logits_bytes = count * (unsigned long long)sizeof(*values);
    logits_accumulate_summary(out, values, count);
    free(values);
    out->cleanup_attempted = 1;
    out->cleanup_status = "pass";
    out->logits_buffer_created = 1;
    out->bounded_logits_ready = 1;
    out->logits_phase = "complete";
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_logits_create(yvex_logits **out,
                       const yvex_model_descriptor *model,
                       yvex_error *err)
{
    yvex_logits *logits;

    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_logits_create", "out is required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;

    if (!model) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_logits_create", "model is required");
        return YVEX_ERR_INVALID_ARG;
    }

    logits = (yvex_logits *)calloc(1, sizeof(*logits));
    if (!logits) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_logits_create", "failed to allocate logits summary");
        return YVEX_ERR_NOMEM;
    }

    logits->summary.status = YVEX_LOGITS_STATUS_UNAVAILABLE;
    logits->summary.vocab_size = 0;
    logits->summary.bytes = 0;

    *out = logits;
    yvex_error_clear(err);
    return YVEX_OK;
}

void yvex_logits_close(yvex_logits *logits)
{
    free(logits);
}

yvex_logits_status yvex_logits_status_of(const yvex_logits *logits)
{
    return logits ? logits->summary.status : YVEX_LOGITS_STATUS_EMPTY;
}

const char *yvex_logits_status_name(yvex_logits_status status)
{
    switch (status) {
    case YVEX_LOGITS_STATUS_EMPTY: return "empty";
    case YVEX_LOGITS_STATUS_UNAVAILABLE: return "unavailable";
    case YVEX_LOGITS_STATUS_ALLOCATED: return "allocated";
    }
    return "unknown";
}

int yvex_logits_get_summary(const yvex_logits *logits,
                            yvex_logits_summary *out,
                            yvex_error *err)
{
    if (!logits || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_logits_get_summary", "logits and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memcpy(out, &logits->summary, sizeof(*out));
    yvex_error_clear(err);
    return YVEX_OK;
}
