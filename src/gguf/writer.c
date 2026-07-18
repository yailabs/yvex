/*
 * writer.c - canonical immutable GGUF v3 writer-plan owner.
 *
 * Owner: TRACK.ARTIFACT.
 * Owns: exact metadata encoding, tokenizer projection, tensor directory order,
 *   checked offsets/alignment, structural prefix bytes, and plan identity.
 * Does not own: numeric execution, source reads, file creation/publication,
 *   reader roundtrip, support admission, materialization, runtime, or rendering.
 * Invariants: plan bytes are explicit little-endian GGUF v3; qtype geometry is
 *   consumed from its canonical owner; planning reads zero tensor payload.
 * Boundary: sealed structure and predicted ranges do not prove file emission.
 */
#include "writer.h"

#include "src/core/sha256.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WRITER_GGUF_VERSION 3u
#define WRITER_DEFAULT_ALIGNMENT 32u
#define WRITER_DEFAULT_BUDGET (128u * 1024u * 1024u)
#define WRITER_METADATA_CAP 96u

typedef enum {
    WRITER_META_SCALAR = 0,
    WRITER_META_TOKEN_ARRAY,
    WRITER_META_TOKEN_TYPE_ARRAY,
    WRITER_META_MERGE_ARRAY,
    WRITER_META_U64_ARRAY,
    WRITER_META_F64_ARRAY
} writer_metadata_source;

typedef struct {
    char key[128];
    unsigned int type;
    writer_metadata_source source;
    const unsigned char *string_bytes;
    size_t string_length;
    unsigned long long u64;
    double f64;
    int boolean;
    unsigned int element_type;
    unsigned long long count;
    const yvex_deepseek_gguf_metadata *map_entry;
} writer_metadata;

typedef struct {
    unsigned char *bytes;
    size_t length;
    size_t capacity;
    size_t maximum;
} writer_buffer;

struct yvex_gguf_writer_plan {
    yvex_gguf_writer_plan_summary summary;
    const yvex_quant_plan *quant_plan;
    const yvex_deepseek_gguf_map *map;
    yvex_gguf_tokenizer_metadata *tokenizer;
    yvex_gguf_writer_tensor *tensors;
    unsigned char *prefix;
    size_t prefix_bytes;
};

static int writer_fail(yvex_gguf_writer_failure *failure,
                       yvex_gguf_writer_code code,
                       const char *name,
                       unsigned long long metadata_index,
                       unsigned long long tensor_index,
                       unsigned long long expected,
                       unsigned long long actual,
                       yvex_error *err,
                       yvex_status status,
                       const char *message)
{
    if (failure) {
        memset(failure, 0, sizeof(*failure));
        failure->code = code;
        failure->metadata_index = metadata_index;
        failure->tensor_index = tensor_index;
        failure->expected = expected;
        failure->actual = actual;
        if (name)
            (void)snprintf(failure->name, sizeof(failure->name), "%s", name);
    }
    yvex_error_set(err, status, "gguf.writer.plan", message);
    return status;
}

static int writer_add_u64(unsigned long long left,
                          unsigned long long right,
                          unsigned long long *out)
{
    if (!out || left > ULLONG_MAX - right) return 0;
    *out = left + right;
    return 1;
}

/* Multiplies serialized ownership geometry and refuses overflow. */
static int writer_mul_u64(unsigned long long left,
                          unsigned long long right,
                          unsigned long long *out)
{
    if (!out || (left && right > ULLONG_MAX / left)) return 0;
    *out = left * right;
    return 1;
}

static int writer_align(unsigned long long value,
                        unsigned int alignment,
                        unsigned long long *out)
{
    unsigned long long mask;
    if (!out || !alignment || (alignment & (alignment - 1u)) != 0u)
        return 0;
    mask = alignment - 1u;
    if (value > ULLONG_MAX - mask) return 0;
    *out = (value + mask) & ~mask;
    return 1;
}

/* Orders borrowed tensor pointers lexically without mutation or allocation. */
static int writer_tensor_name_compare(const void *left, const void *right)
{
    const yvex_gguf_writer_tensor *const *left_tensor =
        (const yvex_gguf_writer_tensor *const *)left;
    const yvex_gguf_writer_tensor *const *right_tensor =
        (const yvex_gguf_writer_tensor *const *)right;
    return strcmp((*left_tensor)->name, (*right_tensor)->name);
}

/* Sorts borrowed tensor pointers to prove name uniqueness in O(n log n). */
static int writer_tensor_names_unique(const yvex_gguf_writer_tensor *tensors,
                                      unsigned long long tensor_count)
{
    const yvex_gguf_writer_tensor **ordered;
    unsigned long long index;
    int result = 1;
    if (!tensors || tensor_count > SIZE_MAX / sizeof(*ordered)) return -1;
    ordered = (const yvex_gguf_writer_tensor **)malloc(
        (size_t)tensor_count * sizeof(*ordered));
    if (!ordered) return -1;
    for (index = 0u; index < tensor_count; ++index)
        ordered[index] = &tensors[index];
    qsort(ordered, (size_t)tensor_count, sizeof(*ordered),
          writer_tensor_name_compare);
    for (index = 1u; index < tensor_count; ++index)
        if (strcmp(ordered[index - 1u]->name, ordered[index]->name) == 0) {
            result = 0;
            break;
        }
    free(ordered);
    return result;
}

static int writer_buffer_reserve(writer_buffer *buffer, size_t extra)
{
    size_t required;
    size_t capacity;
    unsigned char *grown;
    if (!buffer || extra > SIZE_MAX - buffer->length) return 0;
    required = buffer->length + extra;
    if (required > buffer->maximum) return 0;
    if (required <= buffer->capacity) return 1;
    capacity = buffer->capacity ? buffer->capacity : 4096u;
    while (capacity < required) {
        if (capacity > buffer->maximum / 2u) {
            capacity = buffer->maximum;
            break;
        }
        capacity *= 2u;
    }
    if (capacity < required) return 0;
    grown = (unsigned char *)realloc(buffer->bytes, capacity);
    if (!grown) return 0;
    buffer->bytes = grown;
    buffer->capacity = capacity;
    return 1;
}

static int writer_bytes(writer_buffer *buffer,
                        const void *bytes,
                        size_t byte_count)
{
    if (!writer_buffer_reserve(buffer, byte_count)) return 0;
    if (byte_count) memcpy(buffer->bytes + buffer->length, bytes, byte_count);
    buffer->length += byte_count;
    return 1;
}

static int writer_zero(writer_buffer *buffer, size_t byte_count)
{
    if (!writer_buffer_reserve(buffer, byte_count)) return 0;
    memset(buffer->bytes + buffer->length, 0, byte_count);
    buffer->length += byte_count;
    return 1;
}

