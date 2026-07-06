/*
 * yvex_kv.c - KV ownership boundary.
 *
 * This file owns minimal session KV shape, allocation, append/read, lifecycle,
 * and capacity diagnostics. It does not run attention, decode, logits,
 * sampling, generation, or advanced paged/spill KV.
 */

#include <yvex/kv.h>
#include "yvex_console_private.h"
#include "yvex_cli_out.h"

#include <ctype.h>
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

typedef struct {
    const char *model;
    const char *family;
    const char *backend;
    const char *registry_path;
    int audit_output;
    int include_attention;
    int include_context;
    int include_residency;
    int include_blockers;
} yvex_kv_report_options;

typedef struct {
    unsigned long long tensor_count;
    unsigned long long total_tensor_bytes;
    int has_token_embedding;
    int has_attention_norm;
    int has_q;
    int has_k;
    int has_v;
    int has_o;
    int has_output_head;
    const yvex_tensor_info *q_tensor;
    const yvex_tensor_info *k_tensor;
    const yvex_tensor_info *v_tensor;
} yvex_kv_role_scan;

static const char *kv_bool(int value)
{
    return value ? "true" : "false";
}

static int kv_parse_output_mode(const char *value, int *audit_output)
{
    if (!value || !audit_output) return 0;
    if (strcmp(value, "normal") == 0 || strcmp(value, "table") == 0) {
        *audit_output = 0;
        return 1;
    }
    if (strcmp(value, "audit") == 0) {
        *audit_output = 1;
        return 1;
    }
    return 0;
}

static int kv_streq(const char *a, const char *b)
{
    return a && b && strcmp(a, b) == 0;
}

static int kv_contains_ci(const char *text, const char *needle)
{
    size_t needle_len;
    size_t i;
    size_t j;

    if (!text || !needle) {
        return 0;
    }
    needle_len = strlen(needle);
    if (needle_len == 0u) {
        return 1;
    }
    for (i = 0u; text[i] != '\0'; ++i) {
        for (j = 0u; j < needle_len; ++j) {
            unsigned char a = (unsigned char)text[i + j];
            unsigned char b = (unsigned char)needle[j];
            if (a == '\0' || (char)tolower(a) != (char)tolower(b)) {
                break;
            }
        }
        if (j == needle_len) {
            return 1;
        }
    }
    return 0;
}

static int kv_known_family(const char *family)
{
    return kv_streq(family, "auto") ||
           kv_streq(family, "deepseek") ||
           kv_streq(family, "glm") ||
           kv_streq(family, "qwen") ||
           kv_streq(family, "llama");
}

static const char *kv_family_from_text(const char *text)
{
    if (kv_contains_ci(text, "deepseek")) {
        return "deepseek";
    }
    if (kv_contains_ci(text, "glm")) {
        return "glm";
    }
    if (kv_contains_ci(text, "qwen")) {
        return "qwen";
    }
    if (kv_contains_ci(text, "llama")) {
        return "llama";
    }
    return "unknown";
}

static const char *kv_detect_family(const yvex_model_ref *ref,
                                    const yvex_cli_tokenizer_context *ctx,
                                    const char *input)
{
    const char *arch = NULL;

    if (ctx && ctx->model) {
        arch = yvex_arch_name(yvex_model_arch(ctx->model));
        if (arch && strcmp(arch, "unknown") != 0) {
            return arch;
        }
    }
    if (ref) {
        if (ref->family && ref->family[0]) {
            return ref->family;
        }
        if (ref->architecture && ref->architecture[0]) {
            return ref->architecture;
        }
        if (ref->alias) {
            arch = kv_family_from_text(ref->alias);
            if (strcmp(arch, "unknown") != 0) {
                return arch;
            }
        }
        if (ref->path) {
            arch = kv_family_from_text(ref->path);
            if (strcmp(arch, "unknown") != 0) {
                return arch;
            }
        }
    }
    return kv_family_from_text(input);
}

static int kv_source_only_target(const char *model)
{
    return kv_contains_ci(model, "glm-5.2-official-safetensors");
}

static const char *kv_target_class_for_model(const char *model,
                                             const yvex_model_ref *ref)
{
    const char *text = model ? model : "";

    if (kv_source_only_target(text)) {
        return "official-source-huge-model";
    }
    if (kv_contains_ci(text, "selected-embed") ||
        (ref && ref->alias && kv_contains_ci(ref->alias, "selected-embed"))) {
        return "selected-runtime-slice";
    }
    return "candidate-GGUF-path";
}

static const char *kv_target_id_for_model(const yvex_kv_report_options *options,
                                          const yvex_model_ref *ref)
{
    if (ref && ref->alias && ref->alias[0]) {
        return ref->alias;
    }
    if (options && options->model && kv_source_only_target(options->model)) {
        return "glm-5.2-official-safetensors";
    }
    if (options && options->model && kv_contains_ci(options->model, "selected-embed-rmsnorm")) {
        return "deepseek4-v4-flash-selected-embed-rmsnorm";
    }
    if (options && options->model && kv_contains_ci(options->model, "selected-embed")) {
        return "deepseek4-v4-flash-selected-embed";
    }
    return "candidate-GGUF-path";
}

static const char *kv_role_status(int present)
{
    return present ? "present" : "missing";
}

static void kv_scan_roles(const yvex_tensor_table *table, yvex_kv_role_scan *scan)
{
    unsigned long long i;

    memset(scan, 0, sizeof(*scan));
    if (!table) {
        return;
    }
    scan->tensor_count = yvex_tensor_table_count(table);
    for (i = 0ull; i < scan->tensor_count; ++i) {
        const yvex_tensor_info *tensor = yvex_tensor_table_at(table, i);
        if (!tensor) {
            continue;
        }
        scan->total_tensor_bytes += tensor->storage_bytes;
        switch (tensor->role) {
        case YVEX_TENSOR_ROLE_TOKEN_EMBEDDING:
            scan->has_token_embedding = 1;
            break;
        case YVEX_TENSOR_ROLE_ATTENTION_NORM:
            scan->has_attention_norm = 1;
            break;
        case YVEX_TENSOR_ROLE_ATTENTION_Q:
            scan->has_q = 1;
            scan->q_tensor = tensor;
            break;
        case YVEX_TENSOR_ROLE_ATTENTION_K:
            scan->has_k = 1;
            scan->k_tensor = tensor;
            break;
        case YVEX_TENSOR_ROLE_ATTENTION_V:
            scan->has_v = 1;
            scan->v_tensor = tensor;
            break;
        case YVEX_TENSOR_ROLE_ATTENTION_OUT:
            scan->has_o = 1;
            break;
        case YVEX_TENSOR_ROLE_OUTPUT_HEAD:
            scan->has_output_head = 1;
            break;
        default:
            break;
        }
    }
}

