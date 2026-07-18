/*
 * kv.c - KV ownership boundary.
 *
 * Owner:
 *   src/generation
 *
 * Owns:
 *   minimal session KV shape, allocation, append/read, clear, lifecycle,
 *   capacity accounting, and pure demo-value helpers used by KV reports.
 *
 * Does not own:
 *   command dispatch, input parsing, text rendering, stdout/stderr output,
 *   attention execution, decode, logits, sampling, generation, eval,
 *   benchmark, or release decisions.
 *
 * Invariants:
 *   this file mutates only KV cache state owned by the caller and reports
 *   bounded facts through public KV structs.
 *
 * Boundary:
 *   minimal session-owned KV is not real attention-backed KV, decode
 *   readiness, logits readiness, generation readiness, benchmark evidence,
 *   throughput, or release readiness.
 */

#include <yvex/kv.h>

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct yvex_kv_cache {
    yvex_kv_summary summary;
    float *values;
};

static int kv_checked_mul_ull(unsigned long long a,
                              unsigned long long b,
                              unsigned long long *out)
{
    if (!out) {
        return 0;
    }
    if (a != 0ull && b > ULLONG_MAX / a) {
        return 0;
    }
    *out = a * b;
    return 1;
}

int yvex_kv_cache_create(yvex_kv_cache **out,
                         const yvex_model_descriptor *model,
                         unsigned long long context_length,
                         yvex_error *err)
{
    yvex_kv_cache *kv;

    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_kv_cache_create", "out is required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;

    if (!model) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_kv_cache_create", "model is required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (context_length == 0) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_kv_cache_create", "context_length must be positive");
        return YVEX_ERR_INVALID_ARG;
    }

    kv = (yvex_kv_cache *)calloc(1, sizeof(*kv));
    if (!kv) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_kv_cache_create", "failed to allocate KV cache summary");
        return YVEX_ERR_NOMEM;
    }

    (void)model;
    kv->summary.status = YVEX_KV_STATUS_UNAVAILABLE;
    kv->summary.owner = "session";
    kv->summary.dtype = "none";
    kv->summary.context_length = context_length;
    kv->summary.bytes = 0;
    kv->summary.overflow_status = "not-checked";
    kv->summary.cleanup_status = "not-needed";
    kv->summary.session_owned = 1;
    kv->summary.decode_ready = 0;
    kv->summary.logits_ready = 0;
    kv->summary.generation_ready = 0;

    *out = kv;
    yvex_error_clear(err);
    return YVEX_OK;
}

static int kv_shape_values_per_position(const yvex_kv_shape *shape,
                                        unsigned long long *out,
                                        yvex_error *err)
{
    unsigned long long values = 0ull;
    unsigned long long tmp = 0ull;

    if (!shape || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_kv_cache_create_shape",
                       "shape and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (shape->layer_count == 0ull) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_kv_cache_create_shape",
                       "layer_count must be positive");
        return YVEX_ERR_INVALID_ARG;
    }
    if (shape->kv_head_count == 0ull) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_kv_cache_create_shape",
                       "kv_head_count must be positive");
        return YVEX_ERR_INVALID_ARG;
    }
    if (shape->head_dim == 0ull) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_kv_cache_create_shape",
                       "head_dim must be positive");
        return YVEX_ERR_INVALID_ARG;
    }
    if (shape->capacity == 0ull) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_kv_cache_create_shape",
                       "capacity must be positive");
        return YVEX_ERR_INVALID_ARG;
    }

    if (!kv_checked_mul_ull(shape->layer_count, shape->kv_head_count, &tmp) ||
        !kv_checked_mul_ull(tmp, shape->head_dim, &tmp) ||
        !kv_checked_mul_ull(tmp, 2ull, &values)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_kv_cache_create_shape",
                       "KV shape value count overflows");
        return YVEX_ERR_BOUNDS;
    }
    *out = values;
    return YVEX_OK;
}

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
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_kv_cache_create_shape",
                       "out is required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;

    rc = kv_shape_values_per_position(shape, &values_per_position, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    if (!kv_checked_mul_ull(values_per_position,
                                 (unsigned long long)sizeof(float),
                                 &bytes_per_position) ||
        !kv_checked_mul_ull(bytes_per_position, shape->capacity, &planned_bytes)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_kv_cache_create_shape",
                       "KV byte count overflows");
        return YVEX_ERR_BOUNDS;
    }
    if (planned_bytes > (unsigned long long)SIZE_MAX) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_kv_cache_create_shape",
                       "KV byte count exceeds host allocation size");
        return YVEX_ERR_BOUNDS;
    }

    kv = (yvex_kv_cache *)calloc(1, sizeof(*kv));
    if (!kv) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_kv_cache_create_shape",
                       "failed to allocate KV cache");
        return YVEX_ERR_NOMEM;
    }
    kv->values = (float *)calloc((size_t)(planned_bytes / sizeof(float)), sizeof(float));
    if (!kv->values) {
        free(kv);
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_kv_cache_create_shape",
                       "failed to allocate KV storage");
        return YVEX_ERR_NOMEM;
    }

    kv->summary.status = YVEX_KV_STATUS_ALLOCATED;
    kv->summary.owner = "session";
    kv->summary.dtype = "F32";
    kv->summary.context_length = shape->capacity;
    kv->summary.layer_count = shape->layer_count;
    kv->summary.kv_head_count = shape->kv_head_count;
    kv->summary.head_dim = shape->head_dim;
    kv->summary.values_per_position = values_per_position;
    kv->summary.bytes_per_position = bytes_per_position;
    kv->summary.bytes = planned_bytes;
    kv->summary.allocated_bytes = planned_bytes;
    kv->summary.overflow_status = "not-overflowed";
    kv->summary.cleanup_status = "not-needed";
    kv->summary.session_owned = 1;
    kv->summary.decode_ready = 0;
    kv->summary.logits_ready = 0;
    kv->summary.generation_ready = 0;

    *out = kv;
    yvex_error_clear(err);
    return YVEX_OK;
}