static int writer_u32(writer_buffer *buffer, unsigned int value)
{
    unsigned char bytes[4];
    unsigned int index;
    for (index = 0u; index < 4u; ++index)
        bytes[index] = (unsigned char)(value >> (index * 8u));
    return writer_bytes(buffer, bytes, sizeof(bytes));
}

static int writer_u64(writer_buffer *buffer, unsigned long long value)
{
    unsigned char bytes[8];
    unsigned int index;
    for (index = 0u; index < 8u; ++index)
        bytes[index] = (unsigned char)(value >> (index * 8u));
    return writer_bytes(buffer, bytes, sizeof(bytes));
}

static int writer_f32(writer_buffer *buffer, float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return writer_u32(buffer, bits);
}

static int writer_f64(writer_buffer *buffer, double value)
{
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return writer_u64(buffer, bits);
}

static int writer_string(writer_buffer *buffer,
                         const unsigned char *bytes,
                         size_t byte_count)
{
    return writer_u64(buffer, byte_count) &&
           writer_bytes(buffer, bytes, byte_count);
}

static int writer_metadata_key_exists(const writer_metadata *entries,
                                      unsigned int count,
                                      const char *key)
{
    unsigned int index;
    for (index = 0u; index < count; ++index)
        if (strcmp(entries[index].key, key) == 0) return 1;
    return 0;
}

static writer_metadata *writer_metadata_new(writer_metadata *entries,
                                            unsigned int *count,
                                            const char *key)
{
    writer_metadata *entry;
    if (!entries || !count || !key || !key[0] ||
        strlen(key) >= sizeof(entries[0].key) ||
        *count >= WRITER_METADATA_CAP ||
        writer_metadata_key_exists(entries, *count, key)) return NULL;
    entry = &entries[(*count)++];
    memset(entry, 0, sizeof(*entry));
    (void)snprintf(entry->key, sizeof(entry->key), "%s", key);
    return entry;
}

static int writer_meta_string(writer_metadata *entries,
                              unsigned int *count,
                              const char *key,
                              const unsigned char *bytes,
                              size_t byte_count)
{
    writer_metadata *entry = writer_metadata_new(entries, count, key);
    if (!entry || (!bytes && byte_count)) return 0;
    entry->type = YVEX_GGUF_VALUE_STRING;
    entry->string_bytes = bytes;
    entry->string_length = byte_count;
    return 1;
}

static int writer_meta_text(writer_metadata *entries,
                            unsigned int *count,
                            const char *key,
                            const char *text)
{
    return text && writer_meta_string(
        entries, count, key, (const unsigned char *)text, strlen(text));
}

static int writer_meta_u32(writer_metadata *entries,
                           unsigned int *count,
                           const char *key,
                           unsigned long long value)
{
    writer_metadata *entry;
    if (value > UINT_MAX) return 0;
    entry = writer_metadata_new(entries, count, key);
    if (!entry) return 0;
    entry->type = YVEX_GGUF_VALUE_UINT32;
    entry->u64 = value;
    return 1;
}

static int writer_meta_bool(writer_metadata *entries,
                            unsigned int *count,
                            const char *key,
                            int value)
{
    writer_metadata *entry = writer_metadata_new(entries, count, key);
    if (!entry) return 0;
    entry->type = YVEX_GGUF_VALUE_BOOL;
    entry->boolean = value != 0;
    return 1;
}

static int writer_meta_map(writer_metadata *entries,
                           unsigned int *count,
                           const yvex_deepseek_gguf_metadata *map_entry)
{
    writer_metadata *entry;
    int custom;
    if (!map_entry) return 0;
    entry = writer_metadata_new(entries, count, map_entry->key);
    if (!entry) return 0;
    entry->map_entry = map_entry;
    custom = strncmp(map_entry->key, "yvex.", 5u) == 0;
    switch (map_entry->type) {
    case YVEX_DEEPSEEK_GGUF_METADATA_STRING:
        entry->type = YVEX_GGUF_VALUE_STRING;
        entry->string_bytes = (const unsigned char *)map_entry->string_value;
        entry->string_length = strlen(map_entry->string_value);
        return 1;
    case YVEX_DEEPSEEK_GGUF_METADATA_U64:
        if (!custom && map_entry->u64_value > UINT_MAX) return 0;
        entry->type = custom ? YVEX_GGUF_VALUE_UINT64
                             : YVEX_GGUF_VALUE_UINT32;
        entry->u64 = map_entry->u64_value;
        return 1;
    case YVEX_DEEPSEEK_GGUF_METADATA_F64:
        if (!isfinite(map_entry->f64_value) ||
            fabs(map_entry->f64_value) > FLT_MAX) return 0;
        entry->type = custom ? YVEX_GGUF_VALUE_FLOAT64
                             : YVEX_GGUF_VALUE_FLOAT32;
        entry->f64 = map_entry->f64_value;
        return 1;
    case YVEX_DEEPSEEK_GGUF_METADATA_BOOL:
        entry->type = YVEX_GGUF_VALUE_BOOL;
        entry->boolean = map_entry->bool_value != 0;
        return 1;
    case YVEX_DEEPSEEK_GGUF_METADATA_U64_ARRAY:
        entry->type = YVEX_GGUF_VALUE_ARRAY;
        entry->source = WRITER_META_U64_ARRAY;
        entry->element_type = custom ? YVEX_GGUF_VALUE_UINT64
                                     : YVEX_GGUF_VALUE_UINT32;
        entry->count = map_entry->array_count;
        return entry->count != 0u;
    case YVEX_DEEPSEEK_GGUF_METADATA_F64_ARRAY:
        entry->type = YVEX_GGUF_VALUE_ARRAY;
        entry->source = WRITER_META_F64_ARRAY;
        entry->element_type = custom ? YVEX_GGUF_VALUE_FLOAT64
                                     : YVEX_GGUF_VALUE_FLOAT32;
        entry->count = map_entry->array_count;
        return entry->count != 0u;
    default:
        return 0;
    }
}

static int writer_meta_dynamic_array(writer_metadata *entries,
                                     unsigned int *count,
                                     const char *key,
                                     writer_metadata_source source,
                                     unsigned int element_type,
                                     unsigned long long element_count)
{
    writer_metadata *entry = writer_metadata_new(entries, count, key);
    if (!entry || !element_count) return 0;
    entry->type = YVEX_GGUF_VALUE_ARRAY;
    entry->source = source;
    entry->element_type = element_type;
    entry->count = element_count;
    return 1;
}