static const char *kv_attention_dependency_status(const yvex_kv_role_scan *scan)
{
    if (!scan || !scan->has_q || !scan->has_k || !scan->has_v) {
        return "blocked-missing-qkv";
    }
    return "blocked-runtime-integration";
}

static const char *kv_class_status_for_scan(const yvex_kv_role_scan *scan)
{
    if (scan && scan->has_q && scan->has_k && scan->has_v && scan->has_o) {
        return "complete";
    }
    return "partial";
}

static const char *kv_family_status_for_scan(const yvex_kv_role_scan *scan)
{
    return kv_class_status_for_scan(scan);
}

static void kv_print_phases(const char *status, const char *failed_phase)
{
    static const char *names[] = {
        "preflight",
        "resolve-model",
        "resolve-family",
        "load-family-runtime",
        "load-attention-class",
        "kv-profile",
        "kv-layout",
        "kv-shape",
        "kv-indexing",
        "kv-capacity",
        "kv-residency",
        "kv-context",
        "kv-readiness",
        "blocker-report",
        "complete",
        "failed",
        "cleanup"
    };
    unsigned long i;
    unsigned long failed_index = sizeof(names) / sizeof(names[0]);

    if (failed_phase) {
        for (i = 0u; i < sizeof(names) / sizeof(names[0]); ++i) {
            if (strcmp(names[i], failed_phase) == 0) {
                failed_index = i;
                break;
            }
        }
    }

    for (i = 0u; i < sizeof(names) / sizeof(names[0]); ++i) {
        const char *phase_status = status;
        if (!failed_phase && strcmp(names[i], "failed") == 0) {
            phase_status = "unknown";
        } else if (failed_phase) {
            if (i < failed_index) {
                phase_status = "pass";
            } else if (i == failed_index || strcmp(names[i], "failed") == 0) {
                phase_status = "failed";
            } else if (strcmp(names[i], "cleanup") == 0) {
                phase_status = "pass";
            } else {
                phase_status = "blocked";
            }
        }
        yvex_cli_out_writef(stdout, "kv_phase.%lu.name: %s\n", i, names[i]);
        yvex_cli_out_writef(stdout, "kv_phase.%lu.status: %s\n", i, phase_status);
    }
}

static void kv_print_next_rows(void)
{
    yvex_cli_out_writef(stdout, "next_required_rows: ATTENTION.CLASS.0 complete,CONTEXT.CLASS.0,RUNTIME.KV.1,RUNTIME.KV.2,RUNTIME.KV.3,real-transformer-prefill,real-decode,real-output-head-logits,GEN.DEEPSEEK.0\n");
}

static void kv_print_report_header(const yvex_kv_report_options *options,
                                   const yvex_model_ref *ref,
                                   const char *status,
                                   const char *resolved_path,
                                   const char *target_class,
                                   const char *family,
                                   const char *family_detected,
                                   const char *family_runtime_status,
                                   const char *attention_class_status,
                                   const char *kv_class_status)
{
    yvex_cli_out_writef(stdout, "kv: report\n");
    yvex_cli_out_writef(stdout, "status: %s\n", status);
    yvex_cli_out_writef(stdout, "model: %s\n", options->model ? options->model : "none");
    yvex_cli_out_writef(stdout, "model_resolved_path: %s\n", resolved_path ? resolved_path : "not-resolved");
    yvex_cli_out_writef(stdout, "target_id: %s\n", kv_target_id_for_model(options, ref));
    yvex_cli_out_writef(stdout, "target_class: %s\n", target_class ? target_class : "unknown");
    yvex_cli_out_writef(stdout, "backend: %s\n", options->backend ? options->backend : "cpu");
    yvex_cli_out_writef(stdout, "family: %s\n", family ? family : "unknown");
    yvex_cli_out_writef(stdout, "family_detected: %s\n", family_detected ? family_detected : "unknown");
    yvex_cli_out_writef(stdout, "family_requested: %s\n", options->family ? options->family : "auto");
    yvex_cli_out_writef(stdout, "family_runtime_status: %s\n", family_runtime_status ? family_runtime_status : "unknown");
    yvex_cli_out_writef(stdout, "attention_class_status: %s\n", attention_class_status ? attention_class_status : "unknown");
    yvex_cli_out_writef(stdout, "kv_class_status: %s\n", kv_class_status ? kv_class_status : "report-only");
    yvex_cli_out_writef(stdout, "kv_stage: report-only\n");
    yvex_cli_out_writef(stdout, "kv_support_status: report-only\n");
    yvex_cli_out_writef(stdout, "runtime_claim: unsupported\n");
    yvex_cli_out_writef(stdout, "generation_ready: false\n");
    yvex_cli_out_writef(stdout, "generation: unsupported-full-model\n");
    yvex_cli_out_writef(stdout, "benchmark_status: not-measured\n");
}

static void kv_print_boundary_fields(void)
{
    yvex_cli_out_writef(stdout, "diagnostic_kv_available: true\n");
    yvex_cli_out_writef(stdout, "diagnostic_kv_boundary: segment-summary/minimal diagnostic KV\n");
    yvex_cli_out_writef(stdout, "real_attention_kv: unsupported\n");
    yvex_cli_out_writef(stdout, "real_attention_kv_write_ready: false\n");
    yvex_cli_out_writef(stdout, "real_attention_kv_read_ready: false\n");
    yvex_cli_out_writef(stdout, "decode_kv_consumer_ready: false\n");
}

