/* Owner: src/generation
 * Owns: minimal session KV shape, allocation, append/read, clear, lifecycle, capacity accounting, and pure
 *   demo-value helpers used by KV reports.
 * Does not own: command dispatch, input parsing, text rendering, stdout/stderr output, attention execution, decode,
 *   logits, sampling, generation, eval, benchmark, or release decisions.
 * Invariants: this file mutates only KV cache state owned by the caller and reports bounded facts through public KV
 *   structs.
 * Boundary: minimal session-owned KV is not real attention-backed KV, decode readiness, logits readiness,
 *   generation readiness, benchmark evidence, throughput, or release readiness.
 * Purpose: Own the bounded session KV allocation, append, read, clear, and lifecycle contract.
 * Inputs: A validated KV shape, caller-owned cache, token indices, and scalar demo values.
 * Effects: Mutates only cache-owned storage and explicit token-count state.
 * Failure: Invalid indices, overflow, and allocation failures leave cache ownership coherent. */

#include <yvex/internal/generation.h>
#include <yvex/internal/core.h>
#include <yvex/runtime.h>

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct yvex_kv_cache {
    yvex_kv_summary summary;
    float *values;
};

static const yvex_kv_summary kv_unavailable_summary = {
    .status = YVEX_KV_STATUS_UNAVAILABLE,
    .owner = "session",
    .dtype = "none",
    .overflow_status = "not-checked",
    .cleanup_status = "not-needed",
    .session_owned = 1,
};

static const yvex_kv_summary kv_allocated_summary = {
    .status = YVEX_KV_STATUS_ALLOCATED,
    .owner = "session",
    .dtype = "F32",
    .overflow_status = "not-overflowed",
    .cleanup_status = "not-needed",
    .session_owned = 1,
};

static const char *const kv_status_names[] = {
    "empty", "unavailable", "planned", "allocated",
};

typedef struct {
    size_t offset;
    const char *message;
} kv_dimension;

static const kv_dimension kv_shape_dimensions[] = {
    {offsetof(yvex_kv_shape, layer_count), "layer_count must be positive"},
    {offsetof(yvex_kv_shape, kv_head_count), "kv_head_count must be positive"},
    {offsetof(yvex_kv_shape, head_dim), "head_dim must be positive"},
    {offsetof(yvex_kv_shape, capacity), "capacity must be positive"},
};