static int writer_metadata_serialize(
    writer_buffer *buffer,
    const writer_metadata *entry,
    const yvex_gguf_tokenizer_metadata *tokenizer)
{
    unsigned long long index;
    if (!writer_string(buffer, (const unsigned char *)entry->key,
                       strlen(entry->key)) ||
        !writer_u32(buffer, entry->type)) return 0;
    switch (entry->type) {
    case YVEX_GGUF_VALUE_UINT32:
        return writer_u32(buffer, (unsigned int)entry->u64);
    case YVEX_GGUF_VALUE_UINT64:
        return writer_u64(buffer, entry->u64);
    case YVEX_GGUF_VALUE_FLOAT32:
        return writer_f32(buffer, (float)entry->f64);
    case YVEX_GGUF_VALUE_FLOAT64:
        return writer_f64(buffer, entry->f64);
    case YVEX_GGUF_VALUE_BOOL: {
        unsigned char value = entry->boolean ? 1u : 0u;
        return writer_bytes(buffer, &value, 1u);
    }
    case YVEX_GGUF_VALUE_STRING:
        return writer_string(buffer, entry->string_bytes,
                             entry->string_length);
    case YVEX_GGUF_VALUE_ARRAY:
        if (!writer_u32(buffer, entry->element_type) ||
            !writer_u64(buffer, entry->count)) return 0;
        for (index = 0u; index < entry->count; ++index) {
            if (entry->source == WRITER_META_TOKEN_ARRAY) {
                const unsigned char *bytes;
                size_t byte_count;
                int token_type;
                if (!yvex_gguf_tokenizer_token_at(
                        tokenizer, index, &bytes, &byte_count, &token_type) ||
                    !writer_string(buffer, bytes, byte_count)) return 0;
            } else if (entry->source == WRITER_META_TOKEN_TYPE_ARRAY) {
                const unsigned char *bytes;
                size_t byte_count;
                int token_type;
                if (!yvex_gguf_tokenizer_token_at(
                        tokenizer, index, &bytes, &byte_count, &token_type) ||
                    !writer_u32(buffer, (unsigned int)token_type)) return 0;
            } else if (entry->source == WRITER_META_MERGE_ARRAY) {
                const unsigned char *bytes;
                size_t byte_count;
                if (!yvex_gguf_tokenizer_merge_at(
                        tokenizer, index, &bytes, &byte_count) ||
                    !writer_string(buffer, bytes, byte_count)) return 0;
            } else if (entry->source == WRITER_META_U64_ARRAY) {
                unsigned long long value = entry->map_entry->array_values[index];
                if (entry->element_type == YVEX_GGUF_VALUE_UINT32) {
                    if (value > UINT_MAX ||
                        !writer_u32(buffer, (unsigned int)value)) return 0;
                } else if (!writer_u64(buffer, value)) return 0;
            } else if (entry->source == WRITER_META_F64_ARRAY) {
                double value = entry->map_entry->f64_array_values[index];
                if (!isfinite(value) || fabs(value) > FLT_MAX) return 0;
                if (entry->element_type == YVEX_GGUF_VALUE_FLOAT32) {
                    if (!writer_f32(buffer, (float)value)) return 0;
                } else if (!writer_f64(buffer, value)) return 0;
            } else {
                return 0;
            }
        }
        return 1;
    default:
        return 0;
    }
}

static int writer_plan_identity(yvex_gguf_writer_plan *plan)
{
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    unsigned char scalar[8];
    unsigned int index;

    yvex_sha256_init(&hash);
    if (!yvex_sha256_update(&hash, "yvex.gguf.writer.plan.v1", 24u) ||
        !yvex_sha256_update(&hash, plan->prefix, plan->prefix_bytes)) return 0;
    for (index = 0u; index < 8u; ++index)
        scalar[index] = (unsigned char)(plan->summary.final_file_bytes >>
                                        (index * 8u));
    if (!yvex_sha256_update(&hash, scalar, sizeof(scalar)) ||
        !yvex_sha256_update(&hash, plan->summary.profile_identity,
                            strlen(plan->summary.profile_identity)) ||
        !yvex_sha256_update(&hash, plan->summary.required_execution_identity,
                            strlen(plan->summary.required_execution_identity)) ||
        !yvex_sha256_final(&hash, digest)) return 0;
    yvex_sha256_hex(digest, plan->summary.writer_plan_identity);
    return 1;
}

void yvex_gguf_writer_plan_options_default(
    yvex_gguf_writer_plan_options *options)
{
    if (!options) return;
    memset(options, 0, sizeof(*options));
    options->alignment = WRITER_DEFAULT_ALIGNMENT;
    options->maximum_owned_bytes = WRITER_DEFAULT_BUDGET;
}

/*
 * Builds a tiny structurally real GGUF plan from an explicit quant fixture.
 * This internal fault seam owns its arrays/prefix and intentionally carries no
 * tokenizer evidence, so it can prove writer mechanics but never admission.
 */