static void kv_print_requirement_fields(const yvex_kv_role_scan *scan,
                                        unsigned long long max_context)
{
    const int has_qkv = scan && scan->has_q && scan->has_k && scan->has_v;

    yvex_cli_out_writef(stdout, "kv_required: true\n");
    yvex_cli_out_writef(stdout, "kv_source: attention-qkv-requirements\n");
    yvex_cli_out_writef(stdout, "kv_layout: planned\n");
    yvex_cli_out_writef(stdout, "kv_layout_status: planned\n");
    yvex_cli_out_writef(stdout, "kv_dtype: planned\n");
    yvex_cli_out_writef(stdout, "kv_dtype_status: planned\n");
    yvex_cli_out_writef(stdout, "kv_layers: unknown\n");
    yvex_cli_out_writef(stdout, "kv_layers_status: planned\n");
    yvex_cli_out_writef(stdout, "kv_heads: unknown\n");
    yvex_cli_out_writef(stdout, "kv_heads_status: planned\n");
    yvex_cli_out_writef(stdout, "kv_head_dim: unknown\n");
    yvex_cli_out_writef(stdout, "kv_head_dim_status: planned\n");
    yvex_cli_out_writef(stdout, "kv_positions: context-dependent\n");
    yvex_cli_out_writef(stdout, "kv_capacity: planned\n");
    yvex_cli_out_writef(stdout, "kv_capacity_status: planned\n");
    yvex_cli_out_writef(stdout, "kv_indexing: layer-head-position-token-order\n");
    yvex_cli_out_writef(stdout, "kv_layer_indexing: planned\n");
    yvex_cli_out_writef(stdout, "kv_head_indexing: planned\n");
    yvex_cli_out_writef(stdout, "kv_position_indexing: planned\n");
    yvex_cli_out_writef(stdout, "kv_token_order_policy: prompt-order-then-append-order\n");
    yvex_cli_out_writef(stdout, "kv_residency_class: planned\n");
    yvex_cli_out_writef(stdout, "kv_residency_status: planned\n");
    yvex_cli_out_writef(stdout, "kv_cpu_bytes_estimate: unknown\n");
    yvex_cli_out_writef(stdout, "kv_cuda_bytes_estimate: unknown\n");
    yvex_cli_out_writef(stdout, "kv_host_staged_bytes_estimate: unknown\n");
    yvex_cli_out_writef(stdout, "kv_ssd_staged_status: planned\n");
    yvex_cli_out_writef(stdout, "kv_ssd_streamed_status: planned\n");
    yvex_cli_out_writef(stdout, "kv_paged_status: planned\n");
    yvex_cli_out_writef(stdout, "kv_chunked_status: planned\n");
    yvex_cli_out_writef(stdout, "kv_quantized_status: planned\n");
    yvex_cli_out_writef(stdout, "kv_managed_memory_status: planned\n");
    yvex_cli_out_writef(stdout, "context_required: true\n");
    yvex_cli_out_writef(stdout, "context_length_source: %s\n", max_context > 0ull ? "model-metadata" : "planned");
    yvex_cli_out_writef(stdout, "max_context: %s", max_context > 0ull ? "" : "unknown");
    if (max_context > 0ull) {
        yvex_cli_out_writef(stdout, "%llu", max_context);
    }
    yvex_cli_out_writef(stdout, "\n");
    yvex_cli_out_writef(stdout, "requested_context: not-requested\n");
    yvex_cli_out_writef(stdout, "context_capacity_status: planned\n");
    yvex_cli_out_writef(stdout, "context_overflow_policy: planned-refusal-before-mutation\n");
    yvex_cli_out_writef(stdout, "attention_dependency_status: %s\n", kv_attention_dependency_status(scan));
    yvex_cli_out_writef(stdout, "attention_q_required: true\n");
    yvex_cli_out_writef(stdout, "attention_k_required: true\n");
    yvex_cli_out_writef(stdout, "attention_v_required: true\n");
    yvex_cli_out_writef(stdout, "attention_q_status: %s\n", kv_role_status(scan && scan->has_q));
    yvex_cli_out_writef(stdout, "attention_k_status: %s\n", kv_role_status(scan && scan->has_k));
    yvex_cli_out_writef(stdout, "attention_v_status: %s\n", kv_role_status(scan && scan->has_v));
    yvex_cli_out_writef(stdout, "attention_o_status: %s\n", kv_role_status(scan && scan->has_o));
    yvex_cli_out_writef(stdout, "attention_runtime_ready: false\n");
    yvex_cli_out_writef(stdout, "full_transformer_attention_ready: false\n");
    yvex_cli_out_writef(stdout, "prefill_kv_write_required: true\n");
    yvex_cli_out_writef(stdout, "prefill_kv_write_ready: false\n");
    yvex_cli_out_writef(stdout, "decode_kv_read_required: true\n");
    yvex_cli_out_writef(stdout, "decode_kv_read_ready: false\n");
    yvex_cli_out_writef(stdout, "qkv_role_coverage: %s\n", has_qkv ? "present" : "missing");
}

static void kv_print_role_summary(const yvex_kv_role_scan *scan)
{
    yvex_cli_out_writef(stdout, "tensor_inventory_status: loaded-gguf-directory\n");
    yvex_cli_out_writef(stdout, "tensor_count: %llu\n", scan ? scan->tensor_count : 0ull);
    yvex_cli_out_writef(stdout, "tensor_bytes: %llu\n", scan ? scan->total_tensor_bytes : 0ull);
    yvex_cli_out_writef(stdout, "role.token_embedding.status: %s\n", kv_role_status(scan && scan->has_token_embedding));
    yvex_cli_out_writef(stdout, "role.attention_norm.status: %s\n", kv_role_status(scan && scan->has_attention_norm));
    yvex_cli_out_writef(stdout, "role.q_projection.status: %s\n", kv_role_status(scan && scan->has_q));
    yvex_cli_out_writef(stdout, "role.k_projection.status: %s\n", kv_role_status(scan && scan->has_k));
    yvex_cli_out_writef(stdout, "role.v_projection.status: %s\n", kv_role_status(scan && scan->has_v));
    yvex_cli_out_writef(stdout, "role.o_projection.status: %s\n", kv_role_status(scan && scan->has_o));
    yvex_cli_out_writef(stdout, "role.output_head.status: %s\n", kv_role_status(scan && scan->has_output_head));
}

