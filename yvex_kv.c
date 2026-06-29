/*
 * yvex_kv.c - KV ownership boundary.
 *
 * This file owns minimal session KV shape, allocation, append/read, lifecycle,
 * and capacity diagnostics. It does not run attention, decode, logits,
 * sampling, generation, or advanced paged/spill KV.
 */

#include <yvex/kv.h>
#include "yvex_console_private.h"

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

/* Domain-owned command surface moved out of yvex_runtime.c. */

static void fill_kv_demo_values(float *values,
                                unsigned long long value_count,
                                unsigned long long position)
{
    unsigned long long i;

    for (i = 0; values && i < value_count; ++i) {
        values[i] = (float)((position * 1000ull) + i);
    }
}

static unsigned long long checksum_kv_values(const float *values,
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

static int command_kv(int argc, char **argv)
{
    yvex_kv_shape shape;
    yvex_kv_cache *kv = NULL;
    yvex_kv_summary summary;
    yvex_error err;
    float *append_values = NULL;
    float *read_values = NULL;
    unsigned long long value_count = 0ull;
    unsigned long long append_target = 0ull;
    unsigned long long appended_position = 0ull;
    unsigned long long read_position = 0ull;
    unsigned long long read_checksum = 0ull;
    int append_demo = 0;
    int read_requested = 0;
    int cleanup_attempted = 0;
    const char *cleanup_status = "not-needed";
    int i;
    int rc;

    memset(&shape, 0, sizeof(shape));
    memset(&summary, 0, sizeof(summary));
    yvex_error_clear(&err);

    if (argc < 3 || strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0) {
        yvex_kv_help(stdout);
        return argc >= 3 ? 0 : 2;
    }

    for (i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--layers") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &shape.layer_count)) {
                fprintf(stderr, "yvex: --layers requires a positive integer\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(argv[i], "--heads") == 0 || strcmp(argv[i], "--lanes") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &shape.kv_head_count)) {
                fprintf(stderr, "yvex: --heads requires a positive integer\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(argv[i], "--head-dim") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &shape.head_dim)) {
                fprintf(stderr, "yvex: --head-dim requires a positive integer\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(argv[i], "--capacity") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &shape.capacity)) {
                fprintf(stderr, "yvex: --capacity requires a positive integer\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(argv[i], "--append-demo") == 0) {
            append_demo = 1;
        } else if (strcmp(argv[i], "--read-position") == 0) {
            if (i + 1 >= argc || !parse_ull_allow_zero(argv[i + 1], &read_position)) {
                fprintf(stderr, "yvex: --read-position requires a non-negative integer\n");
                return 2;
            }
            read_requested = 1;
            i += 1;
        } else {
            fprintf(stderr, "yvex: unknown kv option: %s\n", argv[i]);
            fprintf(stderr, "Try 'yvex help kv' for usage.\n");
            return 2;
        }
    }

    if (shape.layer_count == 0ull || shape.kv_head_count == 0ull ||
        shape.head_dim == 0ull || shape.capacity == 0ull) {
        fprintf(stderr, "usage: yvex kv --layers N --heads N --head-dim N --capacity N [--append-demo] [--read-position N]\n");
        return 2;
    }

    rc = yvex_kv_cache_create_shape(&kv, &shape, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }

    value_count = yvex_kv_cache_position_value_count(kv);
    if (append_demo) {
        append_values = (float *)calloc((size_t)value_count, sizeof(float));
        if (!append_values) {
            yvex_kv_cache_close(kv);
            fprintf(stderr, "yvex: failed to allocate KV append demo buffer\n");
            return 4;
        }
        append_target = shape.capacity > 1ull ? 2ull : 1ull;
        for (i = 0; (unsigned long long)i < append_target; ++i) {
            fill_kv_demo_values(append_values, value_count, (unsigned long long)i);
            rc = yvex_kv_cache_append_position_f32(kv,
                                                   append_values,
                                                   value_count,
                                                   &appended_position,
                                                   &err);
            if (rc != YVEX_OK) {
                free(append_values);
                yvex_kv_cache_close(kv);
                return print_yvex_error(&err, exit_for_status(rc));
            }
        }
    }

    if (read_requested) {
        read_values = (float *)calloc((size_t)value_count, sizeof(float));
        if (!read_values) {
            free(append_values);
            yvex_kv_cache_close(kv);
            fprintf(stderr, "yvex: failed to allocate KV read buffer\n");
            return 4;
        }
        rc = yvex_kv_cache_read_position_f32(kv, read_position, read_values, value_count, &err);
        if (rc != YVEX_OK) {
            free(read_values);
            free(append_values);
            yvex_kv_cache_close(kv);
            return print_yvex_error(&err, exit_for_status(rc));
        }
        read_checksum = checksum_kv_values(read_values, value_count);
    }

    rc = yvex_kv_cache_get_summary(kv, &summary, &err);
    if (rc != YVEX_OK) {
        free(read_values);
        free(append_values);
        yvex_kv_cache_close(kv);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    cleanup_attempted = 1;
    yvex_kv_cache_close(kv);
    kv = NULL;
    cleanup_status = "pass";

    printf("kv: ownership\n");
    printf("kv_created: true\n");
    printf("session_owned: %s\n", summary.session_owned ? "true" : "false");
    printf("layers: %llu\n", summary.layer_count);
    printf("heads: %llu\n", summary.kv_head_count);
    printf("head_dim: %llu\n", summary.head_dim);
    printf("capacity: %llu\n", summary.context_length);
    printf("dtype: %s\n", summary.dtype ? summary.dtype : "F32");
    printf("values_per_position: %llu\n", summary.values_per_position);
    printf("bytes_per_position: %llu\n", summary.bytes_per_position);
    printf("planned_bytes: %llu\n", summary.bytes);
    printf("allocated_bytes: %llu\n", summary.allocated_bytes);
    printf("append_count: %llu\n", summary.append_count);
    printf("read_count: %llu\n", summary.read_count);
    printf("written_positions: %llu\n", summary.written_positions);
    printf("last_appended_position: %llu\n", appended_position);
    if (read_requested) {
        unsigned long long sample_count = value_count < 8ull ? value_count : 8ull;
        unsigned long long j;
        printf("read_position: %llu\n", read_position);
        printf("read_value_count: %llu\n", value_count);
        printf("read_checksum: %llu\n", read_checksum);
        printf("read_sample_values:");
        for (j = 0; j < sample_count; ++j) {
            printf("%s%.9g", j == 0 ? " " : ",", (double)read_values[j]);
        }
        printf("\n");
    } else {
        printf("read_position: not-requested\n");
        printf("read_value_count: 0\n");
        printf("read_checksum: 0\n");
    }
    printf("overflow_status: %s\n",
           summary.overflow_status ? summary.overflow_status : "not-overflowed");
    printf("cleanup_attempted: %s\n", cleanup_attempted ? "true" : "false");
    printf("cleanup_status: %s\n", cleanup_status);
    printf("decode_ready: false\n");
    printf("logits_ready: false\n");
    printf("generation_ready: false\n");
    printf("generation: unsupported\n");
    printf("status: kv-owned\n");

    free(read_values);
    free(append_values);
    return 0;
}

int yvex_kv_command(int argc, char **argv)
{
    return command_kv(argc, argv);
}

void yvex_kv_help(FILE *fp)
{
    fprintf(fp, "usage: yvex kv --layers N --heads N --head-dim N --capacity N [--append-demo] [--read-position N]\n\nKV allocates a minimal F32 session-owned KV store and reports lifecycle/bounds facts. It does not run attention, decode, logits, sampling, generation, or prefill.\n");
}