int yvex_gguf_writer_plan_build_fixture(
    yvex_gguf_writer_plan **out,
    const yvex_quant_plan *quant_plan,
    const yvex_gguf_writer_fixture_tensor *fixture_tensors,
    unsigned long long tensor_count,
    const yvex_gguf_writer_plan_options *options,
    yvex_gguf_writer_failure *failure,
    yvex_error *err)
{
    const yvex_quant_plan_summary *quant =
        yvex_quant_plan_summary_get(quant_plan);
    yvex_gguf_writer_plan_options local;
    yvex_gguf_writer_plan *plan = NULL;
    writer_metadata metadata[2];
    writer_buffer buffer = {0};
    unsigned long long ordinal;
    unsigned long long relative = 0u;
    unsigned long long structural_unaligned;
    unsigned long long data_offset;
    int unique;

    if (out) *out = NULL;
    if (!out || !quant || !quant->complete || !fixture_tensors ||
        !tensor_count || tensor_count != quant->terminal_count ||
        tensor_count > SIZE_MAX / sizeof(*plan->tensors))
        return writer_fail(
            failure, YVEX_GGUF_WRITER_INVALID_ARGUMENT, NULL,
            ULLONG_MAX, ULLONG_MAX, quant ? quant->terminal_count : 0u,
            tensor_count, err, YVEX_ERR_INVALID_ARG,
            "matching explicit quant and writer fixture tensors are required");
    yvex_gguf_writer_plan_options_default(&local);
    if (options) local = *options;
    if (!local.alignment ||
        (local.alignment & (local.alignment - 1u)) != 0u ||
        !local.maximum_owned_bytes)
        return writer_fail(
            failure, YVEX_GGUF_WRITER_INVALID_ARGUMENT, NULL,
            ULLONG_MAX, ULLONG_MAX, 1u, local.alignment, err,
            YVEX_ERR_INVALID_ARG, "fixture writer options are invalid");
    plan = (yvex_gguf_writer_plan *)calloc(1u, sizeof(*plan));
    if (!plan) goto allocation_failure;
    plan->tensors = (yvex_gguf_writer_tensor *)calloc(
        (size_t)tensor_count, sizeof(*plan->tensors));
    if (!plan->tensors) goto allocation_failure;
    plan->quant_plan = quant_plan;
    plan->summary.schema_version = YVEX_GGUF_WRITER_SCHEMA_VERSION;
    plan->summary.gguf_version = WRITER_GGUF_VERSION;
    plan->summary.alignment = local.alignment;
    plan->summary.tensor_count = tensor_count;
    plan->summary.source_snapshot_identity = quant->source_snapshot_identity;
    plan->summary.mapping_identity = quant->mapping_identity;
    (void)snprintf(plan->summary.payload_identity,
                   sizeof(plan->summary.payload_identity), "%s",
                   quant->required_payload_identity);
    (void)snprintf(plan->summary.transform_identity,
                   sizeof(plan->summary.transform_identity), "%s",
                   quant->transform_identity);
    (void)snprintf(plan->summary.profile_name,
                   sizeof(plan->summary.profile_name), "%s",
                   quant->profile_name);
    (void)snprintf(plan->summary.profile_identity,
                   sizeof(plan->summary.profile_identity), "%s",
                   quant->profile_identity);
    if (local.required_execution_identity)
        (void)snprintf(plan->summary.required_execution_identity,
                       sizeof(plan->summary.required_execution_identity),
                       "%s", local.required_execution_identity);
    for (ordinal = 0u; ordinal < tensor_count; ++ordinal) {
        const yvex_quant_decision *decision =
            yvex_quant_plan_decision_at(quant_plan, ordinal);
        yvex_gguf_qtype_storage_result geometry;
        yvex_gguf_writer_tensor *tensor = &plan->tensors[ordinal];
        unsigned long long raw_end;
        unsigned int dimension;
        if (!decision || !fixture_tensors[ordinal].name ||
            !fixture_tensors[ordinal].name[0] ||
            strlen(fixture_tensors[ordinal].name) >= sizeof(tensor->name) ||
            yvex_gguf_qtype_validate_tensor_storage(
                decision->qtype, decision->dims, decision->rank,
                decision->encoded_bytes, &geometry) !=
                YVEX_GGUF_QTYPE_STORAGE_OK)
            goto tensor_failure;
        (void)snprintf(tensor->name, sizeof(tensor->name), "%s",
                       fixture_tensors[ordinal].name);
        tensor->rank = decision->rank;
        tensor->qtype = decision->qtype;
        tensor->relative_offset = relative;
        tensor->raw_bytes = decision->encoded_bytes;
        for (dimension = 0u; dimension < decision->rank; ++dimension)
            tensor->dims[dimension] = decision->dims[dimension];
        if (!writer_add_u64(relative, tensor->raw_bytes, &raw_end) ||
            !writer_align(raw_end, local.alignment, &tensor->padded_end))
            goto serialization_failure;
        tensor->padded_bytes = tensor->padded_end - relative;
        relative = tensor->padded_end;
        if (decision->qtype > YVEX_GGUF_QTYPE_ABI_UPSTREAM_MAX_ID ||
            !writer_add_u64(plan->summary.tensor_payload_bytes,
                            tensor->raw_bytes,
                            &plan->summary.tensor_payload_bytes) ||
            !writer_add_u64(plan->summary.tensor_padding_bytes,
                            tensor->padded_bytes - tensor->raw_bytes,
                            &plan->summary.tensor_padding_bytes) ||
            !writer_add_u64(
                plan->summary.qtype_payload_bytes[decision->qtype],
                tensor->raw_bytes,
                &plan->summary.qtype_payload_bytes[decision->qtype]) ||
            plan->summary.qtype_tensor_counts[decision->qtype] == ULLONG_MAX)
            goto serialization_failure;
        plan->summary.qtype_tensor_counts[decision->qtype]++;
    }
    unique = writer_tensor_names_unique(plan->tensors, tensor_count);
    if (unique < 0) goto allocation_failure;
    if (unique == 0) goto duplicate_failure;
    memset(metadata, 0, sizeof(metadata));
    (void)snprintf(metadata[0].key, sizeof(metadata[0].key),
                   "general.architecture");
    metadata[0].type = YVEX_GGUF_VALUE_STRING;
    metadata[0].source = WRITER_META_SCALAR;
    metadata[0].string_bytes = (const unsigned char *)"yvex-fixture";
    metadata[0].string_length = strlen("yvex-fixture");
    (void)snprintf(metadata[1].key, sizeof(metadata[1].key),
                   "general.alignment");
    metadata[1].type = YVEX_GGUF_VALUE_UINT32;
    metadata[1].source = WRITER_META_SCALAR;
    metadata[1].u64 = local.alignment;
    memset(&buffer, 0, sizeof(buffer));
    buffer.maximum = local.maximum_owned_bytes;
    if (!writer_u32(&buffer, YVEX_GGUF_MAGIC) ||
        !writer_u32(&buffer, WRITER_GGUF_VERSION) ||
        !writer_u64(&buffer, tensor_count) ||
        !writer_u64(&buffer, 2u) ||
        !writer_metadata_serialize(&buffer, &metadata[0], NULL) ||
        !writer_metadata_serialize(&buffer, &metadata[1], NULL))
        goto serialization_failure;
    for (ordinal = 0u; ordinal < tensor_count; ++ordinal) {
        const yvex_gguf_writer_tensor *tensor = &plan->tensors[ordinal];
        unsigned int dimension;
        if (!writer_string(&buffer, (const unsigned char *)tensor->name,
                           strlen(tensor->name)) ||
            !writer_u32(&buffer, tensor->rank))
            goto serialization_failure;
        for (dimension = 0u; dimension < tensor->rank; ++dimension)
            if (!writer_u64(&buffer, tensor->dims[dimension]))
                goto serialization_failure;
        if (!writer_u32(&buffer, tensor->qtype) ||
            !writer_u64(&buffer, tensor->relative_offset))
            goto serialization_failure;
    }
    structural_unaligned = buffer.length;
    if (!writer_align(structural_unaligned, local.alignment, &data_offset) ||
        data_offset > SIZE_MAX ||
        !writer_zero(&buffer, (size_t)(data_offset - structural_unaligned)) ||
        !writer_add_u64(data_offset, relative,
                        &plan->summary.final_file_bytes))
        goto serialization_failure;
    for (ordinal = 0u; ordinal < tensor_count; ++ordinal) {
        yvex_gguf_writer_tensor *tensor = &plan->tensors[ordinal];
        if (!writer_add_u64(data_offset, tensor->relative_offset,
                            &tensor->absolute_offset) ||
            !writer_add_u64(tensor->absolute_offset, tensor->raw_bytes,
                            &tensor->absolute_end) ||
            !writer_add_u64(data_offset, tensor->padded_end,
                            &tensor->padded_end))
            goto serialization_failure;
    }
    plan->prefix = buffer.bytes;
    plan->prefix_bytes = buffer.length;
    plan->summary.metadata_count = 2u;
    plan->summary.structural_bytes = structural_unaligned;
    plan->summary.pre_data_padding_bytes =
        data_offset - structural_unaligned;
    plan->summary.data_section_bytes = relative;
    {
        unsigned long long tensor_bytes;
        if (!writer_mul_u64(tensor_count, sizeof(*plan->tensors),
                            &tensor_bytes) ||
            !writer_add_u64(sizeof(*plan), tensor_bytes,
                            &plan->summary.owned_bytes) ||
            !writer_add_u64(plan->summary.owned_bytes, buffer.length,
                            &plan->summary.owned_bytes))
            goto serialization_failure;
    }
    if (plan->summary.owned_bytes > local.maximum_owned_bytes ||
        plan->summary.tensor_payload_bytes != quant->encoded_bytes ||
        !writer_plan_identity(plan))
        goto serialization_failure;
    plan->summary.complete = 1;
    *out = plan;
    if (failure) memset(failure, 0, sizeof(*failure));
    yvex_error_clear(err);
    return YVEX_OK;

duplicate_failure:
    yvex_gguf_writer_plan_release(&plan);
    return writer_fail(
        failure, YVEX_GGUF_WRITER_DUPLICATE_TENSOR, NULL,
        ULLONG_MAX, ULLONG_MAX, tensor_count, 0u, err, YVEX_ERR_FORMAT,
        "fixture tensor names are duplicate");
tensor_failure:
    yvex_gguf_writer_plan_release(&plan);
    return writer_fail(
        failure, YVEX_GGUF_WRITER_QTYPE_GEOMETRY, NULL,
        ULLONG_MAX, ordinal, 1u, 0u, err, YVEX_ERR_FORMAT,
        "fixture tensor name or qtype geometry is invalid");
allocation_failure:
    yvex_gguf_writer_plan_release(&plan);
    return writer_fail(
        failure, YVEX_GGUF_WRITER_ALLOCATION, NULL,
        ULLONG_MAX, ULLONG_MAX, tensor_count, 0u, err, YVEX_ERR_NOMEM,
        "fixture writer allocation failed");
serialization_failure:
    free(buffer.bytes);
    yvex_gguf_writer_plan_release(&plan);
    return writer_fail(
        failure, YVEX_GGUF_WRITER_SERIALIZATION, NULL,
        ULLONG_MAX, ULLONG_MAX, 1u, 0u, err, YVEX_ERR_BOUNDS,
        "fixture writer serialization failed");
}