static unsigned long long kv_report_context_length(const yvex_cli_tokenizer_context *ctx)
{
    static const char *keys[] = {
        "llama.context_length",
        "deepseek.context_length",
        "qwen.context_length",
        "glm.context_length",
        "general.context_length"
    };
    unsigned long i;
    unsigned long long value_u64 = 0ull;

    if (!ctx) {
        return 0ull;
    }
    if (ctx->model) {
        value_u64 = yvex_model_context_length(ctx->model);
        if (value_u64 > 0ull) {
            return value_u64;
        }
    }
    for (i = 0u; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        const yvex_gguf_value *value = yvex_gguf_metadata_find(ctx->gguf, keys[i]);
        if (value && yvex_gguf_value_as_u64(value, &value_u64) == YVEX_OK) {
            return value_u64;
        }
    }
    return 0ull;
}

static void kv_print_blockers(const yvex_kv_role_scan *scan)
{
    if (!scan || !scan->has_q || !scan->has_k || !scan->has_v) {
        yvex_cli_out_writef(stdout, "kv_blockers: ");
        if (!scan || !scan->has_q) {
            yvex_cli_out_writef(stdout, "q projection tensor missing; ");
        }
        if (!scan || !scan->has_k) {
            yvex_cli_out_writef(stdout, "k projection tensor missing; ");
        }
        if (!scan || !scan->has_v) {
            yvex_cli_out_writef(stdout, "v projection tensor missing; ");
        }
        yvex_cli_out_writef(stdout, "real attention-backed KV writes unsupported; decode KV consumer unsupported; context class report pending; KV capacity estimator pending\n");
        return;
    }
    yvex_cli_out_writef(stdout, "kv_blockers: real attention-backed KV writes unsupported; decode KV consumer unsupported; KV capacity estimator pending; context class report pending; full transformer prefill unsupported\n");
}

static int kv_print_source_only_report(const yvex_kv_report_options *options)
{
    const char *family = kv_streq(options->family, "auto") ? "glm" : options->family;

    if (options && !options->audit_output) {
        yvex_cli_out_writef(stdout, "report: kv\n");
        yvex_cli_out_writef(stdout, "model: %s\n", options->model ? options->model : "");
        yvex_cli_out_writef(stdout, "family: %s\n", family ? family : "unknown");
        yvex_cli_out_writef(stdout, "status: unsupported\n");
        yvex_cli_out_writef(stdout, "top_blocker: source-only target has no YVEX-produced GGUF tensor inventory\n");
        yvex_cli_out_writef(stdout, "next: V010.KV.*\n");
        yvex_cli_out_writef(stdout, "boundary: report-only, no runtime execution\n");
        return 5;
    }

    kv_print_report_header(options, NULL, "kv-report-unsupported",
                           "not-resolved-source-only-target",
                           "official-source-huge-model",
                           family, "glm", "unsupported", "unsupported",
                           "unsupported");
    kv_print_boundary_fields();
    yvex_cli_out_writef(stdout, "tensor_inventory_status: not-performed-source-only-target\n");
    yvex_cli_out_writef(stdout, "source_artifact_class: official safetensors\n");
    yvex_cli_out_writef(stdout, "target_artifact_class: future YVEX-produced GGUF\n");
    yvex_cli_out_writef(stdout, "kv_required: true\n");
    yvex_cli_out_writef(stdout, "kv_source: planned-family-mapping\n");
    yvex_cli_out_writef(stdout, "kv_layout: planned\n");
    yvex_cli_out_writef(stdout, "kv_layout_status: planned\n");
    yvex_cli_out_writef(stdout, "kv_dtype: planned\n");
    yvex_cli_out_writef(stdout, "kv_dtype_status: planned\n");
    yvex_cli_out_writef(stdout, "kv_layers: unknown\n");
    yvex_cli_out_writef(stdout, "kv_layers_status: planned\n");
    yvex_cli_out_writef(stdout, "kv_heads: unknown\n");
    yvex_cli_out_writef(stdout, "kv_heads_status: planned\n");
    yvex_cli_out_writef(stdout, "kv_head_dim: unknown\n");
    yvex_cli_out_writef(stdout, "kv_head_dim_status: planned\n");
    yvex_cli_out_writef(stdout, "kv_positions: planned\n");
    yvex_cli_out_writef(stdout, "kv_capacity: planned\n");
    yvex_cli_out_writef(stdout, "kv_capacity_status: planned\n");
    yvex_cli_out_writef(stdout, "kv_indexing: planned\n");
    yvex_cli_out_writef(stdout, "kv_layer_indexing: planned\n");
    yvex_cli_out_writef(stdout, "kv_head_indexing: planned\n");
    yvex_cli_out_writef(stdout, "kv_position_indexing: planned\n");
    yvex_cli_out_writef(stdout, "kv_token_order_policy: planned\n");
    yvex_cli_out_writef(stdout, "kv_residency_class: planned\n");
    yvex_cli_out_writef(stdout, "kv_residency_status: planned\n");
    yvex_cli_out_writef(stdout, "kv_cpu_bytes_estimate: unknown\n");
    yvex_cli_out_writef(stdout, "kv_cuda_bytes_estimate: unknown\n");
    yvex_cli_out_writef(stdout, "kv_host_staged_bytes_estimate: unknown\n");
    yvex_cli_out_writef(stdout, "kv_ssd_staged_status: planned\n");
    yvex_cli_out_writef(stdout, "kv_ssd_streamed_status: planned\n");
    yvex_cli_out_writef(stdout, "kv_paged_status: planned\n");
    yvex_cli_out_writef(stdout, "kv_chunked_status: planned\n");
    yvex_cli_out_writef(stdout, "kv_quantized_status: planned\n");
    yvex_cli_out_writef(stdout, "context_required: true\n");
    yvex_cli_out_writef(stdout, "context_length_source: planned-source-manifest\n");
    yvex_cli_out_writef(stdout, "max_context: unknown\n");
    yvex_cli_out_writef(stdout, "requested_context: not-requested\n");
    yvex_cli_out_writef(stdout, "context_capacity_status: planned\n");
    yvex_cli_out_writef(stdout, "context_overflow_policy: planned\n");
    yvex_cli_out_writef(stdout, "attention_dependency_status: unsupported-source-only\n");
    yvex_cli_out_writef(stdout, "attention_q_required: true\n");
    yvex_cli_out_writef(stdout, "attention_k_required: true\n");
    yvex_cli_out_writef(stdout, "attention_v_required: true\n");
    yvex_cli_out_writef(stdout, "attention_runtime_ready: false\n");
    yvex_cli_out_writef(stdout, "full_transformer_attention_ready: false\n");
    yvex_cli_out_writef(stdout, "prefill_kv_write_required: true\n");
    yvex_cli_out_writef(stdout, "prefill_kv_write_ready: false\n");
    yvex_cli_out_writef(stdout, "decode_kv_read_required: true\n");
    yvex_cli_out_writef(stdout, "decode_kv_read_ready: false\n");
    yvex_cli_out_writef(stdout, "kv_blockers: source-only target has no YVEX-produced GGUF tensor inventory; GLM KV mapping planned; real attention-backed KV writes unsupported\n");
    kv_print_next_rows();
    yvex_cli_out_writef(stdout, "cleanup_attempted: true\n");
    yvex_cli_out_writef(stdout, "cleanup_status: pass\n");
    kv_print_phases("planned", "load-family-runtime");
    return 5;
}

