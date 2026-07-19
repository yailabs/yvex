/* Owner: generation.logits (generation).
 * Owns: the typed diagnostic state transition consumed by runtime and reports.
 * Does not own: model policy, graph admission, generation readiness, or higher-stage claims.
 * Invariants: diagnostic state never promotes model-backed generation capability.
 * Boundary: this owner exposes typed facts only at its admitted subsystem stage.
 * Purpose: Own the bounded diagnostic logits buffer and its decode-state projection.
 * Inputs: A vocabulary bound, diagnostic decode state, and caller-owned logits object.
 * Effects: Allocates or updates only logits-owned storage.
 * Failure: Invalid dimensions and allocation failures publish no usable logits state. */

#include <yvex/internal/core.h>
#include <yvex/internal/generation.h>
#include <yvex/generation.h>

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct yvex_logits {
    yvex_logits_summary summary;
};

/* Purpose: Enforce the typed ownership, geometry, and lifecycle invariants for logits count valid.
 * Inputs: Immutable typed facts whose ownership, shape, or lifecycle state must be admitted.
 * Effects: Does not mutate caller-visible or owner state.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
int yvex_logits_count_valid(unsigned long long count)
{
    return count >= 1ull && count <= 256ull;
}

static const yvex_logits_buffer_summary logits_summary_default = {
    .logits_buffer_kind = "bounded-diagnostic",
    .logits_phase = "preflight",
    .logits_source = "decode-state",
    .backend_name = "cpu",
    .decode_step_kind = "none",
    .decode_phase = "not-started",
    .cleanup_status = "not-needed",
    .generation_status = "unsupported"
};

static const char *const logits_status_names[] = {
    "empty", "unavailable", "allocated",
};

#define LOGITS_FIELD(destination_, source_)                                             \
    {                                                                                   \
        offsetof(yvex_logits_buffer_summary, destination_),                             \
        offsetof(yvex_decode_step_summary, source_),                                    \
        sizeof(((yvex_decode_step_summary *)0)->source_) +                              \
            0u * sizeof(char[sizeof(((yvex_logits_buffer_summary *)0)->destination_) ==  \
                                    sizeof(((yvex_decode_step_summary *)0)->source_)     \
                                ? 1                                                     \
                                : -1])                                                  \
    }

static const yvex_generation_field_projection logits_decode_fields[] = {
    LOGITS_FIELD(decode_state_created, decode_state_created),
    LOGITS_FIELD(decode_step_executed, decode_step_executed),
    LOGITS_FIELD(decode_position, decode_position),
    LOGITS_FIELD(decode_state_checksum, decode_state_checksum),
};

#undef LOGITS_FIELD

/* Purpose: Construct the admitted logits buffer summary init state only after its identities and resources are valid.
 * Inputs: A validated configuration, checked resource limits, and caller-owned result storage.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
void yvex_logits_buffer_summary_init(yvex_logits_buffer_summary *out,
                                     const yvex_logits_buffer_options *options)
{
    const yvex_decode_step_options *decode_options;

    if (!out) {
        return;
    }
    *out = logits_summary_default;
    decode_options = options ? options->decode_options : NULL;
    out->backend_name = decode_options && decode_options->backend_name
                            ? decode_options->backend_name
                            : "cpu";
}

/* Purpose: Copy logits copy decode summary between compatible admitted ranges without changing semantic identity.
 * Inputs: Typed admitted handles, immutable source ranges, checked dimensions, and an explicit destination.
 * Effects: Mutates only the admitted destination or transaction after every precondition passes.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
static void logits_copy_decode_summary(yvex_logits_buffer_summary *out,
                                       const yvex_decode_step_summary *decode)
{
    if (!out || !decode) {
        return;
    }
    yvex_generation_project_fields(
        out, decode, logits_decode_fields,
        sizeof(logits_decode_fields) / sizeof(logits_decode_fields[0]));
    out->backend_name = decode->backend_name ? decode->backend_name : out->backend_name;
    out->decode_step_kind = decode->decode_step_kind ? decode->decode_step_kind
                                                     : "bounded-diagnostic";
    out->decode_phase = decode->decode_phase ? decode->decode_phase : "unknown";
}

/* Purpose: Decode logits value from decode according to its pinned numeric representation.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Mutates only the admitted destination or transaction after every precondition passes.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
static float logits_value_from_decode(const yvex_decode_step_summary *decode,
                                      unsigned long long index,
                                      unsigned long long seed)
{
    unsigned long long local;
    unsigned long long word;
    double basis = 0.0;
    double signed_offset;
    double value;

    local = yvex_core_hash_mix_u64(seed, index);
    local = yvex_core_hash_mix_u64(local, decode->decode_position);
    if (decode->decode_state_value_count > 0ull) {
        basis = (double)decode->decode_state_values[index % decode->decode_state_value_count];
    }
    word = local & 0xffffull;
    signed_offset = ((double)word / 65535.0 - 0.5) * 0.125;
    value = basis + signed_offset - (double)index * 0.0001;
    return (float)value;
}

/* Purpose: Implement the canonical logits accumulate summary mechanism owned by the generation boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
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
        checksum = yvex_generation_hash_float(checksum, values[i]);
        checksum = yvex_core_hash_mix_u64(checksum, i);
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

/* Purpose: Construct the admitted engine create logits buffer state only after its identities and resources are valid.
 * Inputs: A validated configuration, checked resource limits, and caller-owned result storage.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
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
        return yvex_generation_refuse(
            err, YVEX_ERR_INVALID_ARG, "yvex_engine_create_logits_buffer",
            "engine, options, decode options, and out are required");
    }
    yvex_logits_buffer_summary_init(out, options);
    count = options->logits_count;
    if (!yvex_logits_count_valid(count)) {
        return yvex_generation_refuse(err, YVEX_ERR_INVALID_ARG,
                                      "yvex_engine_create_logits_buffer",
                                      "logits count must be between 1 and 256");
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
        return yvex_generation_refuse(err, YVEX_ERR_STATE,
                                      "yvex_engine_create_logits_buffer",
                                      "decode state was not created");
    }

    rc = yvex_generation_test_refuse(
        "YVEX_TEST_FAIL_LOGITS_AFTER_DECODE", err, YVEX_ERR_BACKEND,
        "yvex_engine_create_logits_buffer", "test logits failure after decode");
    if (rc != YVEX_OK) {
        out->logits_phase = "after-decode";
        return rc;
    }

    out->logits_phase = "allocation";
    rc = yvex_generation_test_refuse(
        "YVEX_TEST_FAIL_LOGITS_AFTER_ALLOC", err, YVEX_ERR_NOMEM,
        "yvex_engine_create_logits_buffer", "test logits allocation failure");
    if (rc != YVEX_OK) {
        return rc;
    }
    if (count > ULLONG_MAX / (unsigned long long)sizeof(*values)) {
        return yvex_generation_refuse(err, YVEX_ERR_BOUNDS,
                                      "yvex_engine_create_logits_buffer",
                                      "logits byte count overflow");
    }
    values = (float *)calloc((size_t)count, sizeof(*values));
    if (!values) {
        return yvex_generation_refuse(err, YVEX_ERR_NOMEM,
                                      "yvex_engine_create_logits_buffer",
                                      "failed to allocate bounded logits buffer");
    }

    out->logits_phase = "fill";
    seed = decode_summary.decode_state_checksum;
    for (i = 0; i < count; ++i) {
        values[i] = logits_value_from_decode(&decode_summary, i, seed);
    }
    rc = yvex_generation_test_refuse(
        "YVEX_TEST_FAIL_LOGITS_AFTER_FILL", err, YVEX_ERR_BACKEND,
        "yvex_engine_create_logits_buffer", "test logits failure after fill");
    if (rc != YVEX_OK) {
        free(values);
        out->cleanup_attempted = 1;
        out->cleanup_status = "pass";
        return rc;
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

/* Purpose: Construct the admitted logits create state only after its identities and resources are valid.
 * Inputs: A validated configuration, checked resource limits, and caller-owned result storage.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
int yvex_logits_create(yvex_logits **out,
                       const yvex_model_descriptor *model,
                       yvex_error *err)
{
    yvex_logits *logits;

    if (!out) {
        return yvex_generation_refuse(err, YVEX_ERR_INVALID_ARG,
                                      "yvex_logits_create", "out is required");
    }
    *out = NULL;

    if (!model) {
        return yvex_generation_refuse(err, YVEX_ERR_INVALID_ARG,
                                      "yvex_logits_create", "model is required");
    }

    logits = (yvex_logits *)calloc(1, sizeof(*logits));
    if (!logits) {
        return yvex_generation_refuse(err, YVEX_ERR_NOMEM,
                                      "yvex_logits_create",
                                      "failed to allocate logits summary");
    }

    logits->summary.status = YVEX_LOGITS_STATUS_UNAVAILABLE;
    logits->summary.vocab_size = 0;
    logits->summary.bytes = 0;

    *out = logits;
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: Release the resources owned by logits close without changing borrowed inputs.
 * Inputs: An owned object that may be null or already released where its lifecycle permits.
 * Effects: Releases only resources owned by the supplied object and leaves it reset or unusable.
 * Failure: Null and already-released inputs follow the idempotent lifecycle contract.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
void yvex_logits_close(yvex_logits *logits)
{
    free(logits);
}

/* Purpose: Implement the canonical logits status of mechanism owned by the generation boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Does not mutate caller-visible or owner state.
 * Failure: Returns the canonical unknown or zero sentinel for an invalid typed value.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
yvex_logits_status yvex_logits_status_of(const yvex_logits *logits)
{
    return logits ? logits->summary.status : YVEX_LOGITS_STATUS_EMPTY;
}

/* Purpose: Return the canonical diagnostic label for logits status name.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Does not mutate caller-visible or owner state.
 * Failure: Returns the canonical unknown or zero sentinel for an invalid typed value.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
const char *yvex_logits_status_name(yvex_logits_status status)
{
    return status >= YVEX_LOGITS_STATUS_EMPTY &&
                   (size_t)status < sizeof(logits_status_names) /
                                        sizeof(logits_status_names[0])
               ? logits_status_names[status]
               : "unknown";
}

/* Purpose: Retrieve logits get summary from admitted immutable or owned state.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
int yvex_logits_get_summary(const yvex_logits *logits,
                            yvex_logits_summary *out,
                            yvex_error *err)
{
    if (!logits || !out) {
        return yvex_generation_refuse(err, YVEX_ERR_INVALID_ARG,
                                      "yvex_logits_get_summary",
                                      "logits and out are required");
    }
    memcpy(out, &logits->summary, sizeof(*out));
    yvex_error_clear(err);
    return YVEX_OK;
}