/*
 * Builds the complete DeepSeek writer plan from canonical lowering, quant, and
 * verified source facts. It owns tokenizer/prefix/tensor memory, borrows the
 * sealed plan and map, performs no file IO, and reads zero source payload.
 */
int yvex_gguf_writer_build_deepseek(
    yvex_gguf_writer_plan **out,
    const yvex_quant_plan *quant_plan,
    const yvex_deepseek_gguf_map *map,
    const yvex_source_verification *verification,
    const yvex_gguf_writer_plan_options *options,
    yvex_gguf_writer_failure *failure,
    yvex_error *err)
{
    const yvex_quant_plan_summary *quant =
        yvex_quant_plan_summary_get(quant_plan);
    const yvex_deepseek_gguf_map_summary *mapping =
        yvex_model_register_deepseek_v4()->lowering.summary(map);
    yvex_gguf_writer_plan_options local;
    yvex_gguf_writer_plan *plan = NULL;
    writer_metadata metadata[WRITER_METADATA_CAP];
    unsigned int metadata_count = 0u;
    writer_buffer buffer;
    const yvex_gguf_tokenizer_summary *tokenizer_summary;
    yvex_gguf_tokenizer_failure tokenizer_failure;
    const unsigned char *raw_json;
    const unsigned char *raw_config;
    size_t raw_json_bytes;
    size_t raw_config_bytes;
    unsigned long long ordinal;
    unsigned long long relative = 0u;
    unsigned long long structural_unaligned;
    unsigned long long data_offset;
    unsigned long long data_span;
    char source_identity[32];
    char mapping_identity[32];
    int rc;

    if (out) *out = NULL;
    if (!out || !quant_plan || !map || !verification || !quant || !mapping ||
        !quant->complete || !mapping->complete ||
        yvex_quant_plan_lowering(quant_plan) != map ||
        quant->terminal_count != mapping->descriptor_count ||
        quant->terminal_count != YVEX_DEEPSEEK_GGUF_DESCRIPTOR_COUNT ||
        quant->terminal_count > SIZE_MAX / sizeof(*plan->tensors) ||
        quant->mapping_identity != mapping->mapping_identity ||
        quant->source_snapshot_identity != verification->source_snapshot_identity ||
        strcmp(quant->required_payload_identity,
               verification->manifest_payload_identity) != 0)
        return writer_fail(
            failure, YVEX_GGUF_WRITER_INVALID_ARGUMENT, NULL,
            ULLONG_MAX, ULLONG_MAX, 1u, 0u, err, YVEX_ERR_INVALID_ARG,
            "sealed quant plan, matching lowering, and verified source are required");
    yvex_gguf_writer_plan_options_default(&local);
    if (options) local = *options;
    if (!local.alignment || (local.alignment & (local.alignment - 1u)) != 0u ||
        local.maximum_owned_bytes < 1024u ||
        (local.required_execution_identity &&
         local.required_execution_identity[0] &&
         !yvex_sha256_hex_valid(local.required_execution_identity)))
        return writer_fail(
            failure, YVEX_GGUF_WRITER_INVALID_ARGUMENT, NULL,
            ULLONG_MAX, ULLONG_MAX, 1u, 0u, err, YVEX_ERR_INVALID_ARG,
            "writer alignment, ownership budget, or execution identity is invalid");
    plan = (yvex_gguf_writer_plan *)calloc(1u, sizeof(*plan));
    if (!plan)
        return writer_fail(
            failure, YVEX_GGUF_WRITER_ALLOCATION, NULL, ULLONG_MAX,
            ULLONG_MAX, sizeof(*plan), 0u, err, YVEX_ERR_NOMEM,
            "writer plan allocation failed");
    plan->quant_plan = quant_plan;
    plan->map = map;
    plan->summary.schema_version = YVEX_GGUF_WRITER_SCHEMA_VERSION;
    plan->summary.gguf_version = WRITER_GGUF_VERSION;
    plan->summary.alignment = local.alignment;
    plan->summary.tensor_count = quant->terminal_count;
    plan->summary.source_snapshot_identity = quant->source_snapshot_identity;
    plan->summary.mapping_identity = quant->mapping_identity;
    (void)snprintf(plan->summary.payload_identity,
                   sizeof(plan->summary.payload_identity), "%s",
                   quant->required_payload_identity);
    (void)snprintf(plan->summary.transform_identity,
                   sizeof(plan->summary.transform_identity), "%s",
                   quant->transform_identity);
    (void)snprintf(plan->summary.profile_name,
                   sizeof(plan->summary.profile_name), "%s",
                   quant->profile_name);
    (void)snprintf(plan->summary.profile_identity,
                   sizeof(plan->summary.profile_identity), "%s",
                   quant->profile_identity);
    if (local.required_execution_identity)
        (void)snprintf(plan->summary.required_execution_identity,
                       sizeof(plan->summary.required_execution_identity),
                       "%s", local.required_execution_identity);
    plan->tensors = (yvex_gguf_writer_tensor *)calloc(
        (size_t)quant->terminal_count, sizeof(*plan->tensors));
    if (!plan->tensors) {
        yvex_gguf_writer_plan_release(&plan);
        return writer_fail(
            failure, YVEX_GGUF_WRITER_ALLOCATION, NULL, ULLONG_MAX,
            ULLONG_MAX, quant->terminal_count, 0u, err, YVEX_ERR_NOMEM,
            "writer tensor plan allocation failed");
    }
    rc = yvex_gguf_tokenizer_metadata_load(
        &plan->tokenizer, verification, verification->tokenizer_effective_vocab_size,
        "deepseek-v3", local.maximum_owned_bytes / 2u,
        &tokenizer_failure, err);
    if (rc != YVEX_OK) {
        yvex_gguf_writer_plan_release(&plan);
        return writer_fail(
            failure, YVEX_GGUF_WRITER_METADATA_INCOMPLETE,
            tokenizer_failure.field, tokenizer_failure.record_index,
            ULLONG_MAX, tokenizer_failure.expected,
            tokenizer_failure.actual, err, (yvex_status)rc,
            "complete verified tokenizer material is required");
    }
    tokenizer_summary = yvex_gguf_tokenizer_summary_get(plan->tokenizer);
    if (!tokenizer_summary ||
        !yvex_gguf_tokenizer_raw_json(plan->tokenizer, &raw_json,
                                      &raw_json_bytes) ||
        !yvex_gguf_tokenizer_raw_config(plan->tokenizer, &raw_config,
                                        &raw_config_bytes)) {
        yvex_gguf_writer_plan_release(&plan);
        return writer_fail(
            failure, YVEX_GGUF_WRITER_METADATA_INCOMPLETE, "tokenizer",
            ULLONG_MAX, ULLONG_MAX, 1u, 0u, err, YVEX_ERR_FORMAT,
            "tokenizer metadata did not seal");
    }
    memset(metadata, 0, sizeof(metadata));
    for (ordinal = 0u; ordinal < mapping->metadata_count; ++ordinal) {
        const yvex_deepseek_gguf_metadata *entry =
            yvex_model_register_deepseek_v4()->lowering.metadata_at(map, ordinal);
        if (!writer_meta_map(metadata, &metadata_count, entry)) {
            yvex_gguf_writer_plan_release(&plan);
            return writer_fail(
                failure, writer_metadata_key_exists(
                    metadata, metadata_count, entry ? entry->key : "")
                    ? YVEX_GGUF_WRITER_DUPLICATE_METADATA
                    : YVEX_GGUF_WRITER_UNSUPPORTED_METADATA,
                entry ? entry->key : NULL, ordinal, ULLONG_MAX, 1u, 0u,
                err, YVEX_ERR_FORMAT,
                "lowering metadata cannot be serialized canonically");
        }
    }
    (void)snprintf(source_identity, sizeof(source_identity), "%016llx",
                   quant->source_snapshot_identity);
    (void)snprintf(mapping_identity, sizeof(mapping_identity), "%016llx",
                   quant->mapping_identity);
    if (!writer_meta_u32(metadata, &metadata_count, "general.alignment",
                         local.alignment) ||
        !writer_meta_text(metadata, &metadata_count, "tokenizer.ggml.pre",
                          tokenizer_summary->pre_tokenizer) ||
        !writer_meta_dynamic_array(
            metadata, &metadata_count, "tokenizer.ggml.tokens",
            WRITER_META_TOKEN_ARRAY, YVEX_GGUF_VALUE_STRING,
            tokenizer_summary->token_count) ||
        !writer_meta_dynamic_array(
            metadata, &metadata_count, "tokenizer.ggml.token_type",
            WRITER_META_TOKEN_TYPE_ARRAY, YVEX_GGUF_VALUE_INT32,
            tokenizer_summary->token_count) ||
        !writer_meta_dynamic_array(
            metadata, &metadata_count, "tokenizer.ggml.merges",
            WRITER_META_MERGE_ARRAY, YVEX_GGUF_VALUE_STRING,
            tokenizer_summary->merge_count) ||
        !writer_meta_u32(metadata, &metadata_count,
                         "tokenizer.ggml.padding_token_id",
                         tokenizer_summary->pad_token_id) ||
        !writer_meta_bool(metadata, &metadata_count,
                          "tokenizer.ggml.add_bos_token",
                          tokenizer_summary->add_bos_token) ||
        !writer_meta_bool(metadata, &metadata_count,
                          "tokenizer.ggml.add_eos_token",
                          tokenizer_summary->add_eos_token) ||
        !writer_meta_string(metadata, &metadata_count,
                            "tokenizer.huggingface.json",
                            raw_json, raw_json_bytes) ||
        !writer_meta_string(metadata, &metadata_count,
                            "yvex.tokenizer.config.json",
                            raw_config, raw_config_bytes) ||
        !writer_meta_text(metadata, &metadata_count,
                          "yvex.tokenizer.json.sha256",
                          tokenizer_summary->tokenizer_json_sha256) ||
        !writer_meta_text(metadata, &metadata_count,
                          "yvex.tokenizer.config.sha256",
                          tokenizer_summary->tokenizer_config_sha256) ||
        !writer_meta_text(metadata, &metadata_count,
                          "yvex.tokenizer.json.git_oid",
                          tokenizer_summary->tokenizer_json_git_oid) ||
        !writer_meta_text(metadata, &metadata_count,
                          "yvex.tokenizer.config.git_oid",
                          tokenizer_summary->tokenizer_config_git_oid) ||
        !writer_meta_text(metadata, &metadata_count,
                          "yvex.source.snapshot.identity", source_identity) ||
        !writer_meta_text(metadata, &metadata_count,
                          "yvex.source.payload.identity",
                          quant->required_payload_identity) ||
        !writer_meta_text(metadata, &metadata_count,
                          "yvex.transform.identity",
                          quant->transform_identity) ||
        !writer_meta_text(metadata, &metadata_count,
                          "yvex.gguf.mapping.identity", mapping_identity) ||
        !writer_meta_text(metadata, &metadata_count,
                          "yvex.quant.profile.name", quant->profile_name) ||
        !writer_meta_text(metadata, &metadata_count,
                          "yvex.quant.profile.identity",
                          quant->profile_identity) ||
        !writer_meta_u32(metadata, &metadata_count,
                         "yvex.quant.numeric_contract",
                         YVEX_QUANT_NUMERIC_CONTRACT_VERSION)) {
        yvex_gguf_writer_plan_release(&plan);
        return writer_fail(
            failure, YVEX_GGUF_WRITER_DUPLICATE_METADATA, NULL,
            metadata_count, ULLONG_MAX, 1u, 0u, err, YVEX_ERR_FORMAT,
            "required artifact metadata is duplicate or unrepresentable");
    }

    for (ordinal = 0u; ordinal < quant->terminal_count; ++ordinal) {
        const yvex_quant_decision *decision =
            yvex_quant_plan_decision_at(quant_plan, ordinal);
        const yvex_deepseek_gguf_descriptor *descriptor =
            yvex_model_register_deepseek_v4()->lowering.at(map, ordinal);
        yvex_gguf_writer_tensor *tensor = &plan->tensors[ordinal];
        yvex_gguf_qtype_storage_result geometry;
        unsigned long long next;
        unsigned int dimension;

        if (!decision || !descriptor ||
            decision->terminal_ordinal != ordinal ||
            descriptor->logical_rank != decision->rank ||
            !descriptor->emitted_name[0] ||
            strlen(descriptor->emitted_name) >= sizeof(tensor->name)) {
            yvex_gguf_writer_plan_release(&plan);
            return writer_fail(
                failure, YVEX_GGUF_WRITER_TENSOR_DIVERGENCE,
                descriptor ? descriptor->emitted_name : NULL,
                ULLONG_MAX, ordinal, ordinal,
                decision ? decision->terminal_ordinal : ULLONG_MAX,
                err, YVEX_ERR_FORMAT,
                "quant decision and lowering tensor do not biject");
        }
        memset(&geometry, 0, sizeof(geometry));
        if (yvex_gguf_qtype_validate_tensor_storage(
                decision->qtype, decision->dims, decision->rank,
                decision->encoded_bytes, &geometry) !=
                YVEX_GGUF_QTYPE_STORAGE_OK) {
            yvex_gguf_writer_plan_release(&plan);
            return writer_fail(
                failure, YVEX_GGUF_WRITER_QTYPE_GEOMETRY,
                descriptor->emitted_name, ULLONG_MAX, ordinal,
                decision->encoded_bytes, geometry.total_bytes,
                err, YVEX_ERR_FORMAT,
                "quant decision violates canonical qtype byte geometry");
        }
        (void)snprintf(tensor->name, sizeof(tensor->name), "%s",
                       descriptor->emitted_name);
        tensor->rank = decision->rank;
        tensor->qtype = decision->qtype;
        tensor->relative_offset = relative;
        tensor->raw_bytes = decision->encoded_bytes;
        for (dimension = 0u; dimension < tensor->rank; ++dimension) {
            if (decision->dims[dimension] !=
                descriptor->logical_dims[dimension]) {
                yvex_gguf_writer_plan_release(&plan);
                return writer_fail(
                    failure, YVEX_GGUF_WRITER_TENSOR_DIVERGENCE,
                    tensor->name, ULLONG_MAX, ordinal,
                    descriptor->logical_dims[dimension],
                    decision->dims[dimension], err, YVEX_ERR_FORMAT,
                    "quant and lowering tensor dimensions diverge");
            }
            tensor->dims[dimension] = decision->dims[dimension];
        }
        if (!writer_add_u64(relative, tensor->raw_bytes, &next) ||
            !writer_align(next, local.alignment, &tensor->padded_end)) {
            yvex_gguf_writer_plan_release(&plan);
            return writer_fail(
                failure, YVEX_GGUF_WRITER_ARITHMETIC_OVERFLOW,
                tensor->name, ULLONG_MAX, ordinal, ULLONG_MAX, relative,
                err, YVEX_ERR_BOUNDS,
                "tensor relative range or alignment overflowed");
        }
        tensor->padded_bytes = tensor->padded_end - relative;
        relative = tensor->padded_end;
        if (decision->qtype > YVEX_GGUF_QTYPE_ABI_UPSTREAM_MAX_ID ||
            plan->summary.qtype_tensor_counts[decision->qtype] == ULLONG_MAX ||
            !writer_add_u64(
                plan->summary.qtype_payload_bytes[decision->qtype],
                tensor->raw_bytes,
                &plan->summary.qtype_payload_bytes[decision->qtype]) ||
            !writer_add_u64(plan->summary.tensor_payload_bytes,
                            tensor->raw_bytes,
                            &plan->summary.tensor_payload_bytes) ||
            !writer_add_u64(plan->summary.tensor_padding_bytes,
                            tensor->padded_bytes - tensor->raw_bytes,
                            &plan->summary.tensor_padding_bytes)) {
            yvex_gguf_writer_plan_release(&plan);
            return writer_fail(
                failure, YVEX_GGUF_WRITER_ARITHMETIC_OVERFLOW,
                tensor->name, ULLONG_MAX, ordinal, ULLONG_MAX,
                tensor->raw_bytes, err, YVEX_ERR_BOUNDS,
                "aggregate tensor accounting overflowed");
        }
        plan->summary.qtype_tensor_counts[decision->qtype]++;
    }

    {
        int unique = writer_tensor_names_unique(
            plan->tensors, quant->terminal_count);
        if (unique <= 0) {
            yvex_gguf_writer_plan_release(&plan);
            return writer_fail(
                failure,
                unique == 0 ? YVEX_GGUF_WRITER_DUPLICATE_TENSOR
                            : YVEX_GGUF_WRITER_ALLOCATION,
                NULL, ULLONG_MAX, ULLONG_MAX, quant->terminal_count, 0u,
                err, unique == 0 ? YVEX_ERR_FORMAT : YVEX_ERR_NOMEM,
                unique == 0
                    ? "duplicate emitted tensor name refused"
                    : "tensor name uniqueness index allocation failed");
        }
    }

    memset(&buffer, 0, sizeof(buffer));
    buffer.maximum = local.maximum_owned_bytes;
    if (!writer_u32(&buffer, YVEX_GGUF_MAGIC) ||
        !writer_u32(&buffer, WRITER_GGUF_VERSION) ||
        !writer_u64(&buffer, quant->terminal_count) ||
        !writer_u64(&buffer, metadata_count)) goto serialization_failure;
    for (ordinal = 0u; ordinal < metadata_count; ++ordinal)
        if (!writer_metadata_serialize(
                &buffer, &metadata[ordinal], plan->tokenizer))
            goto serialization_failure;
    for (ordinal = 0u; ordinal < quant->terminal_count; ++ordinal) {
        yvex_gguf_writer_tensor *tensor = &plan->tensors[ordinal];
        unsigned int dimension;
        if (!writer_string(&buffer, (const unsigned char *)tensor->name,
                           strlen(tensor->name)) ||
            !writer_u32(&buffer, tensor->rank)) goto serialization_failure;
        for (dimension = 0u; dimension < tensor->rank; ++dimension)
            if (!writer_u64(&buffer, tensor->dims[dimension]))
                goto serialization_failure;
        if (!writer_u32(&buffer, tensor->qtype) ||
            !writer_u64(&buffer, tensor->relative_offset))
            goto serialization_failure;
    }
    structural_unaligned = buffer.length;
    if (!writer_align(structural_unaligned, local.alignment, &data_offset) ||
        data_offset > SIZE_MAX ||
        !writer_zero(&buffer, (size_t)(data_offset - structural_unaligned)))
        goto serialization_failure;
    data_span = relative;
    if (!writer_add_u64(data_offset, data_span,
                        &plan->summary.final_file_bytes))
        goto serialization_failure;
    for (ordinal = 0u; ordinal < quant->terminal_count; ++ordinal) {
        yvex_gguf_writer_tensor *tensor = &plan->tensors[ordinal];
        if (!writer_add_u64(data_offset, tensor->relative_offset,
                            &tensor->absolute_offset) ||
            !writer_add_u64(tensor->absolute_offset, tensor->raw_bytes,
                            &tensor->absolute_end) ||
            !writer_add_u64(data_offset, tensor->padded_end,
                            &tensor->padded_end))
            goto serialization_failure;
    }
    plan->prefix = buffer.bytes;
    plan->prefix_bytes = buffer.length;
    buffer.bytes = NULL;
    plan->summary.metadata_count = metadata_count;
    plan->summary.structural_bytes = structural_unaligned;
    plan->summary.pre_data_padding_bytes = data_offset - structural_unaligned;
    plan->summary.data_section_bytes = data_span;
    plan->summary.tokenizer_token_count = tokenizer_summary->token_count;
    plan->summary.tokenizer_merge_count = tokenizer_summary->merge_count;
    plan->summary.tokenizer_embedded_bytes =
        raw_json_bytes + raw_config_bytes;
    {
        unsigned long long tensor_bytes;
        if (!writer_mul_u64(quant->terminal_count, sizeof(*plan->tensors),
                            &tensor_bytes) ||
            !writer_add_u64(sizeof(*plan), tensor_bytes,
                            &plan->summary.owned_bytes) ||
            !writer_add_u64(plan->summary.owned_bytes, plan->prefix_bytes,
                            &plan->summary.owned_bytes) ||
            !writer_add_u64(plan->summary.owned_bytes,
                            tokenizer_summary->owned_bytes,
                            &plan->summary.owned_bytes))
            goto serialization_failure;
    }
    if (plan->summary.owned_bytes > local.maximum_owned_bytes ||
        plan->summary.tensor_payload_bytes != quant->encoded_bytes ||
        plan->summary.final_file_bytes != data_offset + data_span ||
        !writer_plan_identity(plan)) {
        unsigned long long owned_bytes = plan->summary.owned_bytes;
        yvex_gguf_writer_plan_release(&plan);
        return writer_fail(
            failure, YVEX_GGUF_WRITER_RESOURCE_LIMIT, NULL,
            ULLONG_MAX, ULLONG_MAX, local.maximum_owned_bytes,
            owned_bytes, err, YVEX_ERR_BOUNDS,
            "writer plan budget, payload accounting, or identity failed");
    }
    plan->summary.payload_bytes_read = 0u;
    plan->summary.complete = 1;
    *out = plan;
    if (failure) memset(failure, 0, sizeof(*failure));
    yvex_error_clear(err);
    return YVEX_OK;

serialization_failure:
    free(buffer.bytes);
    yvex_gguf_writer_plan_release(&plan);
    return writer_fail(
        failure, YVEX_GGUF_WRITER_SERIALIZATION, NULL,
        ULLONG_MAX, ULLONG_MAX, 1u, 0u, err, YVEX_ERR_BOUNDS,
        "GGUF structural prefix serialization overflowed or exceeded budget");
}