/* Purpose: Construct the admitted kv cache create state only after its identities and resources are valid.
 * Inputs: A validated configuration, checked resource limits, and caller-owned result storage.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
int yvex_kv_cache_create(yvex_kv_cache **out,
                         const yvex_model_descriptor *model,
                         unsigned long long context_length,
                         yvex_error *err)
{
    yvex_kv_cache *kv;

    if (!out) {
        return yvex_generation_refuse(err, YVEX_ERR_INVALID_ARG,
                                      "yvex_kv_cache_create", "out is required");
    }
    *out = NULL;

    if (!model) {
        return yvex_generation_refuse(err, YVEX_ERR_INVALID_ARG,
                                      "yvex_kv_cache_create", "model is required");
    }
    if (context_length == 0) {
        return yvex_generation_refuse(err, YVEX_ERR_INVALID_ARG,
                                      "yvex_kv_cache_create",
                                      "context_length must be positive");
    }

    kv = (yvex_kv_cache *)calloc(1, sizeof(*kv));
    if (!kv) {
        return yvex_generation_refuse(err, YVEX_ERR_NOMEM,
                                      "yvex_kv_cache_create",
                                      "failed to allocate KV cache summary");
    }

    (void)model;
    kv->summary = kv_unavailable_summary;
    kv->summary.context_length = context_length;

    *out = kv;
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: Implement the canonical kv shape values per position mechanism owned by the generation boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
static int kv_shape_values_per_position(const yvex_kv_shape *shape,
                                        unsigned long long *out,
                                        yvex_error *err)
{
    unsigned long long values = 0ull;
    unsigned long long tmp = 0ull;
    size_t index;

    if (!shape || !out) {
        return yvex_generation_refuse(err, YVEX_ERR_INVALID_ARG,
                                      "yvex_kv_cache_create_shape",
                                      "shape and out are required");
    }
    for (index = 0; index < sizeof(kv_shape_dimensions) / sizeof(kv_shape_dimensions[0]);
         ++index) {
        const unsigned long long *value =
            (const unsigned long long *)((const unsigned char *)shape +
                                        kv_shape_dimensions[index].offset);

        if (*value == 0ull) {
            return yvex_generation_refuse(err, YVEX_ERR_INVALID_ARG,
                                          "yvex_kv_cache_create_shape",
                                          kv_shape_dimensions[index].message);
        }
    }

    if (!yvex_core_u64_mul(shape->layer_count, shape->kv_head_count, &tmp) ||
        !yvex_core_u64_mul(tmp, shape->head_dim, &tmp) ||
        !yvex_core_u64_mul(tmp, 2ull, &values)) {
        return yvex_generation_refuse(err, YVEX_ERR_BOUNDS,
                                      "yvex_kv_cache_create_shape",
                                      "KV shape value count overflows");
    }
    *out = values;
    return YVEX_OK;
}

/* Purpose: Construct the admitted kv cache create shape state only after its identities and resources are valid.
 * Inputs: A validated configuration, checked resource limits, and caller-owned result storage.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
int yvex_kv_cache_create_shape(yvex_kv_cache **out,
                               const yvex_kv_shape *shape,
                               yvex_error *err)
{
    yvex_kv_cache *kv = NULL;
    unsigned long long values_per_position = 0ull;
    unsigned long long bytes_per_position = 0ull;
    unsigned long long planned_bytes = 0ull;
    int rc;

    if (!out) {
        return yvex_generation_refuse(err, YVEX_ERR_INVALID_ARG,
                                      "yvex_kv_cache_create_shape", "out is required");
    }
    *out = NULL;

    rc = kv_shape_values_per_position(shape, &values_per_position, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    if (!yvex_core_u64_mul(values_per_position,
                                 (unsigned long long)sizeof(float),
                                 &bytes_per_position) ||
        !yvex_core_u64_mul(bytes_per_position, shape->capacity, &planned_bytes)) {
        return yvex_generation_refuse(err, YVEX_ERR_BOUNDS,
                                      "yvex_kv_cache_create_shape",
                                      "KV byte count overflows");
    }
    if (planned_bytes > (unsigned long long)SIZE_MAX) {
        return yvex_generation_refuse(err, YVEX_ERR_BOUNDS,
                                      "yvex_kv_cache_create_shape",
                                      "KV byte count exceeds host allocation size");
    }

    kv = (yvex_kv_cache *)calloc(1, sizeof(*kv));
    if (!kv) {
        return yvex_generation_refuse(err, YVEX_ERR_NOMEM,
                                      "yvex_kv_cache_create_shape",
                                      "failed to allocate KV cache");
    }
    kv->values = (float *)calloc((size_t)(planned_bytes / sizeof(float)), sizeof(float));
    if (!kv->values) {
        free(kv);
        return yvex_generation_refuse(err, YVEX_ERR_NOMEM,
                                      "yvex_kv_cache_create_shape",
                                      "failed to allocate KV storage");
    }

    kv->summary = kv_allocated_summary;
    kv->summary.context_length = shape->capacity;
    kv->summary.layer_count = shape->layer_count;
    kv->summary.kv_head_count = shape->kv_head_count;
    kv->summary.head_dim = shape->head_dim;
    kv->summary.values_per_position = values_per_position;
    kv->summary.bytes_per_position = bytes_per_position;
    kv->summary.bytes = planned_bytes;
    kv->summary.allocated_bytes = planned_bytes;

    *out = kv;
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: Release the resources owned by kv cache close without changing borrowed inputs.
 * Inputs: An owned object that may be null or already released where its lifecycle permits.
 * Effects: Releases only resources owned by the supplied object and leaves it reset or unusable.
 * Failure: Null and already-released inputs follow the idempotent lifecycle contract.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
void yvex_kv_cache_close(yvex_kv_cache *kv)
{
    if (!kv) {
        return;
    }
    free(kv->values);
    free(kv);
}

/* Purpose: Implement the canonical kv status of mechanism owned by the generation boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Does not mutate caller-visible or owner state.
 * Failure: Returns the canonical unknown or zero sentinel for an invalid typed value.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
yvex_kv_status yvex_kv_status_of(const yvex_kv_cache *kv)
{
    return kv ? kv->summary.status : YVEX_KV_STATUS_EMPTY;
}

/* Purpose: Return the canonical diagnostic label for kv status name.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Does not mutate caller-visible or owner state.
 * Failure: Returns the canonical unknown or zero sentinel for an invalid typed value.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
const char *yvex_kv_status_name(yvex_kv_status status)
{
    return status >= YVEX_KV_STATUS_EMPTY &&
                   (size_t)status < sizeof(kv_status_names) / sizeof(kv_status_names[0])
               ? kv_status_names[status]
               : "unknown";
}

/* Purpose: Implement the canonical kv cache position value count mechanism owned by the generation boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
unsigned long long yvex_kv_cache_position_value_count(const yvex_kv_cache *kv)
{
    return kv ? kv->summary.values_per_position : 0ull;
}

/* Purpose: Append kv cache append position F32 while preserving checked capacity and deterministic order.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
int yvex_kv_cache_append_position_f32(yvex_kv_cache *kv,
                                      const float *values,
                                      unsigned long long value_count,
                                      unsigned long long *out_position,
                                      yvex_error *err)
{
    unsigned long long offset = 0ull;

    if (out_position) {
        *out_position = 0ull;
    }
    if (!kv || !values) {
        return yvex_generation_refuse(err, YVEX_ERR_INVALID_ARG,
                                      "yvex_kv_cache_append_position_f32",
                                      "kv and values are required");
    }
    if (kv->summary.status != YVEX_KV_STATUS_ALLOCATED || !kv->values) {
        return yvex_generation_refuse(err, YVEX_ERR_STATE,
                                      "yvex_kv_cache_append_position_f32",
                                      "KV store is not allocated");
    }
    if (value_count != kv->summary.values_per_position) {
        yvex_error_setf(err, YVEX_ERR_INVALID_ARG, "yvex_kv_cache_append_position_f32",
                        "value_count must be %llu", kv->summary.values_per_position);
        return YVEX_ERR_INVALID_ARG;
    }
    if (kv->summary.written_positions >= kv->summary.context_length) {
        kv->summary.overflow_status = "capacity-exceeded";
        yvex_error_setf(err, YVEX_ERR_BOUNDS, "yvex_kv_cache_append_position_f32",
                        "append would exceed KV capacity %llu",
                        kv->summary.context_length);
        return YVEX_ERR_BOUNDS;
    }
    if (!yvex_core_u64_mul(kv->summary.written_positions,
                                 kv->summary.values_per_position,
                                 &offset)) {
        kv->summary.overflow_status = "offset-overflow";
        return yvex_generation_refuse(err, YVEX_ERR_BOUNDS,
                                      "yvex_kv_cache_append_position_f32",
                                      "KV append offset overflows");
    }

    memcpy(kv->values + offset, values, (size_t)value_count * sizeof(float));
    if (out_position) {
        *out_position = kv->summary.written_positions;
    }
    kv->summary.written_positions += 1ull;
    kv->summary.append_count += 1ull;
    kv->summary.overflow_status = "not-overflowed";
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: Retrieve kv cache read position F32 from admitted immutable or owned state.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
int yvex_kv_cache_read_position_f32(yvex_kv_cache *kv,
                                    unsigned long long position,
                                    float *out_values,
                                    unsigned long long value_count,
                                    yvex_error *err)
{
    unsigned long long offset = 0ull;

    if (!kv || !out_values) {
        return yvex_generation_refuse(err, YVEX_ERR_INVALID_ARG,
                                      "yvex_kv_cache_read_position_f32",
                                      "kv and out_values are required");
    }
    if (kv->summary.status != YVEX_KV_STATUS_ALLOCATED || !kv->values) {
        return yvex_generation_refuse(err, YVEX_ERR_STATE,
                                      "yvex_kv_cache_read_position_f32",
                                      "KV store is not allocated");
    }
    if (value_count != kv->summary.values_per_position) {
        yvex_error_setf(err, YVEX_ERR_INVALID_ARG, "yvex_kv_cache_read_position_f32",
                        "value_count must be %llu", kv->summary.values_per_position);
        return YVEX_ERR_INVALID_ARG;
    }
    if (position >= kv->summary.context_length) {
        yvex_error_setf(err, YVEX_ERR_BOUNDS, "yvex_kv_cache_read_position_f32",
                        "read position %llu exceeds KV capacity %llu",
                        position, kv->summary.context_length);
        return YVEX_ERR_BOUNDS;
    }
    if (position >= kv->summary.written_positions) {
        yvex_error_setf(err, YVEX_ERR_BOUNDS, "yvex_kv_cache_read_position_f32",
                        "read position %llu has not been written", position);
        return YVEX_ERR_BOUNDS;
    }
    if (!yvex_core_u64_mul(position, kv->summary.values_per_position, &offset)) {
        return yvex_generation_refuse(err, YVEX_ERR_BOUNDS,
                                      "yvex_kv_cache_read_position_f32",
                                      "KV read offset overflows");
    }

    memcpy(out_values, kv->values + offset, (size_t)value_count * sizeof(float));
    kv->summary.read_count += 1ull;
    kv->summary.last_read_position = position;
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: Implement the canonical kv cache clear mechanism owned by the generation boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
int yvex_kv_cache_clear(yvex_kv_cache *kv,
                        yvex_error *err)
{
    if (!kv) {
        return yvex_generation_refuse(err, YVEX_ERR_INVALID_ARG,
                                      "yvex_kv_cache_clear", "kv is required");
    }
    if (kv->values && kv->summary.allocated_bytes <= (unsigned long long)SIZE_MAX) {
        memset(kv->values, 0, (size_t)kv->summary.allocated_bytes);
    }
    kv->summary.written_positions = 0ull;
    kv->summary.append_count = 0ull;
    kv->summary.read_count = 0ull;
    kv->summary.last_read_position = 0ull;
    kv->summary.overflow_status = kv->summary.status == YVEX_KV_STATUS_ALLOCATED
                                      ? "not-overflowed"
                                      : "not-checked";
    kv->summary.cleanup_status = "pass";
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: Retrieve kv cache get summary from admitted immutable or owned state.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
int yvex_kv_cache_get_summary(const yvex_kv_cache *kv,
                              yvex_kv_summary *out,
                              yvex_error *err)
{
    if (!kv || !out) {
        return yvex_generation_refuse(err, YVEX_ERR_INVALID_ARG,
                                      "yvex_kv_cache_get_summary",
                                      "kv and out are required");
    }
    memcpy(out, &kv->summary, sizeof(*out));
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: populate caller-owned temporary F32 values for diagnostic append proofs.
 * Inputs: values points to caller-owned storage for value_count floats; position is the
 * synthetic position encoded into the values.
 * Effects: mutates only values; performs no allocation, cleanup, IO, or rendering.
 * Failure: null values is a no-op.
 * Boundary: synthetic values are diagnostic KV data only and are not model attention activations. */
void yvex_kv_fill_demo_values(float *values,
                         unsigned long long value_count,
                         unsigned long long position)
{
    unsigned long long i;

    for (i = 0; values && i < value_count; ++i) {
        values[i] = (float)((position * 1000ull) + i);
    }
}

/* Purpose: compute a stable diagnostic checksum over caller-owned KV readback values.
 * Inputs: values is borrowed read-only storage with value_count floats.
 * Effects: performs no mutation, allocation, cleanup, IO, or rendering.
 * Failure: null values yields the seed checksum.
 * Boundary: checksum evidence proves only diagnostic buffer round-trip behavior. */
unsigned long long yvex_kv_checksum_values(const float *values,
                                      unsigned long long value_count)
{
    unsigned long long checksum = 1469598103934665603ull;
    unsigned long long i;

    for (i = 0; values && i < value_count; ++i) {
        unsigned long long v = (unsigned long long)values[i];
        checksum ^= v + (i << 8u);
        checksum *= 1099511628211ull;
    }
    return checksum;
}