void yvex_kv_cache_close(yvex_kv_cache *kv)
{
    if (!kv) {
        return;
    }
    free(kv->values);
    free(kv);
}

yvex_kv_status yvex_kv_status_of(const yvex_kv_cache *kv)
{
    return kv ? kv->summary.status : YVEX_KV_STATUS_EMPTY;
}

const char *yvex_kv_status_name(yvex_kv_status status)
{
    switch (status) {
    case YVEX_KV_STATUS_EMPTY: return "empty";
    case YVEX_KV_STATUS_UNAVAILABLE: return "unavailable";
    case YVEX_KV_STATUS_PLANNED: return "planned";
    case YVEX_KV_STATUS_ALLOCATED: return "allocated";
    }
    return "unknown";
}

unsigned long long yvex_kv_cache_position_value_count(const yvex_kv_cache *kv)
{
    return kv ? kv->summary.values_per_position : 0ull;
}

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
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_kv_cache_append_position_f32",
                       "kv and values are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (kv->summary.status != YVEX_KV_STATUS_ALLOCATED || !kv->values) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_kv_cache_append_position_f32",
                       "KV store is not allocated");
        return YVEX_ERR_STATE;
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
    if (!kv_checked_mul_ull(kv->summary.written_positions,
                                 kv->summary.values_per_position,
                                 &offset)) {
        kv->summary.overflow_status = "offset-overflow";
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_kv_cache_append_position_f32",
                       "KV append offset overflows");
        return YVEX_ERR_BOUNDS;
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

int yvex_kv_cache_read_position_f32(yvex_kv_cache *kv,
                                    unsigned long long position,
                                    float *out_values,
                                    unsigned long long value_count,
                                    yvex_error *err)
{
    unsigned long long offset = 0ull;

    if (!kv || !out_values) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_kv_cache_read_position_f32",
                       "kv and out_values are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (kv->summary.status != YVEX_KV_STATUS_ALLOCATED || !kv->values) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_kv_cache_read_position_f32",
                       "KV store is not allocated");
        return YVEX_ERR_STATE;
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
    if (!kv_checked_mul_ull(position, kv->summary.values_per_position, &offset)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_kv_cache_read_position_f32",
                       "KV read offset overflows");
        return YVEX_ERR_BOUNDS;
    }

    memcpy(out_values, kv->values + offset, (size_t)value_count * sizeof(float));
    kv->summary.read_count += 1ull;
    kv->summary.last_read_position = position;
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_kv_cache_clear(yvex_kv_cache *kv,
                        yvex_error *err)
{
    if (!kv) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_kv_cache_clear", "kv is required");
        return YVEX_ERR_INVALID_ARG;
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

int yvex_kv_cache_get_summary(const yvex_kv_cache *kv,
                              yvex_kv_summary *out,
                              yvex_error *err)
{
    if (!kv || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_kv_cache_get_summary", "kv and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memcpy(out, &kv->summary, sizeof(*out));
    yvex_error_clear(err);
    return YVEX_OK;
}

/*
 * yvex_kv_fill_demo_values()
 *
 * Purpose:
 *   populate caller-owned temporary F32 values for diagnostic append proofs.
 *
 * Inputs:
 *   values points to caller-owned storage for value_count floats; position is
 *   the synthetic position encoded into the values.
 *
 * Effects:
 *   mutates only values; performs no allocation, cleanup, IO, or rendering.
 *
 * Failure:
 *   null values is a no-op.
 *
 * Boundary:
 *   synthetic values are diagnostic KV data only and are not model attention
 *   activations.
 */
void yvex_kv_fill_demo_values(float *values,
                         unsigned long long value_count,
                         unsigned long long position)
{
    unsigned long long i;

    for (i = 0; values && i < value_count; ++i) {
        values[i] = (float)((position * 1000ull) + i);
    }
}

/*
 * yvex_kv_checksum_values()
 *
 * Purpose:
 *   compute a stable diagnostic checksum over caller-owned KV readback values.
 *
 * Inputs:
 *   values is borrowed read-only storage with value_count floats.
 *
 * Effects:
 *   performs no mutation, allocation, cleanup, IO, or rendering.
 *
 * Failure:
 *   null values yields the seed checksum.
 *
 * Boundary:
 *   checksum evidence proves only diagnostic buffer round-trip behavior.
 */
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