/* Releases all independently owned writer-plan state and nulls the handle. */
void yvex_gguf_writer_plan_release(yvex_gguf_writer_plan **plan_address)
{
    yvex_gguf_writer_plan *plan;
    if (!plan_address || !*plan_address) return;
    plan = *plan_address;
    *plan_address = NULL;
    yvex_gguf_tokenizer_metadata_release(&plan->tokenizer);
    free(plan->tensors);
    free(plan->prefix);
    memset(plan, 0, sizeof(*plan));
    free(plan);
}

const yvex_gguf_writer_plan_summary *yvex_gguf_writer_plan_summary_get(
    const yvex_gguf_writer_plan *plan)
{
    return plan && plan->summary.complete ? &plan->summary : NULL;
}

const yvex_gguf_writer_tensor *yvex_gguf_writer_plan_tensor_at(
    const yvex_gguf_writer_plan *plan, unsigned long long ordinal)
{
    return plan && plan->summary.complete && ordinal < plan->summary.tensor_count
        ? &plan->tensors[ordinal] : NULL;
}

const unsigned char *yvex_gguf_writer_plan_prefix(
    const yvex_gguf_writer_plan *plan, size_t *byte_count)
{
    if (byte_count) *byte_count = 0u;
    if (!plan || !plan->summary.complete || !byte_count) return NULL;
    *byte_count = plan->prefix_bytes;
    return plan->prefix;
}