static int kv_print_unsupported_family_report(const yvex_kv_report_options *options,
                                              const char *detected,
                                              const char *reason)
{
    if (options && !options->audit_output) {
        yvex_cli_out_writef(stdout, "report: kv\n");
        yvex_cli_out_writef(stdout, "model: %s\n", options->model ? options->model : "");
        yvex_cli_out_writef(stdout, "family: %s\n", options->family ? options->family : "unknown");
        yvex_cli_out_writef(stdout, "status: unsupported\n");
        yvex_cli_out_writef(stdout, "top_blocker: %s\n", reason ? reason : "unsupported family");
        yvex_cli_out_writef(stdout, "next: V010.KV.*\n");
        yvex_cli_out_writef(stdout, "boundary: report-only, no runtime execution\n");
        return 5;
    }

    kv_print_report_header(options, NULL, "kv-report-unsupported",
                           "not-resolved",
                           "unknown",
                           options->family ? options->family : "unknown",
                           detected ? detected : "unknown",
                           "unsupported",
                           "unsupported",
                           "unsupported");
    kv_print_boundary_fields();
    yvex_cli_out_writef(stdout, "tensor_inventory_status: not-performed\n");
    yvex_cli_out_writef(stdout, "kv_required: unknown\n");
    yvex_cli_out_writef(stdout, "kv_source: unsupported-family\n");
    yvex_cli_out_writef(stdout, "kv_layout: unsupported\n");
    yvex_cli_out_writef(stdout, "kv_layout_status: unsupported\n");
    yvex_cli_out_writef(stdout, "kv_dtype: unsupported\n");
    yvex_cli_out_writef(stdout, "kv_dtype_status: unsupported\n");
    yvex_cli_out_writef(stdout, "kv_layers: unknown\n");
    yvex_cli_out_writef(stdout, "kv_layers_status: unsupported\n");
    yvex_cli_out_writef(stdout, "kv_heads: unknown\n");
    yvex_cli_out_writef(stdout, "kv_heads_status: unsupported\n");
    yvex_cli_out_writef(stdout, "kv_head_dim: unknown\n");
    yvex_cli_out_writef(stdout, "kv_head_dim_status: unsupported\n");
    yvex_cli_out_writef(stdout, "kv_positions: unknown\n");
    yvex_cli_out_writef(stdout, "kv_capacity: unsupported\n");
    yvex_cli_out_writef(stdout, "kv_capacity_status: unsupported\n");
    yvex_cli_out_writef(stdout, "kv_indexing: unsupported\n");
    yvex_cli_out_writef(stdout, "kv_layer_indexing: unsupported\n");
    yvex_cli_out_writef(stdout, "kv_head_indexing: unsupported\n");
    yvex_cli_out_writef(stdout, "kv_position_indexing: unsupported\n");
    yvex_cli_out_writef(stdout, "kv_token_order_policy: unsupported\n");
    yvex_cli_out_writef(stdout, "kv_residency_class: unsupported\n");
    yvex_cli_out_writef(stdout, "kv_residency_status: unsupported\n");
    yvex_cli_out_writef(stdout, "kv_cpu_bytes_estimate: unknown\n");
    yvex_cli_out_writef(stdout, "kv_cuda_bytes_estimate: unknown\n");
    yvex_cli_out_writef(stdout, "kv_host_staged_bytes_estimate: unknown\n");
    yvex_cli_out_writef(stdout, "kv_ssd_staged_status: unsupported\n");
    yvex_cli_out_writef(stdout, "kv_paged_status: unsupported\n");
    yvex_cli_out_writef(stdout, "kv_chunked_status: unsupported\n");
    yvex_cli_out_writef(stdout, "kv_quantized_status: unsupported\n");
    yvex_cli_out_writef(stdout, "context_required: unknown\n");
    yvex_cli_out_writef(stdout, "context_length_source: unsupported-family\n");
    yvex_cli_out_writef(stdout, "max_context: unknown\n");
    yvex_cli_out_writef(stdout, "requested_context: not-requested\n");
    yvex_cli_out_writef(stdout, "context_capacity_status: unsupported\n");
    yvex_cli_out_writef(stdout, "context_overflow_policy: unsupported\n");
    yvex_cli_out_writef(stdout, "attention_dependency_status: unsupported-family\n");
    yvex_cli_out_writef(stdout, "attention_q_required: unknown\n");
    yvex_cli_out_writef(stdout, "attention_k_required: unknown\n");
    yvex_cli_out_writef(stdout, "attention_v_required: unknown\n");
    yvex_cli_out_writef(stdout, "attention_runtime_ready: false\n");
    yvex_cli_out_writef(stdout, "full_transformer_attention_ready: false\n");
    yvex_cli_out_writef(stdout, "prefill_kv_write_required: unknown\n");
    yvex_cli_out_writef(stdout, "prefill_kv_write_ready: false\n");
    yvex_cli_out_writef(stdout, "decode_kv_read_required: unknown\n");
    yvex_cli_out_writef(stdout, "decode_kv_read_ready: false\n");
    yvex_cli_out_writef(stdout, "kv_blockers: %s\n", reason ? reason : "unsupported family");
    kv_print_next_rows();
    yvex_cli_out_writef(stdout, "cleanup_attempted: true\n");
    yvex_cli_out_writef(stdout, "cleanup_status: pass\n");
    kv_print_phases("unsupported", "resolve-family");
    return 5;
}