const yvex_gguf_tokenizer_metadata *yvex_gguf_writer_plan_tokenizer(
    const yvex_gguf_writer_plan *plan)
{
    return plan && plan->summary.complete ? plan->tokenizer : NULL;
}

const char *yvex_gguf_writer_code_name(yvex_gguf_writer_code code)
{
    switch (code) {
    case YVEX_GGUF_WRITER_OK: return "ok";
    case YVEX_GGUF_WRITER_INVALID_ARGUMENT: return "invalid-argument";
    case YVEX_GGUF_WRITER_UNSEALED_PLAN: return "unsealed-plan";
    case YVEX_GGUF_WRITER_IDENTITY_MISMATCH: return "identity-mismatch";
    case YVEX_GGUF_WRITER_METADATA_INCOMPLETE: return "metadata-incomplete";
    case YVEX_GGUF_WRITER_DUPLICATE_METADATA: return "duplicate-metadata";
    case YVEX_GGUF_WRITER_UNSUPPORTED_METADATA: return "unsupported-metadata";
    case YVEX_GGUF_WRITER_DUPLICATE_TENSOR: return "duplicate-tensor";
    case YVEX_GGUF_WRITER_TENSOR_DIVERGENCE: return "tensor-divergence";
    case YVEX_GGUF_WRITER_QTYPE_GEOMETRY: return "qtype-geometry";
    case YVEX_GGUF_WRITER_ARITHMETIC_OVERFLOW: return "arithmetic-overflow";
    case YVEX_GGUF_WRITER_RESOURCE_LIMIT: return "resource-limit";
    case YVEX_GGUF_WRITER_ALLOCATION: return "allocation";
    case YVEX_GGUF_WRITER_SERIALIZATION: return "serialization";
    case YVEX_GGUF_WRITER_LIFECYCLE: return "lifecycle";
    default: return "unknown";
    }
}

/* Reports the concrete writer-plan capability without creating a file. */
int yvex_gguf_writer_supported(const char **reason)
{
    if (reason) *reason = "immutable GGUF v3 writer plans are implemented";
    return 1;
}