static int command_kv_report(int argc, char **argv)
{
    yvex_kv_report_options options;
    yvex_model_ref_options ref_options;
    yvex_model_ref ref;
    yvex_cli_tokenizer_context ctx;
    yvex_kv_role_scan scan;
    yvex_error err;
    const char *detected_family = "unknown";
    const char *family = "unknown";
    const char *target_class = "unknown";
    unsigned long long max_context = 0ull;
    int rc;
    int i;

    memset(&options, 0, sizeof(options));
    memset(&ref_options, 0, sizeof(ref_options));
    memset(&ref, 0, sizeof(ref));
    memset(&ctx, 0, sizeof(ctx));
    memset(&scan, 0, sizeof(scan));
    yvex_error_clear(&err);

    options.family = "auto";
    options.backend = "cpu";
    options.audit_output = 0;

    for (i = 3; i < argc; ++i) {
        if (strcmp(argv[i], "--model") == 0) {
            if (i + 1 >= argc) {
                yvex_cli_out_writef(stderr, "yvex: kv report --model requires FILE_OR_ALIAS\n");
                return 2;
            }
            options.model = argv[++i];
        } else if (strcmp(argv[i], "--family") == 0) {
            if (i + 1 >= argc) {
                yvex_cli_out_writef(stderr, "yvex: kv report --family requires a family name\n");
                return 2;
            }
            options.family = argv[++i];
        } else if (strcmp(argv[i], "--backend") == 0) {
            if (i + 1 >= argc) {
                yvex_cli_out_writef(stderr, "yvex: kv report --backend requires cpu or cuda\n");
                return 2;
            }
            options.backend = argv[++i];
        } else if (strcmp(argv[i], "--registry") == 0) {
            if (i + 1 >= argc) {
                yvex_cli_out_writef(stderr, "yvex: kv report --registry requires FILE\n");
                return 2;
            }
            options.registry_path = argv[++i];
        } else if (strcmp(argv[i], "--include-attention") == 0) {
            options.include_attention = 1;
        } else if (strcmp(argv[i], "--include-context") == 0) {
            options.include_context = 1;
        } else if (strcmp(argv[i], "--include-residency") == 0) {
            options.include_residency = 1;
        } else if (strcmp(argv[i], "--include-blockers") == 0) {
            options.include_blockers = 1;
        } else if (strcmp(argv[i], "--audit") == 0) {
            options.audit_output = 1;
        } else if (strcmp(argv[i], "--output") == 0) {
            if (i + 1 >= argc) {
                yvex_cli_out_writef(stderr, "yvex: kv report --output requires normal, table, or audit\n");
                return 2;
            }
            if (!kv_parse_output_mode(argv[++i], &options.audit_output)) {
                yvex_cli_out_writef(stderr, "yvex: kv report unsupported output mode: %s\n", argv[i]);
                return 2;
            }
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            yvex_kv_help(stdout);
            return 0;
        } else {
            yvex_cli_out_writef(stderr, "yvex: unknown kv report option: %s\n", argv[i]);
            yvex_cli_out_writef(stderr, "Try 'yvex help kv' for usage.\n");
            return 2;
        }
    }

    if (!options.model) {
        yvex_cli_out_writef(stderr, "usage: yvex kv report --model FILE_OR_ALIAS [--family auto|deepseek|glm|qwen|llama] [--backend cpu|cuda]\n");
        return 2;
    }
    if (!kv_streq(options.backend, "cpu") && !kv_streq(options.backend, "cuda")) {
        yvex_cli_out_writef(stderr, "yvex: kv report --backend must be cpu or cuda\n");
        return 2;
    }
    if (!kv_known_family(options.family)) {
        return kv_print_unsupported_family_report(&options, "unknown", "unknown family requested; KV class report is not generic model support");
    }
    if (kv_source_only_target(options.model)) {
        return kv_print_source_only_report(&options);
    }

    ref_options.allow_registry = 1;
    ref_options.registry_path = options.registry_path;
    rc = yvex_model_ref_resolve(&ref, options.model, &ref_options, &err);
    if (rc != YVEX_OK) {
        if (!options.audit_output) {
            yvex_cli_out_writef(stdout, "report: kv\n");
            yvex_cli_out_writef(stdout, "model: %s\n", options.model ? options.model : "");
            yvex_cli_out_writef(stdout, "family: %s\n", kv_streq(options.family, "auto") ? "unknown" : options.family);
            yvex_cli_out_writef(stdout, "status: fail\n");
            yvex_cli_out_writef(stdout, "top_blocker: %s\n", err.message[0] ? err.message : "model resolution failed");
            yvex_cli_out_writef(stdout, "next: V010.KV.*\n");
            yvex_cli_out_writef(stdout, "boundary: report-only, no runtime execution\n");
            yvex_model_ref_clear(&ref);
            return exit_for_status(rc);
        }
        kv_print_report_header(&options, &ref, "kv-report-fail",
                               ref.path, "unknown",
                               kv_streq(options.family, "auto") ? "unknown" : options.family,
                               "unknown", "failed", "failed", "failed");
        kv_print_boundary_fields();
        yvex_cli_out_writef(stdout, "tensor_inventory_status: failed\n");
        yvex_cli_out_writef(stdout, "reason: %s\n", err.message[0] ? err.message : "model resolution failed");
        yvex_cli_out_writef(stdout, "cleanup_attempted: true\n");
        yvex_cli_out_writef(stdout, "cleanup_status: pass\n");
        kv_print_phases("failed", "resolve-model");
        yvex_model_ref_clear(&ref);
        return exit_for_status(rc);
    }

    rc = open_model_context(ref.path, &ctx, &err);
    if (rc != YVEX_OK) {
        if (!options.audit_output) {
            yvex_cli_out_writef(stdout, "report: kv\n");
            yvex_cli_out_writef(stdout, "model: %s\n", options.model ? options.model : "");
            yvex_cli_out_writef(stdout, "family: %s\n", kv_streq(options.family, "auto") ? "unknown" : options.family);
            yvex_cli_out_writef(stdout, "status: fail\n");
            yvex_cli_out_writef(stdout, "top_blocker: %s\n", err.message[0] ? err.message : "tensor inventory failed");
            yvex_cli_out_writef(stdout, "next: V010.KV.*\n");
            yvex_cli_out_writef(stdout, "boundary: report-only, no runtime execution\n");
            yvex_model_ref_clear(&ref);
            return exit_for_status(rc);
        }
        kv_print_report_header(&options, &ref, "kv-report-fail",
                               ref.path, kv_target_class_for_model(options.model, &ref),
                               kv_streq(options.family, "auto") ? "unknown" : options.family,
                               "unknown", "failed", "failed", "failed");
        kv_print_boundary_fields();
        yvex_cli_out_writef(stdout, "tensor_inventory_status: failed\n");
        yvex_cli_out_writef(stdout, "reason: %s\n", err.message[0] ? err.message : "tensor inventory failed");
        yvex_cli_out_writef(stdout, "cleanup_attempted: true\n");
        yvex_cli_out_writef(stdout, "cleanup_status: pass\n");
        kv_print_phases("failed", "kv-profile");
        yvex_model_ref_clear(&ref);
        return exit_for_status(rc);
    }

    detected_family = kv_detect_family(&ref, &ctx, options.model);
    family = kv_streq(options.family, "auto") ? detected_family : options.family;
    if (!kv_streq(options.family, "auto") && !kv_streq(options.family, detected_family)) {
        close_model_context(&ctx);
        yvex_model_ref_clear(&ref);
        return kv_print_unsupported_family_report(&options, detected_family,
                                                  "requested family does not match resolved model family");
    }
    if (!kv_streq(family, "deepseek")) {
        close_model_context(&ctx);
        yvex_model_ref_clear(&ref);
        return kv_print_unsupported_family_report(&options, detected_family,
                                                  "KV class report is currently implemented for DeepSeek-family GGUF artifacts only");
    }

    kv_scan_roles(ctx.table, &scan);
    target_class = kv_target_class_for_model(options.model, &ref);
    max_context = kv_report_context_length(&ctx);

    if (!options.audit_output) {
        yvex_cli_out_writef(stdout, "report: kv\n");
        yvex_cli_out_writef(stdout, "model: %s\n", options.model ? options.model : "");
        yvex_cli_out_writef(stdout, "family: %s\n", family ? family : "unknown");
        yvex_cli_out_writef(stdout, "status: %s\n", kv_class_status_for_scan(&scan));
        yvex_cli_out_writef(stdout, "top_blocker: real attention-backed KV unsupported\n");
        yvex_cli_out_writef(stdout, "next: V010.KV.*\n");
        yvex_cli_out_writef(stdout, "boundary: report-only, no runtime execution\n");
        close_model_context(&ctx);
        yvex_model_ref_clear(&ref);
        return 0;
    }

    kv_print_report_header(&options, &ref, "kv-report",
                           ref.path,
                           target_class,
                           family,
                           detected_family,
                           kv_family_status_for_scan(&scan),
                           kv_family_status_for_scan(&scan),
                           kv_class_status_for_scan(&scan));
    yvex_cli_out_writef(stdout, "report_options.include_attention: %s\n", kv_bool(options.include_attention));
    yvex_cli_out_writef(stdout, "report_options.include_context: %s\n", kv_bool(options.include_context));
    yvex_cli_out_writef(stdout, "report_options.include_residency: %s\n", kv_bool(options.include_residency));
    yvex_cli_out_writef(stdout, "report_options.include_blockers: %s\n", kv_bool(options.include_blockers));
    kv_print_boundary_fields();
    kv_print_role_summary(&scan);
    kv_print_requirement_fields(&scan, max_context);
    kv_print_blockers(&scan);
    kv_print_next_rows();
    yvex_cli_out_writef(stdout, "backend_allocation_attempted: false\n");
    yvex_cli_out_writef(stdout, "full_kv_allocation_proof: false\n");
    yvex_cli_out_writef(stdout, "cuda_full_kv_allocation_proof: false\n");
    yvex_cli_out_writef(stdout, "paged_kv_implementation: false\n");
    yvex_cli_out_writef(stdout, "chunked_kv_runtime_implementation: false\n");
    yvex_cli_out_writef(stdout, "ssd_backed_kv: false\n");
    yvex_cli_out_writef(stdout, "quantized_kv_runtime: false\n");
    yvex_cli_out_writef(stdout, "full_transformer_prefill_ready: false\n");
    yvex_cli_out_writef(stdout, "decode_ready: false\n");
    yvex_cli_out_writef(stdout, "logits_ready: false\n");
    yvex_cli_out_writef(stdout, "sampling_ready: false\n");
    yvex_cli_out_writef(stdout, "runtime_execution_ready: false\n");
    yvex_cli_out_writef(stdout, "cleanup_attempted: true\n");
    yvex_cli_out_writef(stdout, "cleanup_status: pass\n");
    kv_print_phases("pass", NULL);

    close_model_context(&ctx);
    yvex_model_ref_clear(&ref);
    return 0;
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
    if (strcmp(argv[2], "report") == 0) {
        return command_kv_report(argc, argv);
    }

    for (i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--layers") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &shape.layer_count)) {
                yvex_cli_out_writef(stderr, "yvex: --layers requires a positive integer\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(argv[i], "--heads") == 0 || strcmp(argv[i], "--lanes") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &shape.kv_head_count)) {
                yvex_cli_out_writef(stderr, "yvex: --heads requires a positive integer\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(argv[i], "--head-dim") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &shape.head_dim)) {
                yvex_cli_out_writef(stderr, "yvex: --head-dim requires a positive integer\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(argv[i], "--capacity") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &shape.capacity)) {
                yvex_cli_out_writef(stderr, "yvex: --capacity requires a positive integer\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(argv[i], "--append-demo") == 0) {
            append_demo = 1;
        } else if (strcmp(argv[i], "--read-position") == 0) {
            if (i + 1 >= argc || !parse_ull_allow_zero(argv[i + 1], &read_position)) {
                yvex_cli_out_writef(stderr, "yvex: --read-position requires a non-negative integer\n");
                return 2;
            }
            read_requested = 1;
            i += 1;
        } else {
            yvex_cli_out_writef(stderr, "yvex: unknown kv option: %s\n", argv[i]);
            yvex_cli_out_writef(stderr, "Try 'yvex help kv' for usage.\n");
            return 2;
        }
    }

    if (shape.layer_count == 0ull || shape.kv_head_count == 0ull ||
        shape.head_dim == 0ull || shape.capacity == 0ull) {
        yvex_cli_out_writef(stderr, "usage: yvex kv --layers N --heads N --head-dim N --capacity N [--append-demo] [--read-position N]\n");
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
            yvex_cli_out_writef(stderr, "yvex: failed to allocate KV append demo buffer\n");
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
            yvex_cli_out_writef(stderr, "yvex: failed to allocate KV read buffer\n");
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

    yvex_cli_out_writef(stdout, "kv: ownership\n");
    yvex_cli_out_writef(stdout, "kv_created: true\n");
    yvex_cli_out_writef(stdout, "session_owned: %s\n", summary.session_owned ? "true" : "false");
    yvex_cli_out_writef(stdout, "layers: %llu\n", summary.layer_count);
    yvex_cli_out_writef(stdout, "heads: %llu\n", summary.kv_head_count);
    yvex_cli_out_writef(stdout, "head_dim: %llu\n", summary.head_dim);
    yvex_cli_out_writef(stdout, "capacity: %llu\n", summary.context_length);
    yvex_cli_out_writef(stdout, "dtype: %s\n", summary.dtype ? summary.dtype : "F32");
    yvex_cli_out_writef(stdout, "values_per_position: %llu\n", summary.values_per_position);
    yvex_cli_out_writef(stdout, "bytes_per_position: %llu\n", summary.bytes_per_position);
    yvex_cli_out_writef(stdout, "planned_bytes: %llu\n", summary.bytes);
    yvex_cli_out_writef(stdout, "allocated_bytes: %llu\n", summary.allocated_bytes);
    yvex_cli_out_writef(stdout, "append_count: %llu\n", summary.append_count);
    yvex_cli_out_writef(stdout, "read_count: %llu\n", summary.read_count);
    yvex_cli_out_writef(stdout, "written_positions: %llu\n", summary.written_positions);
    yvex_cli_out_writef(stdout, "last_appended_position: %llu\n", appended_position);
    if (read_requested) {
        unsigned long long sample_count = value_count < 8ull ? value_count : 8ull;
        unsigned long long j;
        yvex_cli_out_writef(stdout, "read_position: %llu\n", read_position);
        yvex_cli_out_writef(stdout, "read_value_count: %llu\n", value_count);
        yvex_cli_out_writef(stdout, "read_checksum: %llu\n", read_checksum);
        yvex_cli_out_writef(stdout, "read_sample_values:");
        for (j = 0; j < sample_count; ++j) {
            yvex_cli_out_writef(stdout, "%s%.9g", j == 0 ? " " : ",", (double)read_values[j]);
        }
        yvex_cli_out_writef(stdout, "\n");
    } else {
        yvex_cli_out_writef(stdout, "read_position: not-requested\n");
        yvex_cli_out_writef(stdout, "read_value_count: 0\n");
        yvex_cli_out_writef(stdout, "read_checksum: 0\n");
    }
    yvex_cli_out_writef(stdout, "overflow_status: %s\n",
           summary.overflow_status ? summary.overflow_status : "not-overflowed");
    yvex_cli_out_writef(stdout, "cleanup_attempted: %s\n", cleanup_attempted ? "true" : "false");
    yvex_cli_out_writef(stdout, "cleanup_status: %s\n", cleanup_status);
    yvex_cli_out_writef(stdout, "decode_ready: false\n");
    yvex_cli_out_writef(stdout, "logits_ready: false\n");
    yvex_cli_out_writef(stdout, "generation_ready: false\n");
    yvex_cli_out_writef(stdout, "generation: unsupported\n");
    yvex_cli_out_writef(stdout, "status: kv-owned\n");

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
    yvex_cli_out_writef(fp,
            "usage: yvex kv report --model FILE_OR_ALIAS [--family auto|deepseek|glm|qwen|llama] [--backend cpu|cuda] [--audit | --output normal|table|audit] [options]\n"
            "usage: yvex kv --layers N --heads N --head-dim N --capacity N [--append-demo] [--read-position N]\n\n"
            "kv report:\n"
            "  KV cache class and requirements report over model/family facts.\n"
            "  Reports layout, dtype, layer/head/position indexing, capacity, context dependency, residency class, attention dependency, and blockers.\n"
            "  This is a report-only boundary: it does not allocate full runtime KV, write real attention-backed KV, execute decode, generate, evaluate, benchmark, or report throughput.\n"
            "  Existing diagnostic KV is segment-summary/minimal only and is not DeepSeek KV, real attention KV, full transformer KV, or decode-ready KV.\n"
            "  Options: --include-attention --include-context --include-residency --include-blockers --registry FILE\n"
            "  Default report output is compact. Use --audit for full diagnostic fields.\n\n"
            "minimal KV diagnostic:\n"
            "  --append-demo allocates a minimal F32 session-owned KV store and reports lifecycle/bounds facts.\n"
            "  It remains diagnostic/minimal and does not run attention, decode, logits, sampling, generation, or prefill.\n");
}
