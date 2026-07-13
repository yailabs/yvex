/*
 * yvex_deepseek_gguf_map.c - canonical DeepSeek logical emission plan.
 *
 * Owner: src/model/target.
 * Owns: deterministic composition of architecture, complete source coverage,
 *   typed roles, GGUF names, logical shapes, transforms, metadata, and indexes.
 * Does not own: source IO, payload reads, numeric conversion, qtype policy,
 *   physical layout, writer bytes, materialization, runtime, or rendering.
 * Invariants: 69,187 source rows are consumed once into 1,360 descriptors;
 *   construction performs no IO and publishes only a complete immutable plan.
 * Boundary: this is the payload/writer blueprint, not an artifact capability.
 */
#include "yvex_deepseek_gguf_map.h"

#include "yvex_model_target_catalog.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAP_METADATA_CAP 48u

typedef struct {
    unsigned long long hash;
    unsigned long long value_plus_one;
} map_index_slot;

struct yvex_deepseek_gguf_map {
    yvex_deepseek_gguf_map_allocator allocator;
    yvex_deepseek_gguf_descriptor *descriptors;
    yvex_deepseek_gguf_contribution *contributions;
    unsigned char *source_consumed;
    map_index_slot *source_index;
    map_index_slot *emitted_index;
    map_index_slot *role_index;
    unsigned long long source_index_capacity;
    unsigned long long emitted_index_capacity;
    unsigned long long role_index_capacity;
    yvex_deepseek_gguf_metadata metadata[MAP_METADATA_CAP];
    yvex_deepseek_gguf_map_summary summary;
};

typedef struct {
    yvex_deepseek_gguf_map *map;
    const yvex_deepseek_v4_ir *ir;
    const yvex_deepseek_tensor_coverage *coverage;
    yvex_deepseek_gguf_map_failure *failure;
    yvex_error *err;
} map_builder;

static void *map_default_allocate(size_t size, void *context)
{
    (void)context;
    return malloc(size);
}

static void map_default_release(void *allocation, void *context)
{
    (void)context;
    free(allocation);
}

static unsigned long long map_hash_bytes(unsigned long long hash,
                                         const void *data, size_t size)
{
    const unsigned char *bytes = (const unsigned char *)data;
    size_t i;
    for (i = 0u; i < size; ++i) {
        hash ^= bytes[i];
        hash *= 1099511628211ull;
    }
    return hash;
}

static unsigned long long map_hash_string(const char *text)
{
    return map_hash_bytes(1469598103934665603ull, text, strlen(text) + 1u);
}

static unsigned long long map_hash_u64(unsigned long long hash,
                                       unsigned long long value)
{
    return map_hash_bytes(hash, &value, sizeof(value));
}

static int map_checked_mul(unsigned long long left,
                           unsigned long long right,
                           unsigned long long *out)
{
    if (!out || (left && right > ULLONG_MAX / left)) return 0;
    *out = left * right;
    return 1;
}

static void map_failure_clear(yvex_deepseek_gguf_map_failure *failure)
{
    if (!failure) return;
    memset(failure, 0, sizeof(*failure));
    failure->layer_index = YVEX_DEEPSEEK_GGUF_NO_INDEX;
    failure->predictor_index = YVEX_DEEPSEEK_GGUF_NO_INDEX;
    failure->expert_index = YVEX_DEEPSEEK_GGUF_NO_INDEX;
}

/* Records one mapping refusal without publishing a partial plan. */
static int map_reject(map_builder *builder,
                      yvex_deepseek_gguf_map_failure_code code,
                      yvex_tensor_role role,
                      yvex_deepseek_tensor_scope scope,
                      unsigned long long layer,
                      unsigned long long predictor,
                      unsigned long long expert,
                      const char *source_name,
                      const char *emitted_name,
                      unsigned long long expected,
                      unsigned long long actual)
{
    yvex_status status = code == YVEX_DEEPSEEK_GGUF_MAP_FAILURE_ALLOCATION
                             ? YVEX_ERR_NOMEM
                             : (code == YVEX_DEEPSEEK_GGUF_MAP_FAILURE_INVALID_ARGUMENT
                                    ? YVEX_ERR_INVALID_ARG
                                    : YVEX_ERR_FORMAT);
    yvex_deepseek_gguf_map_failure *failure = builder ? builder->failure : NULL;

    if (failure) {
        map_failure_clear(failure);
        failure->code = code;
        failure->role = role;
        failure->scope = scope;
        failure->layer_index = layer;
        failure->predictor_index = predictor;
        failure->expert_index = expert;
        failure->expected = expected;
        failure->actual = actual;
        (void)snprintf(failure->source_name, sizeof(failure->source_name),
                       "%s", source_name ? source_name : "");
        (void)snprintf(failure->emitted_name, sizeof(failure->emitted_name),
                       "%s", emitted_name ? emitted_name : "");
    }
    yvex_error_setf(builder ? builder->err : NULL, status,
                    "deepseek_gguf_map",
                    "%s role=%s source=%s emitted=%s layer=%llu expert=%llu expected=%llu actual=%llu",
                    yvex_deepseek_gguf_map_failure_name(code),
                    yvex_tensor_role_name(role), source_name ? source_name : "none",
                    emitted_name ? emitted_name : "none", layer, expert,
                    expected, actual);
    return status;
}

static void *map_allocate_zero(yvex_deepseek_gguf_map *map, size_t size)
{
    void *allocation = map->allocator.allocate(size, map->allocator.context);
    if (allocation) memset(allocation, 0, size);
    return allocation;
}

static unsigned long long map_index_capacity(unsigned long long count)
{
    unsigned long long capacity = 8u;
    if (count > ULLONG_MAX / 2u) return 0u;
    count *= 2u;
    while (capacity < count) {
        if (capacity > ULLONG_MAX / 2u) return 0u;
        capacity *= 2u;
    }
    return capacity;
}

/* Inserts one hash/value pair; callers resolve hash collisions by key compare. */
static int map_index_insert(map_index_slot *slots,
                            unsigned long long capacity,
                            unsigned long long hash,
                            unsigned long long value,
                            unsigned long long *slot_out)
{
    unsigned long long slot;
    unsigned long long probes;

    if (!slots || !capacity || (capacity & (capacity - 1u)) != 0u) return 0;
    slot = hash & (capacity - 1u);
    for (probes = 0u; probes < capacity; ++probes) {
        if (!slots[slot].value_plus_one) {
            slots[slot].hash = hash;
            slots[slot].value_plus_one = value + 1u;
            if (slot_out) *slot_out = slot;
            return 1;
        }
        slot = (slot + 1u) & (capacity - 1u);
    }
    return 0;
}

/* Inserts an emitted name only when no prior descriptor owns the same key. */
static int map_emitted_index_insert(yvex_deepseek_gguf_map *map,
                                    unsigned long long hash,
                                    unsigned long long value)
{
    unsigned long long slot = hash & (map->emitted_index_capacity - 1u);
    unsigned long long probes;

    for (probes = 0u; probes < map->emitted_index_capacity; ++probes) {
        map_index_slot *entry = &map->emitted_index[slot];
        if (!entry->value_plus_one) {
            entry->hash = hash;
            entry->value_plus_one = value + 1u;
            return 1;
        }
        if (entry->hash == hash &&
            strcmp(map->descriptors[entry->value_plus_one - 1u].emitted_name,
                   map->descriptors[value].emitted_name) == 0) {
            return 0;
        }
        slot = (slot + 1u) & (map->emitted_index_capacity - 1u);
    }
    return 0;
}

/* Inserts one typed role tuple and refuses an already-owned tuple. */
static int map_role_index_insert(yvex_deepseek_gguf_map *map,
                                 unsigned long long hash,
                                 unsigned long long value)
{
    const yvex_deepseek_gguf_descriptor *candidate = &map->descriptors[value];
    unsigned long long slot = hash & (map->role_index_capacity - 1u);
    unsigned long long probes;

    for (probes = 0u; probes < map->role_index_capacity; ++probes) {
        map_index_slot *entry = &map->role_index[slot];
        if (!entry->value_plus_one) {
            entry->hash = hash;
            entry->value_plus_one = value + 1u;
            return 1;
        }
        if (entry->hash == hash) {
            const yvex_deepseek_gguf_descriptor *current =
                &map->descriptors[entry->value_plus_one - 1u];
            if (current->role == candidate->role &&
                current->scope == candidate->scope &&
                current->layer_index == candidate->layer_index &&
                current->predictor_index == candidate->predictor_index) {
                return 0;
            }
        }
        slot = (slot + 1u) & (map->role_index_capacity - 1u);
    }
    return 0;
}

static const yvex_deepseek_tensor_coverage_row *map_source(
    map_builder *builder, const char *name, unsigned long long *row_index)
{
    const yvex_deepseek_tensor_coverage_row *row =
        yvex_deepseek_tensor_coverage_find(builder->coverage, name);
    if (!row || !yvex_deepseek_tensor_coverage_find_index(
                    builder->coverage, name, row_index)) return NULL;
    return row;
}

/* Starts one immutable logical descriptor and indexes its typed identity. */
static int map_descriptor_begin(map_builder *builder,
                                yvex_tensor_role role,
                                yvex_deepseek_tensor_collection collection,
                                yvex_deepseek_tensor_scope scope,
                                unsigned long long layer,
                                unsigned long long predictor,
                                yvex_deepseek_gguf_transform transform,
                                unsigned int qtype,
                                unsigned int rank,
                                const unsigned long long *dims,
                                const unsigned int *axis_order,
                                unsigned long long expert_count,
                                unsigned long long *descriptor_index)
{
    yvex_deepseek_gguf_map *map = builder->map;
    yvex_deepseek_gguf_descriptor *descriptor;
    yvex_gguf_name_provenance provenance;
    const char *reason = NULL;
    unsigned long long index = map->summary.descriptor_count;
    unsigned long long role_hash = 1469598103934665603ull;
    unsigned int i;

    if (index >= YVEX_DEEPSEEK_GGUF_DESCRIPTOR_COUNT) {
        return map_reject(builder, YVEX_DEEPSEEK_GGUF_MAP_FAILURE_ACCOUNTING,
                          role, scope, layer, predictor,
                          YVEX_DEEPSEEK_GGUF_NO_INDEX, NULL, NULL,
                          YVEX_DEEPSEEK_GGUF_DESCRIPTOR_COUNT, index + 1u);
    }
    descriptor = &map->descriptors[index];
    memset(descriptor, 0, sizeof(*descriptor));
    descriptor->role = role;
    descriptor->collection = collection;
    descriptor->scope = scope;
    descriptor->layer_index = layer;
    descriptor->predictor_index = predictor;
    descriptor->expert_count = expert_count;
    descriptor->transform = transform;
    descriptor->forced_qtype = qtype;
    descriptor->logical_rank = rank;
    descriptor->contribution_offset = map->summary.source_contribution_count;
    for (i = 0u; i < rank; ++i) {
        descriptor->logical_dims[i] = dims[i];
        descriptor->source_axis_for_logical[i] = axis_order[i];
    }
    if (!yvex_gguf_name_map_resolve(
            role, scope == YVEX_DEEPSEEK_TENSOR_SCOPE_MTP, layer, predictor,
            descriptor->emitted_name, sizeof(descriptor->emitted_name),
            &provenance, &reason)) {
        return map_reject(builder, YVEX_DEEPSEEK_GGUF_MAP_FAILURE_NAME,
                          role, scope, layer, predictor,
                          YVEX_DEEPSEEK_GGUF_NO_INDEX, NULL, reason, 1u, 0u);
    }
    descriptor->name_provenance = provenance;
    if (!yvex_gguf_layout_map_shape_supported(
            role, qtype, rank, dims, &reason)) {
        return map_reject(builder, YVEX_DEEPSEEK_GGUF_MAP_FAILURE_LAYOUT,
                          role, scope, layer, predictor,
                          YVEX_DEEPSEEK_GGUF_NO_INDEX, NULL,
                          descriptor->emitted_name, 1u, 0u);
    }
    if (!map_emitted_index_insert(
            map, map_hash_string(descriptor->emitted_name), index)) {
        return map_reject(builder, YVEX_DEEPSEEK_GGUF_MAP_FAILURE_DUPLICATE_NAME,
                          role, scope, layer, predictor,
                          YVEX_DEEPSEEK_GGUF_NO_INDEX, NULL,
                          descriptor->emitted_name, 0u, index);
    }
    role_hash = map_hash_u64(role_hash, (unsigned long long)role);
    role_hash = map_hash_u64(role_hash, (unsigned long long)scope);
    role_hash = map_hash_u64(role_hash, layer);
    role_hash = map_hash_u64(role_hash, predictor);
    if (!map_role_index_insert(map, role_hash, index)) {
        return map_reject(builder, YVEX_DEEPSEEK_GGUF_MAP_FAILURE_DUPLICATE_NAME,
                          role, scope, layer, predictor,
                          YVEX_DEEPSEEK_GGUF_NO_INDEX, NULL,
                          descriptor->emitted_name, 0u, index);
    }
    descriptor->identity = map_hash_string(descriptor->emitted_name);
    descriptor->identity = map_hash_u64(descriptor->identity, transform);
    descriptor->identity = map_hash_u64(descriptor->identity, qtype);
    for (i = 0u; i < rank; ++i)
        descriptor->identity = map_hash_u64(descriptor->identity, dims[i]);
    map->summary.descriptor_count++;
    map->summary.collection_counts[collection]++;
    if (scope == YVEX_DEEPSEEK_TENSOR_SCOPE_MTP)
        map->summary.mtp_descriptor_count++;
    else
        map->summary.trunk_descriptor_count++;
    if (provenance == YVEX_GGUF_NAME_PINNED_STANDARD)
        map->summary.pinned_standard_count++;
    else if (provenance == YVEX_GGUF_NAME_SEMANTIC_STANDARD)
        map->summary.semantic_standard_count++;
    else
        map->summary.extension_count++;
    *descriptor_index = index;
    return YVEX_OK;
}

/* Copies one exact coverage row into the plan and indexes the source name. */
static int map_descriptor_add_source(
    map_builder *builder, unsigned long long descriptor_index,
    const char *name, yvex_deepseek_gguf_contribution_kind kind,
    yvex_native_dtype expected_dtype, unsigned long long expected_expert)
{
    yvex_deepseek_gguf_map *map = builder->map;
    yvex_deepseek_gguf_descriptor *descriptor =
        &map->descriptors[descriptor_index];
    const yvex_deepseek_tensor_coverage_row *row;
    yvex_deepseek_gguf_contribution *contribution;
    unsigned long long row_index;
    unsigned long long index = map->summary.source_contribution_count;
    unsigned long long hash;
    unsigned int dimension;

    row = map_source(builder, name, &row_index);
    if (!row) {
        return map_reject(builder, YVEX_DEEPSEEK_GGUF_MAP_FAILURE_MISSING_SOURCE,
                          descriptor->role, descriptor->scope,
                          descriptor->layer_index, descriptor->predictor_index,
                          expected_expert, name, descriptor->emitted_name, 1u, 0u);
    }
    if (row_index >= YVEX_DEEPSEEK_GGUF_SOURCE_COUNT ||
        map->source_consumed[row_index]) {
        return map_reject(builder, YVEX_DEEPSEEK_GGUF_MAP_FAILURE_DUPLICATE_SOURCE,
                          descriptor->role, descriptor->scope,
                          descriptor->layer_index, descriptor->predictor_index,
                          expected_expert, name, descriptor->emitted_name, 1u, 2u);
    }
    if (row->collection != descriptor->collection ||
        row->scope != descriptor->scope ||
        row->layer_index != descriptor->layer_index) {
        return map_reject(builder,
                          YVEX_DEEPSEEK_GGUF_MAP_FAILURE_COVERAGE_ROW,
                          descriptor->role, descriptor->scope,
                          descriptor->layer_index, descriptor->predictor_index,
                          expected_expert, name, descriptor->emitted_name,
                          descriptor->collection, row->collection);
    }
    if (row->source->dtype != expected_dtype) {
        return map_reject(builder, YVEX_DEEPSEEK_GGUF_MAP_FAILURE_SOURCE_DTYPE,
                          descriptor->role, descriptor->scope,
                          descriptor->layer_index, descriptor->predictor_index,
                          expected_expert, name, descriptor->emitted_name,
                          expected_dtype, row->source->dtype);
    }
    if (expected_expert != YVEX_DEEPSEEK_GGUF_NO_INDEX &&
        row->expert_index != expected_expert) {
        return map_reject(builder, YVEX_DEEPSEEK_GGUF_MAP_FAILURE_EXPERT_SEQUENCE,
                          descriptor->role, descriptor->scope,
                          descriptor->layer_index, descriptor->predictor_index,
                          expected_expert, name, descriptor->emitted_name,
                          expected_expert, row->expert_index);
    }
    if (index >= YVEX_DEEPSEEK_GGUF_SOURCE_COUNT) {
        return map_reject(builder, YVEX_DEEPSEEK_GGUF_MAP_FAILURE_ACCOUNTING,
                          descriptor->role, descriptor->scope,
                          descriptor->layer_index, descriptor->predictor_index,
                          expected_expert, name, descriptor->emitted_name,
                          YVEX_DEEPSEEK_GGUF_SOURCE_COUNT, index + 1u);
    }
    contribution = &map->contributions[index];
    (void)snprintf(contribution->source_name,
                   sizeof(contribution->source_name), "%s", name);
    contribution->source_dtype = row->source->dtype;
    contribution->source_rank = row->source->rank;
    for (dimension = 0u; dimension < row->source->rank && dimension < 2u;
         ++dimension)
        contribution->source_dims[dimension] = row->source->dims[dimension];
    contribution->kind = kind;
    contribution->source_row_index = row_index;
    contribution->expert_index = expected_expert;
    contribution->descriptor_index = descriptor_index;
    map->source_consumed[row_index] = 1u;
    hash = map_hash_string(name);
    if (!map_index_insert(map->source_index, map->source_index_capacity,
                          hash, index, NULL)) {
        return map_reject(builder, YVEX_DEEPSEEK_GGUF_MAP_FAILURE_DUPLICATE_SOURCE,
                          descriptor->role, descriptor->scope,
                          descriptor->layer_index, descriptor->predictor_index,
                          expected_expert, name, descriptor->emitted_name, 1u, 2u);
    }
    descriptor->contribution_count++;
    descriptor->identity = map_hash_bytes(descriptor->identity, name,
                                          strlen(name) + 1u);
    map->summary.source_contribution_count++;
    return YVEX_OK;
}

static void map_reverse_shape(const yvex_native_weight_info *source,
                              unsigned long long dims[4],
                              unsigned int axis[4])
{
    unsigned int i;
    memset(dims, 0, sizeof(unsigned long long) * 4u);
    memset(axis, 0, sizeof(unsigned int) * 4u);
    for (i = 0u; i < source->rank; ++i) {
        dims[i] = source->dims[source->rank - i - 1u];
        axis[i] = source->rank - i - 1u;
    }
}

/* Adds a direct or I64-to-I32 descriptor from one source contribution. */
static int map_add_direct(map_builder *builder, yvex_tensor_role role,
                          yvex_deepseek_tensor_collection collection,
                          yvex_deepseek_tensor_scope scope,
                          unsigned long long layer,
                          unsigned long long predictor,
                          const char *source_name,
                          yvex_deepseek_gguf_transform transform,
                          yvex_native_dtype dtype)
{
    const yvex_deepseek_tensor_coverage_row *row;
    unsigned long long source_index;
    unsigned long long descriptor_index;
    unsigned long long dims[4];
    unsigned int axis[4];
    unsigned int qtype = transform == YVEX_DEEPSEEK_GGUF_TRANSFORM_I64_TO_I32
                             ? 26u : YVEX_GGUF_NO_FORCED_QTYPE;
    int rc;

    row = map_source(builder, source_name, &source_index);
    if (!row) return map_reject(builder,
        YVEX_DEEPSEEK_GGUF_MAP_FAILURE_MISSING_SOURCE, role, scope, layer,
        predictor, YVEX_DEEPSEEK_GGUF_NO_INDEX, source_name, NULL, 1u, 0u);
    map_reverse_shape(row->source, dims, axis);
    rc = map_descriptor_begin(builder, role, collection, scope, layer,
                              predictor, transform, qtype, row->source->rank,
                              dims, axis, 0u, &descriptor_index);
    if (rc != YVEX_OK) return rc;
    return map_descriptor_add_source(
        builder, descriptor_index, source_name,
        transform == YVEX_DEEPSEEK_GGUF_TRANSFORM_I64_TO_I32
            ? YVEX_DEEPSEEK_GGUF_CONTRIBUTION_ROUTING_TABLE
            : YVEX_DEEPSEEK_GGUF_CONTRIBUTION_PRIMARY,
        dtype, YVEX_DEEPSEEK_GGUF_NO_INDEX);
}

/* Groups one FP8 weight and its E8M0 scale into one logical descriptor. */
static int map_add_fp8(map_builder *builder, yvex_tensor_role role,
                       yvex_deepseek_tensor_collection collection,
                       yvex_deepseek_tensor_scope scope,
                       unsigned long long layer,
                       unsigned long long predictor,
                       const char *base)
{
    char weight[256];
    char scale[256];
    const yvex_deepseek_tensor_coverage_row *row;
    unsigned long long source_index;
    unsigned long long descriptor_index;
    unsigned long long dims[4];
    unsigned int axis[4];
    int rc;

    (void)snprintf(weight, sizeof(weight), "%s.weight", base);
    (void)snprintf(scale, sizeof(scale), "%s.scale", base);
    row = map_source(builder, weight, &source_index);
    if (!row) return map_reject(builder,
        YVEX_DEEPSEEK_GGUF_MAP_FAILURE_MISSING_SOURCE, role, scope, layer,
        predictor, YVEX_DEEPSEEK_GGUF_NO_INDEX, weight, NULL, 1u, 0u);
    map_reverse_shape(row->source, dims, axis);
    rc = map_descriptor_begin(
        builder, role, collection, scope, layer, predictor,
        YVEX_DEEPSEEK_GGUF_TRANSFORM_FP8_E4M3_E8M0,
        YVEX_GGUF_NO_FORCED_QTYPE, row->source->rank, dims, axis, 0u,
        &descriptor_index);
    if (rc != YVEX_OK) return rc;
    rc = map_descriptor_add_source(
        builder, descriptor_index, weight,
        YVEX_DEEPSEEK_GGUF_CONTRIBUTION_PRIMARY,
        YVEX_NATIVE_DTYPE_F8_E4M3, YVEX_DEEPSEEK_GGUF_NO_INDEX);
    if (rc != YVEX_OK) return rc;
    return map_descriptor_add_source(
        builder, descriptor_index, scale,
        YVEX_DEEPSEEK_GGUF_CONTRIBUTION_SCALE,
        YVEX_NATIVE_DTYPE_F8_E8M0, YVEX_DEEPSEEK_GGUF_NO_INDEX);
}

/* Aggregates one projection across all routed experts in numeric expert order. */
static int map_add_experts(map_builder *builder, yvex_tensor_role role,
                           yvex_deepseek_tensor_scope scope,
                           unsigned long long layer,
                           unsigned long long predictor,
                           const char *prefix,
                           const char *projection,
                           unsigned long long expert_count)
{
    char weight[256];
    char scale[256];
    const yvex_deepseek_tensor_coverage_row *first;
    unsigned long long source_index;
    unsigned long long descriptor_index;
    unsigned long long logical_width;
    unsigned long long dims[4];
    unsigned int axis[4] = {1u, 0u, YVEX_DEEPSEEK_GGUF_AGGREGATED_AXIS, 0u};
    unsigned long long expert;
    int rc;

    (void)snprintf(weight, sizeof(weight), "%s.ffn.experts.0.%s.weight",
                   prefix, projection);
    first = map_source(builder, weight, &source_index);
    if (!first || first->source->rank != 2u ||
        !map_checked_mul(first->source->dims[1], 2u, &logical_width)) {
        return map_reject(builder,
            first ? YVEX_DEEPSEEK_GGUF_MAP_FAILURE_ARITHMETIC_OVERFLOW
                  : YVEX_DEEPSEEK_GGUF_MAP_FAILURE_MISSING_SOURCE,
            role, scope, layer, predictor, 0u, weight, NULL, 1u, 0u);
    }
    dims[0] = logical_width;
    dims[1] = first->source->dims[0];
    dims[2] = expert_count;
    dims[3] = 0u;
    rc = map_descriptor_begin(
        builder, role, YVEX_DEEPSEEK_TENSOR_COLLECTION_ROUTED_EXPERT,
        scope, layer, predictor, YVEX_DEEPSEEK_GGUF_TRANSFORM_EXPERT_MXFP4,
        39u, 3u, dims, axis, expert_count, &descriptor_index);
    if (rc != YVEX_OK) return rc;
    for (expert = 0u; expert < expert_count; ++expert) {
        (void)snprintf(weight, sizeof(weight), "%s.ffn.experts.%llu.%s.weight",
                       prefix, expert, projection);
        (void)snprintf(scale, sizeof(scale), "%s.ffn.experts.%llu.%s.scale",
                       prefix, expert, projection);
        rc = map_descriptor_add_source(
            builder, descriptor_index, weight,
            YVEX_DEEPSEEK_GGUF_CONTRIBUTION_EXPERT_WEIGHT,
            YVEX_NATIVE_DTYPE_I8, expert);
        if (rc != YVEX_OK) return rc;
        rc = map_descriptor_add_source(
            builder, descriptor_index, scale,
            YVEX_DEEPSEEK_GGUF_CONTRIBUTION_EXPERT_SCALE,
            YVEX_NATIVE_DTYPE_F8_E8M0, expert);
        if (rc != YVEX_OK) return rc;
    }
    return YVEX_OK;
}

static int map_add_mhc(map_builder *builder, const char *prefix,
                       const char *kind, yvex_deepseek_tensor_scope scope,
                       unsigned long long layer, unsigned long long predictor)
{
    const yvex_tensor_role roles[3] = {
        strcmp(kind, "attn") == 0 ? YVEX_TENSOR_ROLE_HC_ATTENTION_FUNCTION
                                  : YVEX_TENSOR_ROLE_HC_FFN_FUNCTION,
        strcmp(kind, "attn") == 0 ? YVEX_TENSOR_ROLE_HC_ATTENTION_BASE
                                  : YVEX_TENSOR_ROLE_HC_FFN_BASE,
        strcmp(kind, "attn") == 0 ? YVEX_TENSOR_ROLE_HC_ATTENTION_SCALE
                                  : YVEX_TENSOR_ROLE_HC_FFN_SCALE
    };
    const char *suffixes[3] = {"fn", "base", "scale"};
    char name[256];
    unsigned int i;
    int rc;

    for (i = 0u; i < 3u; ++i) {
        (void)snprintf(name, sizeof(name), "%s.hc_%s_%s", prefix, kind,
                       suffixes[i]);
        rc = map_add_direct(builder, roles[i],
                            YVEX_DEEPSEEK_TENSOR_COLLECTION_MHC, scope,
                            layer, predictor, name,
                            YVEX_DEEPSEEK_GGUF_TRANSFORM_DIRECT,
                            YVEX_NATIVE_DTYPE_F32);
        if (rc != YVEX_OK) return rc;
    }
    return YVEX_OK;
}

static int map_add_attention(map_builder *builder, const char *prefix,
                             const yvex_deepseek_v4_layer_spec *layer,
                             yvex_deepseek_tensor_scope scope,
                             unsigned long long predictor)
{
    char name[256];
    char base[256];
    int rc;

#define MAP_DIRECT(role_id, collection_id, suffix, dtype_id)                    \
    do {                                                                         \
        (void)snprintf(name, sizeof(name), "%s.%s", prefix, suffix);          \
        rc = map_add_direct(builder, role_id, collection_id, scope,              \
                            layer->layer_index, predictor, name,                  \
                            YVEX_DEEPSEEK_GGUF_TRANSFORM_DIRECT, dtype_id);       \
        if (rc != YVEX_OK) return rc;                                             \
    } while (0)
#define MAP_FP8(role_id, collection_id, suffix)                                  \
    do {                                                                         \
        (void)snprintf(base, sizeof(base), "%s.%s", prefix, suffix);          \
        rc = map_add_fp8(builder, role_id, collection_id, scope,                  \
                         layer->layer_index, predictor, base);                    \
        if (rc != YVEX_OK) return rc;                                             \
    } while (0)
    MAP_DIRECT(YVEX_TENSOR_ROLE_ATTENTION_SINKS,
               YVEX_DEEPSEEK_TENSOR_COLLECTION_ATTENTION,
               "attn.attn_sink", YVEX_NATIVE_DTYPE_F32);
    MAP_DIRECT(YVEX_TENSOR_ROLE_ATTENTION_Q_A_NORM,
               YVEX_DEEPSEEK_TENSOR_COLLECTION_ATTENTION,
               "attn.q_norm.weight", YVEX_NATIVE_DTYPE_BF16);
    MAP_DIRECT(YVEX_TENSOR_ROLE_ATTENTION_KV_NORM,
               YVEX_DEEPSEEK_TENSOR_COLLECTION_ATTENTION,
               "attn.kv_norm.weight", YVEX_NATIVE_DTYPE_BF16);
    MAP_FP8(YVEX_TENSOR_ROLE_ATTENTION_KV,
            YVEX_DEEPSEEK_TENSOR_COLLECTION_ATTENTION, "attn.wkv");
    MAP_FP8(YVEX_TENSOR_ROLE_ATTENTION_Q_A,
            YVEX_DEEPSEEK_TENSOR_COLLECTION_ATTENTION, "attn.wq_a");
    MAP_FP8(YVEX_TENSOR_ROLE_ATTENTION_Q_B,
            YVEX_DEEPSEEK_TENSOR_COLLECTION_ATTENTION, "attn.wq_b");
    MAP_FP8(YVEX_TENSOR_ROLE_ATTENTION_OUT_A,
            YVEX_DEEPSEEK_TENSOR_COLLECTION_ATTENTION, "attn.wo_a");
    MAP_FP8(YVEX_TENSOR_ROLE_ATTENTION_OUT_B,
            YVEX_DEEPSEEK_TENSOR_COLLECTION_ATTENTION, "attn.wo_b");
    if (layer->compressor_required) {
        MAP_DIRECT(YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_APE,
                   YVEX_DEEPSEEK_TENSOR_COLLECTION_COMPRESSOR,
                   "attn.compressor.ape", YVEX_NATIVE_DTYPE_F32);
        MAP_DIRECT(YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_NORM,
                   YVEX_DEEPSEEK_TENSOR_COLLECTION_COMPRESSOR,
                   "attn.compressor.norm.weight", YVEX_NATIVE_DTYPE_BF16);
        MAP_DIRECT(YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_GATE,
                   YVEX_DEEPSEEK_TENSOR_COLLECTION_COMPRESSOR,
                   "attn.compressor.wgate.weight", YVEX_NATIVE_DTYPE_BF16);
        MAP_DIRECT(YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_KV,
                   YVEX_DEEPSEEK_TENSOR_COLLECTION_COMPRESSOR,
                   "attn.compressor.wkv.weight", YVEX_NATIVE_DTYPE_BF16);
    }
    if (layer->indexer_required) {
        MAP_DIRECT(YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_APE,
                   YVEX_DEEPSEEK_TENSOR_COLLECTION_INDEXER,
                   "attn.indexer.compressor.ape", YVEX_NATIVE_DTYPE_F32);
        MAP_DIRECT(YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_NORM,
                   YVEX_DEEPSEEK_TENSOR_COLLECTION_INDEXER,
                   "attn.indexer.compressor.norm.weight", YVEX_NATIVE_DTYPE_BF16);
        MAP_DIRECT(YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_GATE,
                   YVEX_DEEPSEEK_TENSOR_COLLECTION_INDEXER,
                   "attn.indexer.compressor.wgate.weight", YVEX_NATIVE_DTYPE_BF16);
        MAP_DIRECT(YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_KV,
                   YVEX_DEEPSEEK_TENSOR_COLLECTION_INDEXER,
                   "attn.indexer.compressor.wkv.weight", YVEX_NATIVE_DTYPE_BF16);
        MAP_FP8(YVEX_TENSOR_ROLE_INDEXER_ATTENTION_Q_B,
                YVEX_DEEPSEEK_TENSOR_COLLECTION_INDEXER,
                "attn.indexer.wq_b");
        MAP_DIRECT(YVEX_TENSOR_ROLE_INDEXER_PROJECTION,
                   YVEX_DEEPSEEK_TENSOR_COLLECTION_INDEXER,
                   "attn.indexer.weights_proj.weight", YVEX_NATIVE_DTYPE_BF16);
    }
#undef MAP_FP8
#undef MAP_DIRECT
    return YVEX_OK;
}

static int map_add_moe(map_builder *builder, const char *prefix,
                       const yvex_deepseek_v4_layer_spec *layer,
                       yvex_deepseek_tensor_scope scope,
                       unsigned long long predictor)
{
    char name[256];
    char base[256];
    int rc;

    rc = map_add_experts(builder, YVEX_TENSOR_ROLE_MOE_EXPERT_GATE, scope,
                         layer->layer_index, predictor, prefix, "w1",
                         layer->moe.routed_experts);
    if (rc != YVEX_OK) return rc;
    rc = map_add_experts(builder, YVEX_TENSOR_ROLE_MOE_EXPERT_DOWN, scope,
                         layer->layer_index, predictor, prefix, "w2",
                         layer->moe.routed_experts);
    if (rc != YVEX_OK) return rc;
    rc = map_add_experts(builder, YVEX_TENSOR_ROLE_MOE_EXPERT_UP, scope,
                         layer->layer_index, predictor, prefix, "w3",
                         layer->moe.routed_experts);
    if (rc != YVEX_OK) return rc;
    (void)snprintf(base, sizeof(base), "%s.ffn.shared_experts.w1", prefix);
    rc = map_add_fp8(builder, YVEX_TENSOR_ROLE_MOE_SHARED_EXPERT_GATE,
                     YVEX_DEEPSEEK_TENSOR_COLLECTION_SHARED_EXPERT, scope,
                     layer->layer_index, predictor, base);
    if (rc != YVEX_OK) return rc;
    (void)snprintf(base, sizeof(base), "%s.ffn.shared_experts.w2", prefix);
    rc = map_add_fp8(builder, YVEX_TENSOR_ROLE_MOE_SHARED_EXPERT_DOWN,
                     YVEX_DEEPSEEK_TENSOR_COLLECTION_SHARED_EXPERT, scope,
                     layer->layer_index, predictor, base);
    if (rc != YVEX_OK) return rc;
    (void)snprintf(base, sizeof(base), "%s.ffn.shared_experts.w3", prefix);
    rc = map_add_fp8(builder, YVEX_TENSOR_ROLE_MOE_SHARED_EXPERT_UP,
                     YVEX_DEEPSEEK_TENSOR_COLLECTION_SHARED_EXPERT, scope,
                     layer->layer_index, predictor, base);
    if (rc != YVEX_OK) return rc;
    (void)snprintf(name, sizeof(name), "%s.ffn.gate.weight", prefix);
    rc = map_add_direct(builder, YVEX_TENSOR_ROLE_MOE_ROUTER,
                        YVEX_DEEPSEEK_TENSOR_COLLECTION_ROUTER, scope,
                        layer->layer_index, predictor, name,
                        YVEX_DEEPSEEK_GGUF_TRANSFORM_DIRECT,
                        YVEX_NATIVE_DTYPE_BF16);
    if (rc != YVEX_OK) return rc;
    if (layer->moe.router_class == YVEX_DEEPSEEK_V4_ROUTER_HASH_TOKEN_ID) {
        (void)snprintf(name, sizeof(name), "%s.ffn.gate.tid2eid", prefix);
        return map_add_direct(builder, YVEX_TENSOR_ROLE_MOE_ROUTER_TABLE,
                              YVEX_DEEPSEEK_TENSOR_COLLECTION_ROUTER, scope,
                              layer->layer_index, predictor, name,
                              YVEX_DEEPSEEK_GGUF_TRANSFORM_I64_TO_I32,
                              YVEX_NATIVE_DTYPE_I64);
    }
    (void)snprintf(name, sizeof(name), "%s.ffn.gate.bias", prefix);
    return map_add_direct(builder, YVEX_TENSOR_ROLE_MOE_ROUTER_BIAS,
                          YVEX_DEEPSEEK_TENSOR_COLLECTION_ROUTER, scope,
                          layer->layer_index, predictor, name,
                          YVEX_DEEPSEEK_GGUF_TRANSFORM_DIRECT,
                          YVEX_NATIVE_DTYPE_F32);
}

static int map_add_layer(map_builder *builder, const char *prefix,
                         const yvex_deepseek_v4_layer_spec *layer,
                         yvex_deepseek_tensor_scope scope,
                         unsigned long long predictor)
{
    char name[256];
    int rc = map_add_attention(builder, prefix, layer, scope, predictor);
    if (rc != YVEX_OK) return rc;
    (void)snprintf(name, sizeof(name), "%s.attn_norm.weight", prefix);
    rc = map_add_direct(builder, YVEX_TENSOR_ROLE_ATTENTION_NORM,
                        YVEX_DEEPSEEK_TENSOR_COLLECTION_NORM, scope,
                        layer->layer_index, predictor, name,
                        YVEX_DEEPSEEK_GGUF_TRANSFORM_DIRECT,
                        YVEX_NATIVE_DTYPE_BF16);
    if (rc != YVEX_OK) return rc;
    (void)snprintf(name, sizeof(name), "%s.ffn_norm.weight", prefix);
    rc = map_add_direct(builder, YVEX_TENSOR_ROLE_FFN_NORM,
                        YVEX_DEEPSEEK_TENSOR_COLLECTION_NORM, scope,
                        layer->layer_index, predictor, name,
                        YVEX_DEEPSEEK_GGUF_TRANSFORM_DIRECT,
                        YVEX_NATIVE_DTYPE_BF16);
    if (rc != YVEX_OK) return rc;
    rc = map_add_mhc(builder, prefix, "attn", scope, layer->layer_index,
                     predictor);
    if (rc != YVEX_OK) return rc;
    rc = map_add_mhc(builder, prefix, "ffn", scope, layer->layer_index,
                     predictor);
    if (rc != YVEX_OK) return rc;
    return map_add_moe(builder, prefix, layer, scope, predictor);
}

static int map_add_metadata_string(map_builder *builder, const char *key,
                                   const char *value)
{
    yvex_deepseek_gguf_map *map = builder->map;
    unsigned long long i;
    yvex_deepseek_gguf_metadata *entry;
    if (!key || !value || map->summary.metadata_count >= MAP_METADATA_CAP)
        return map_reject(builder, YVEX_DEEPSEEK_GGUF_MAP_FAILURE_METADATA,
                          YVEX_TENSOR_ROLE_UNKNOWN,
                          YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL,
                          YVEX_DEEPSEEK_GGUF_NO_INDEX,
                          YVEX_DEEPSEEK_GGUF_NO_INDEX,
                          YVEX_DEEPSEEK_GGUF_NO_INDEX, key, NULL,
                          MAP_METADATA_CAP, map->summary.metadata_count + 1u);
    for (i = 0u; i < map->summary.metadata_count; ++i)
        if (strcmp(map->metadata[i].key, key) == 0)
            return map_reject(builder, YVEX_DEEPSEEK_GGUF_MAP_FAILURE_METADATA,
                              YVEX_TENSOR_ROLE_UNKNOWN,
                              YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL,
                              YVEX_DEEPSEEK_GGUF_NO_INDEX,
                              YVEX_DEEPSEEK_GGUF_NO_INDEX,
                              YVEX_DEEPSEEK_GGUF_NO_INDEX, key, NULL, 1u, 2u);
    entry = &map->metadata[map->summary.metadata_count++];
    (void)snprintf(entry->key, sizeof(entry->key), "%s", key);
    entry->type = YVEX_DEEPSEEK_GGUF_METADATA_STRING;
    (void)snprintf(entry->string_value, sizeof(entry->string_value), "%s", value);
    return YVEX_OK;
}

static int map_add_metadata_u64(map_builder *builder, const char *key,
                                unsigned long long value)
{
    int rc = map_add_metadata_string(builder, key, "");
    yvex_deepseek_gguf_metadata *entry;
    if (rc != YVEX_OK) return rc;
    entry = &builder->map->metadata[builder->map->summary.metadata_count - 1u];
    entry->type = YVEX_DEEPSEEK_GGUF_METADATA_U64;
    entry->u64_value = value;
    return YVEX_OK;
}

static int map_add_metadata_bool(map_builder *builder, const char *key,
                                 int value)
{
    int rc = map_add_metadata_string(builder, key, "");
    yvex_deepseek_gguf_metadata *entry;
    if (rc != YVEX_OK) return rc;
    entry = &builder->map->metadata[builder->map->summary.metadata_count - 1u];
    entry->type = YVEX_DEEPSEEK_GGUF_METADATA_BOOL;
    entry->bool_value = value != 0;
    return YVEX_OK;
}

static int map_add_metadata_f64(map_builder *builder, const char *key,
                                double value)
{
    int rc = map_add_metadata_string(builder, key, "");
    yvex_deepseek_gguf_metadata *entry;
    if (rc != YVEX_OK) return rc;
    entry = &builder->map->metadata[builder->map->summary.metadata_count - 1u];
    entry->type = YVEX_DEEPSEEK_GGUF_METADATA_F64;
    entry->f64_value = value;
    return YVEX_OK;
}

static int map_add_metadata_array(map_builder *builder, const char *key,
                                  const unsigned long long *values,
                                  unsigned int count)
{
    int rc;
    yvex_deepseek_gguf_metadata *entry;
    if (!values || !count || count > 64u)
        return map_reject(builder, YVEX_DEEPSEEK_GGUF_MAP_FAILURE_METADATA,
                          YVEX_TENSOR_ROLE_UNKNOWN,
                          YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL,
                          YVEX_DEEPSEEK_GGUF_NO_INDEX,
                          YVEX_DEEPSEEK_GGUF_NO_INDEX,
                          YVEX_DEEPSEEK_GGUF_NO_INDEX, key, NULL, 64u, count);
    rc = map_add_metadata_string(builder, key, "");
    if (rc != YVEX_OK) return rc;
    entry = &builder->map->metadata[builder->map->summary.metadata_count - 1u];
    entry->type = YVEX_DEEPSEEK_GGUF_METADATA_U64_ARRAY;
    memcpy(entry->array_values, values,
           (size_t)count * sizeof(entry->array_values[0]));
    entry->array_count = count;
    return YVEX_OK;
}

/* Adds one bounded per-layer floating-point metadata array. */
static int map_add_metadata_f64_array(map_builder *builder, const char *key,
                                      const double *values,
                                      unsigned int count)
{
    yvex_deepseek_gguf_metadata *entry;
    unsigned int i;
    int rc;

    if (!values || !count || count > 64u) {
        return map_reject(builder, YVEX_DEEPSEEK_GGUF_MAP_FAILURE_METADATA,
                          YVEX_TENSOR_ROLE_UNKNOWN,
                          YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL,
                          YVEX_DEEPSEEK_GGUF_NO_INDEX,
                          YVEX_DEEPSEEK_GGUF_NO_INDEX,
                          YVEX_DEEPSEEK_GGUF_NO_INDEX, key, NULL, 64u, count);
    }
    rc = map_add_metadata_string(builder, key, "");
    if (rc != YVEX_OK) return rc;
    entry = &builder->map->metadata[builder->map->summary.metadata_count - 1u];
    entry->type = YVEX_DEEPSEEK_GGUF_METADATA_F64_ARRAY;
    for (i = 0u; i < count; ++i) entry->f64_array_values[i] = values[i];
    entry->array_count = count;
    return YVEX_OK;
}

/* Projects IR-owned architecture/tokenizer facts into future writer metadata. */
static int map_build_metadata(map_builder *builder)
{
    const yvex_deepseek_v4_model_spec *model =
        yvex_deepseek_v4_ir_model(builder->ir);
    const yvex_deepseek_v4_layer_spec *first =
        yvex_deepseek_v4_ir_layer_at(builder->ir, 0u);
    const yvex_deepseek_v4_layer_spec *first_csa =
        yvex_deepseek_v4_ir_layer_at(builder->ir, 2u);
    unsigned long long ratios[64];
    double clamp[64];
    unsigned long long i;
    int rc;

#define META_STR(k, v) do { rc = map_add_metadata_string(builder, k, v); if (rc != YVEX_OK) return rc; } while (0)
#define META_U64(k, v) do { rc = map_add_metadata_u64(builder, k, v); if (rc != YVEX_OK) return rc; } while (0)
#define META_BOOL(k, v) do { rc = map_add_metadata_bool(builder, k, v); if (rc != YVEX_OK) return rc; } while (0)
#define META_F64(k, v) do { rc = map_add_metadata_f64(builder, k, v); if (rc != YVEX_OK) return rc; } while (0)
    META_STR("general.architecture", "deepseek4");
    META_STR("general.name", "DeepSeek-V4-Flash");
    META_STR("general.source.huggingface.repository", model->repository);
    META_STR("yvex.source.revision", model->revision);
    META_U64("deepseek4.block_count", model->main_layer_count);
    META_U64("deepseek4.embedding_length", model->hidden_size);
    META_U64("deepseek4.context_length", model->maximum_context);
    META_U64("deepseek4.vocab_size", model->vocabulary_size);
    META_U64("deepseek4.attention.head_count", first->query_heads);
    META_U64("deepseek4.attention.head_count_kv", first->kv_heads);
    META_U64("deepseek4.attention.key_length", first->head_dimension);
    META_U64("deepseek4.attention.value_length", first->head_dimension);
    META_F64("deepseek4.attention.layer_norm_rms_epsilon",
             first->rms_norm_epsilon);
    META_U64("deepseek4.rope.dimension_count", first->rope_head_dimension);
    META_F64("deepseek4.rope.freq_base", (double)first->position.theta);
    META_U64("deepseek4.attention.q_lora_rank", first->query_lora_rank);
    META_U64("deepseek4.attention.output_lora_rank", first->output_lora_rank);
    META_U64("deepseek4.attention.output_group_count", first->output_groups);
    for (i = 0u; i < model->main_layer_count; ++i)
        ratios[i] = yvex_deepseek_v4_ir_layer_at(builder->ir, i)->compression_ratio;
    rc = map_add_metadata_array(builder, "deepseek4.attention.compress_ratios",
                                ratios, (unsigned int)model->main_layer_count);
    if (rc != YVEX_OK) return rc;
    META_U64("deepseek4.attention.sliding_window", first->kv.sliding_window);
    META_U64("deepseek4.expert_count", first->moe.routed_experts);
    META_U64("deepseek4.expert_used_count", first->moe.experts_per_token);
    META_U64("deepseek4.expert_shared_count", first->moe.shared_experts);
    META_U64("deepseek4.expert_feed_forward_length", first->moe.expert_intermediate_size);
    META_F64("deepseek4.expert_weights_scale", first->moe.routed_scaling_factor);
    META_BOOL("deepseek4.expert_weights_norm",
              first->moe.normalize_topk_probabilities);
    META_U64("deepseek4.expert_gating_func", 4u);
    for (i = 0u; i < model->main_layer_count; ++i)
        clamp[i] = yvex_deepseek_v4_ir_layer_at(builder->ir, i)->moe.activation_limit;
    rc = map_add_metadata_f64_array(builder, "deepseek4.swiglu_clamp_exp",
                                    clamp, (unsigned int)model->main_layer_count);
    if (rc != YVEX_OK) return rc;
    rc = map_add_metadata_f64_array(builder, "deepseek4.swiglu_clamp_shexp",
                                    clamp, (unsigned int)model->main_layer_count);
    if (rc != YVEX_OK) return rc;
    META_U64("deepseek4.hash_layer_count", model->hash_router_layer_count);
    META_F64("deepseek4.attention.compress_rope_freq_base",
             (double)first_csa->position.theta);
    META_U64("deepseek4.hyper_connection.count", first->mhc.residual_streams);
    META_U64("deepseek4.hyper_connection.sinkhorn_iterations", first->mhc.sinkhorn_iterations);
    META_F64("deepseek4.hyper_connection.epsilon", first->mhc.epsilon);
    META_U64("deepseek4.indexer.head_count", first_csa->indexer_heads);
    META_U64("deepseek4.indexer.key_length", first_csa->indexer_head_dimension);
    META_U64("deepseek4.indexer.top_k", first_csa->indexer_topk);
    META_STR("tokenizer.ggml.model", "gpt2");
    META_U64("tokenizer.ggml.vocab_size", model->tokenizer.vocabulary_size);
    META_U64("tokenizer.ggml.bos_token_id", model->tokenizer.bos_token_id);
    META_U64("tokenizer.ggml.eos_token_id", model->tokenizer.eos_token_id);
    META_BOOL("yvex.tokenizer.sidecars_verified", 1);
    META_U64("yvex.deepseek4.mtp.schema", YVEX_GGUF_MTP_EXTENSION_VERSION);
    META_U64("yvex.deepseek4.mtp.predictor_count", model->auxiliary_layer_count);
    META_U64("yvex.deepseek4.mtp.descriptor_count", YVEX_DEEPSEEK_GGUF_MTP_DESCRIPTOR_COUNT);
    META_BOOL("yvex.deepseek4.mtp.runtime_supported", 0);
    META_STR("yvex.deepseek4.mtp.name_prefix", "yvex.mtp.v1");
#undef META_F64
#undef META_BOOL
#undef META_U64
#undef META_STR
    return YVEX_OK;
}

/* Builds all global, trunk, and complete-source MTP descriptors. */
static int map_build_descriptors(map_builder *builder)
{
    const yvex_deepseek_v4_model_spec *model =
        yvex_deepseek_v4_ir_model(builder->ir);
    const yvex_tensor_role head_roles[3] = {
        YVEX_TENSOR_ROLE_HC_HEAD_FUNCTION,
        YVEX_TENSOR_ROLE_HC_HEAD_BASE,
        YVEX_TENSOR_ROLE_HC_HEAD_SCALE
    };
    const char *head_names[3] = {"hc_head_fn", "hc_head_base", "hc_head_scale"};
    unsigned long long layer;
    unsigned int i;
    int rc;

    rc = map_add_direct(builder, YVEX_TENSOR_ROLE_TOKEN_EMBEDDING,
                        YVEX_DEEPSEEK_TENSOR_COLLECTION_GLOBAL,
                        YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL,
                        YVEX_DEEPSEEK_GGUF_NO_INDEX,
                        YVEX_DEEPSEEK_GGUF_NO_INDEX, "embed.weight",
                        YVEX_DEEPSEEK_GGUF_TRANSFORM_DIRECT,
                        YVEX_NATIVE_DTYPE_BF16);
    if (rc != YVEX_OK) return rc;
    rc = map_add_direct(builder, YVEX_TENSOR_ROLE_OUTPUT_NORM,
                        YVEX_DEEPSEEK_TENSOR_COLLECTION_GLOBAL,
                        YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL,
                        YVEX_DEEPSEEK_GGUF_NO_INDEX,
                        YVEX_DEEPSEEK_GGUF_NO_INDEX, "norm.weight",
                        YVEX_DEEPSEEK_GGUF_TRANSFORM_DIRECT,
                        YVEX_NATIVE_DTYPE_BF16);
    if (rc != YVEX_OK) return rc;
    rc = map_add_direct(builder, YVEX_TENSOR_ROLE_OUTPUT_HEAD,
                        YVEX_DEEPSEEK_TENSOR_COLLECTION_GLOBAL,
                        YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL,
                        YVEX_DEEPSEEK_GGUF_NO_INDEX,
                        YVEX_DEEPSEEK_GGUF_NO_INDEX, "head.weight",
                        YVEX_DEEPSEEK_GGUF_TRANSFORM_DIRECT,
                        YVEX_NATIVE_DTYPE_BF16);
    if (rc != YVEX_OK) return rc;
    for (i = 0u; i < 3u; ++i) {
        rc = map_add_direct(builder, head_roles[i],
                            YVEX_DEEPSEEK_TENSOR_COLLECTION_GLOBAL,
                            YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL,
                            YVEX_DEEPSEEK_GGUF_NO_INDEX,
                            YVEX_DEEPSEEK_GGUF_NO_INDEX, head_names[i],
                            YVEX_DEEPSEEK_GGUF_TRANSFORM_DIRECT,
                            YVEX_NATIVE_DTYPE_F32);
        if (rc != YVEX_OK) return rc;
    }
    for (layer = 0u; layer < model->main_layer_count; ++layer) {
        char prefix[64];
        (void)snprintf(prefix, sizeof(prefix), "layers.%llu", layer);
        rc = map_add_layer(builder, prefix,
                           yvex_deepseek_v4_ir_layer_at(builder->ir, layer),
                           YVEX_DEEPSEEK_TENSOR_SCOPE_MAIN_LAYER,
                           YVEX_DEEPSEEK_GGUF_NO_INDEX);
        if (rc != YVEX_OK) return rc;
    }
    for (layer = 0u; layer < model->auxiliary_layer_count; ++layer) {
        const yvex_deepseek_v4_auxiliary_spec *aux =
            yvex_deepseek_v4_ir_auxiliary_at(builder->ir, layer);
        char prefix[64];
        char name[128];
        char base[128];
        (void)snprintf(prefix, sizeof(prefix), "mtp.%llu", layer);
        rc = map_add_layer(builder, prefix, &aux->layer,
                           YVEX_DEEPSEEK_TENSOR_SCOPE_MTP, layer);
        if (rc != YVEX_OK) return rc;
        (void)snprintf(base, sizeof(base), "%s.e_proj", prefix);
        rc = map_add_fp8(builder, YVEX_TENSOR_ROLE_MTP_EMBEDDING_PROJECTION,
                         YVEX_DEEPSEEK_TENSOR_COLLECTION_AUXILIARY,
                         YVEX_DEEPSEEK_TENSOR_SCOPE_MTP,
                         aux->layer.layer_index, layer, base);
        if (rc != YVEX_OK) return rc;
        (void)snprintf(base, sizeof(base), "%s.h_proj", prefix);
        rc = map_add_fp8(builder, YVEX_TENSOR_ROLE_MTP_HIDDEN_PROJECTION,
                         YVEX_DEEPSEEK_TENSOR_COLLECTION_AUXILIARY,
                         YVEX_DEEPSEEK_TENSOR_SCOPE_MTP,
                         aux->layer.layer_index, layer, base);
        if (rc != YVEX_OK) return rc;
#define MTP_DIRECT(role_id, suffix)                                             \
        do {                                                                    \
            (void)snprintf(name, sizeof(name), "%s.%s", prefix, suffix);     \
            rc = map_add_direct(builder, role_id,                               \
                YVEX_DEEPSEEK_TENSOR_COLLECTION_AUXILIARY,                     \
                YVEX_DEEPSEEK_TENSOR_SCOPE_MTP, aux->layer.layer_index, layer, \
                name, YVEX_DEEPSEEK_GGUF_TRANSFORM_DIRECT,                     \
                YVEX_NATIVE_DTYPE_BF16);                                       \
            if (rc != YVEX_OK) return rc;                                       \
        } while (0)
        MTP_DIRECT(YVEX_TENSOR_ROLE_MTP_EMBEDDING_NORM, "enorm.weight");
        MTP_DIRECT(YVEX_TENSOR_ROLE_MTP_HIDDEN_NORM, "hnorm.weight");
        MTP_DIRECT(YVEX_TENSOR_ROLE_MTP_OUTPUT_NORM, "norm.weight");
#undef MTP_DIRECT
        for (i = 0u; i < 3u; ++i) {
            (void)snprintf(name, sizeof(name), "%s.hc_head_%s", prefix,
                           i == 0u ? "fn" : (i == 1u ? "base" : "scale"));
            rc = map_add_direct(builder, head_roles[i],
                YVEX_DEEPSEEK_TENSOR_COLLECTION_AUXILIARY,
                YVEX_DEEPSEEK_TENSOR_SCOPE_MTP, aux->layer.layer_index, layer,
                name, YVEX_DEEPSEEK_GGUF_TRANSFORM_DIRECT,
                YVEX_NATIVE_DTYPE_F32);
            if (rc != YVEX_OK) return rc;
        }
    }
    return YVEX_OK;
}

/* Verifies complete accounting and computes the stable plan identity. */
static int map_finalize(map_builder *builder)
{
    yvex_deepseek_gguf_map *map = builder->map;
    unsigned long long i;
    unsigned long long identity = 1469598103934665603ull;
    unsigned long long trunk_collections[YVEX_DEEPSEEK_TENSOR_COLLECTION_COUNT] = {0};

    if (map->summary.source_contribution_count != YVEX_DEEPSEEK_GGUF_SOURCE_COUNT ||
        map->summary.descriptor_count != YVEX_DEEPSEEK_GGUF_DESCRIPTOR_COUNT ||
        map->summary.trunk_descriptor_count != YVEX_DEEPSEEK_GGUF_TRUNK_DESCRIPTOR_COUNT ||
        map->summary.mtp_descriptor_count != YVEX_DEEPSEEK_GGUF_MTP_DESCRIPTOR_COUNT ||
        map->summary.pinned_standard_count != YVEX_DEEPSEEK_GGUF_TRUNK_DESCRIPTOR_COUNT ||
        map->summary.extension_count != YVEX_DEEPSEEK_GGUF_MTP_DESCRIPTOR_COUNT) {
        return map_reject(builder, YVEX_DEEPSEEK_GGUF_MAP_FAILURE_ACCOUNTING,
                          YVEX_TENSOR_ROLE_UNKNOWN,
                          YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL,
                          YVEX_DEEPSEEK_GGUF_NO_INDEX,
                          YVEX_DEEPSEEK_GGUF_NO_INDEX,
                          YVEX_DEEPSEEK_GGUF_NO_INDEX, NULL, NULL,
                          YVEX_DEEPSEEK_GGUF_DESCRIPTOR_COUNT,
                          map->summary.descriptor_count);
    }
    for (i = 0u; i < YVEX_DEEPSEEK_GGUF_SOURCE_COUNT; ++i) {
        if (!map->source_consumed[i])
            return map_reject(builder,
                YVEX_DEEPSEEK_GGUF_MAP_FAILURE_MISSING_SOURCE,
                YVEX_TENSOR_ROLE_UNKNOWN, YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL,
                YVEX_DEEPSEEK_GGUF_NO_INDEX, YVEX_DEEPSEEK_GGUF_NO_INDEX,
                YVEX_DEEPSEEK_GGUF_NO_INDEX, NULL, NULL, 1u, 0u);
    }
    for (i = 0u; i < map->summary.descriptor_count; ++i) {
        const yvex_deepseek_gguf_descriptor *descriptor = &map->descriptors[i];
        if (descriptor->scope != YVEX_DEEPSEEK_TENSOR_SCOPE_MTP)
            trunk_collections[descriptor->collection]++;
        identity = map_hash_u64(identity, descriptor->identity);
    }
    if (trunk_collections[YVEX_DEEPSEEK_TENSOR_COLLECTION_GLOBAL] != 6u ||
        trunk_collections[YVEX_DEEPSEEK_TENSOR_COLLECTION_ATTENTION] != 344u ||
        trunk_collections[YVEX_DEEPSEEK_TENSOR_COLLECTION_MHC] != 258u ||
        trunk_collections[YVEX_DEEPSEEK_TENSOR_COLLECTION_NORM] != 86u ||
        trunk_collections[YVEX_DEEPSEEK_TENSOR_COLLECTION_ROUTED_EXPERT] != 129u ||
        trunk_collections[YVEX_DEEPSEEK_TENSOR_COLLECTION_SHARED_EXPERT] != 129u ||
        trunk_collections[YVEX_DEEPSEEK_TENSOR_COLLECTION_ROUTER] != 86u ||
        trunk_collections[YVEX_DEEPSEEK_TENSOR_COLLECTION_COMPRESSOR] != 164u ||
        trunk_collections[YVEX_DEEPSEEK_TENSOR_COLLECTION_INDEXER] != 126u) {
        return map_reject(builder, YVEX_DEEPSEEK_GGUF_MAP_FAILURE_ACCOUNTING,
                          YVEX_TENSOR_ROLE_UNKNOWN,
                          YVEX_DEEPSEEK_TENSOR_SCOPE_MAIN_LAYER,
                          YVEX_DEEPSEEK_GGUF_NO_INDEX,
                          YVEX_DEEPSEEK_GGUF_NO_INDEX,
                          YVEX_DEEPSEEK_GGUF_NO_INDEX, NULL, NULL, 1328u, 0u);
    }
    identity = map_hash_u64(identity, map->summary.source_identity);
    identity = map_hash_u64(identity, map->summary.coverage_identity);
    map->summary.mapping_identity = identity;
    map->summary.complete = 1;
    return YVEX_OK;
}

int yvex_deepseek_gguf_map_build_with_allocator(
    yvex_deepseek_gguf_map **out, const yvex_deepseek_v4_ir *ir,
    const yvex_deepseek_tensor_coverage *coverage,
    const yvex_deepseek_gguf_map_allocator *allocator,
    yvex_deepseek_gguf_map_failure *failure, yvex_error *err)
{
    const yvex_deepseek_v4_model_spec *model;
    const yvex_deepseek_tensor_coverage_summary *coverage_summary;
    yvex_deepseek_gguf_map *map;
    map_builder builder;
    size_t bytes;
    int rc;

    if (out) *out = NULL;
    map_failure_clear(failure);
    yvex_error_clear(err);
    if (!out || !ir || !coverage || !allocator || !allocator->allocate ||
        !allocator->release) {
        memset(&builder, 0, sizeof(builder));
        builder.failure = failure;
        builder.err = err;
        return map_reject(&builder,
            YVEX_DEEPSEEK_GGUF_MAP_FAILURE_INVALID_ARGUMENT,
            YVEX_TENSOR_ROLE_UNKNOWN, YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL,
            YVEX_DEEPSEEK_GGUF_NO_INDEX, YVEX_DEEPSEEK_GGUF_NO_INDEX,
            YVEX_DEEPSEEK_GGUF_NO_INDEX, NULL, NULL, 1u, 0u);
    }
    model = yvex_deepseek_v4_ir_model(ir);
    coverage_summary = yvex_deepseek_tensor_coverage_summary_get(coverage);
    if (!model || model->main_layer_count != 43u ||
        model->auxiliary_layer_count != 1u) {
        memset(&builder, 0, sizeof(builder));
        builder.failure = failure;
        builder.err = err;
        return map_reject(&builder, YVEX_DEEPSEEK_GGUF_MAP_FAILURE_ARCHITECTURE,
                          YVEX_TENSOR_ROLE_UNKNOWN,
                          YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL,
                          YVEX_DEEPSEEK_GGUF_NO_INDEX,
                          YVEX_DEEPSEEK_GGUF_NO_INDEX,
                          YVEX_DEEPSEEK_GGUF_NO_INDEX, NULL, NULL, 44u,
                          model ? model->main_layer_count + model->auxiliary_layer_count : 0u);
    }
    if (!coverage_summary || !coverage_summary->complete ||
        coverage_summary->source_tensor_count != YVEX_DEEPSEEK_GGUF_SOURCE_COUNT ||
        coverage_summary->matched_tensor_count != YVEX_DEEPSEEK_GGUF_SOURCE_COUNT ||
        coverage_summary->header_scan_count != 1u ||
        coverage_summary->payload_bytes_read != 0u) {
        memset(&builder, 0, sizeof(builder));
        builder.failure = failure;
        builder.err = err;
        return map_reject(&builder, YVEX_DEEPSEEK_GGUF_MAP_FAILURE_COVERAGE,
                          YVEX_TENSOR_ROLE_UNKNOWN,
                          YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL,
                          YVEX_DEEPSEEK_GGUF_NO_INDEX,
                          YVEX_DEEPSEEK_GGUF_NO_INDEX,
                          YVEX_DEEPSEEK_GGUF_NO_INDEX, NULL, NULL,
                          YVEX_DEEPSEEK_GGUF_SOURCE_COUNT,
                          coverage_summary ? coverage_summary->matched_tensor_count : 0u);
    }
    map = (yvex_deepseek_gguf_map *)allocator->allocate(sizeof(*map),
                                                        allocator->context);
    if (!map) {
        memset(&builder, 0, sizeof(builder));
        builder.failure = failure;
        builder.err = err;
        return map_reject(&builder, YVEX_DEEPSEEK_GGUF_MAP_FAILURE_ALLOCATION,
                          YVEX_TENSOR_ROLE_UNKNOWN,
                          YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL,
                          YVEX_DEEPSEEK_GGUF_NO_INDEX,
                          YVEX_DEEPSEEK_GGUF_NO_INDEX,
                          YVEX_DEEPSEEK_GGUF_NO_INDEX, "map", NULL,
                          sizeof(*map), 0u);
    }
    memset(map, 0, sizeof(*map));
    map->allocator = *allocator;
    map->source_index_capacity = map_index_capacity(YVEX_DEEPSEEK_GGUF_SOURCE_COUNT);
    map->emitted_index_capacity = map_index_capacity(YVEX_DEEPSEEK_GGUF_DESCRIPTOR_COUNT);
    map->role_index_capacity = map_index_capacity(YVEX_DEEPSEEK_GGUF_DESCRIPTOR_COUNT);
    map->descriptors = (yvex_deepseek_gguf_descriptor *)map_allocate_zero(
        map, (size_t)YVEX_DEEPSEEK_GGUF_DESCRIPTOR_COUNT * sizeof(*map->descriptors));
    map->contributions = (yvex_deepseek_gguf_contribution *)map_allocate_zero(
        map, (size_t)YVEX_DEEPSEEK_GGUF_SOURCE_COUNT * sizeof(*map->contributions));
    map->source_consumed = (unsigned char *)map_allocate_zero(
        map, (size_t)YVEX_DEEPSEEK_GGUF_SOURCE_COUNT);
    bytes = (size_t)map->source_index_capacity * sizeof(*map->source_index);
    map->source_index = (map_index_slot *)map_allocate_zero(map, bytes);
    bytes = (size_t)map->emitted_index_capacity * sizeof(*map->emitted_index);
    map->emitted_index = (map_index_slot *)map_allocate_zero(map, bytes);
    bytes = (size_t)map->role_index_capacity * sizeof(*map->role_index);
    map->role_index = (map_index_slot *)map_allocate_zero(map, bytes);
    memset(&builder, 0, sizeof(builder));
    builder.map = map;
    builder.ir = ir;
    builder.coverage = coverage;
    builder.failure = failure;
    builder.err = err;
    if (!map->descriptors || !map->contributions || !map->source_consumed ||
        !map->source_index || !map->emitted_index || !map->role_index) {
        rc = map_reject(&builder, YVEX_DEEPSEEK_GGUF_MAP_FAILURE_ALLOCATION,
                        YVEX_TENSOR_ROLE_UNKNOWN,
                        YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL,
                        YVEX_DEEPSEEK_GGUF_NO_INDEX,
                        YVEX_DEEPSEEK_GGUF_NO_INDEX,
                        YVEX_DEEPSEEK_GGUF_NO_INDEX, "mapping-tables", NULL,
                        1u, 0u);
        yvex_deepseek_gguf_map_close(map);
        return rc;
    }
    map->summary.header_scan_count = coverage_summary->header_scan_count;
    map->summary.payload_bytes_read = coverage_summary->payload_bytes_read;
    map->summary.source_identity = coverage_summary->source_identity;
    map->summary.coverage_identity = coverage_summary->coverage_identity;
    rc = map_build_descriptors(&builder);
    if (rc == YVEX_OK) rc = map_build_metadata(&builder);
    if (rc == YVEX_OK) rc = map_finalize(&builder);
    if (rc != YVEX_OK) {
        yvex_deepseek_gguf_map_close(map);
        return rc;
    }
    *out = map;
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_deepseek_gguf_map_build(
    yvex_deepseek_gguf_map **out, const yvex_deepseek_v4_ir *ir,
    const yvex_deepseek_tensor_coverage *coverage,
    yvex_deepseek_gguf_map_failure *failure, yvex_error *err)
{
    yvex_deepseek_gguf_map_allocator allocator;
    allocator.allocate = map_default_allocate;
    allocator.release = map_default_release;
    allocator.context = NULL;
    return yvex_deepseek_gguf_map_build_with_allocator(
        out, ir, coverage, &allocator, failure, err);
}

/*
 * Coordinates one strict source pass, then consumes its immutable snapshot
 * through IR, coverage, and mapping. Source parsing remains in src/source;
 * only the complete map and optional coverage survive return.
 */
int yvex_deepseek_gguf_map_open_verified_source(
    yvex_deepseek_gguf_map **out,
    yvex_deepseek_tensor_coverage **coverage_out,
    yvex_source_verification *verification,
    const char *source_path,
    const char *models_root,
    yvex_deepseek_gguf_map_failure *failure,
    yvex_error *err)
{
    yvex_source_verify_options source_options;
    yvex_source_tensor_snapshot *snapshot = NULL;
    yvex_deepseek_v4_ir *ir = NULL;
    yvex_deepseek_tensor_coverage *coverage = NULL;
    yvex_deepseek_v4_ir_failure ir_failure;
    yvex_deepseek_tensor_coverage_failure coverage_failure;
    int rc;

    if (out) *out = NULL;
    if (coverage_out) *coverage_out = NULL;
    map_failure_clear(failure);
    yvex_error_clear(err);
    if (!out || !verification || !source_path || !source_path[0]) {
        map_builder builder;
        memset(&builder, 0, sizeof(builder));
        builder.failure = failure;
        builder.err = err;
        return map_reject(&builder,
                          YVEX_DEEPSEEK_GGUF_MAP_FAILURE_INVALID_ARGUMENT,
                          YVEX_TENSOR_ROLE_UNKNOWN,
                          YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL,
                          YVEX_DEEPSEEK_GGUF_NO_INDEX,
                          YVEX_DEEPSEEK_GGUF_NO_INDEX,
                          YVEX_DEEPSEEK_GGUF_NO_INDEX,
                          "verified-source-path", NULL, 1u, 0u);
    }
    memset(&source_options, 0, sizeof(source_options));
    source_options.identity = yvex_model_target_release_identity();
    source_options.source_path = source_path;
    source_options.models_root = models_root && models_root[0]
                                     ? models_root : "models";
    rc = yvex_source_verify_with_snapshot(&source_options, verification,
                                          &snapshot, err);
    if (rc != YVEX_OK) {
        map_builder builder;
        memset(&builder, 0, sizeof(builder));
        builder.failure = failure;
        builder.err = err;
        rc = map_reject(&builder,
                        YVEX_DEEPSEEK_GGUF_MAP_FAILURE_SOURCE_VERIFICATION,
                        YVEX_TENSOR_ROLE_UNKNOWN,
                        YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL,
                        YVEX_DEEPSEEK_GGUF_NO_INDEX,
                        YVEX_DEEPSEEK_GGUF_NO_INDEX,
                        YVEX_DEEPSEEK_GGUF_NO_INDEX,
                        source_path, NULL, 1u, 0u);
        goto cleanup;
    }
    if (!verification->verified || !snapshot) {
        map_builder builder;
        memset(&builder, 0, sizeof(builder));
        builder.failure = failure;
        builder.err = err;
        rc = map_reject(&builder,
                        YVEX_DEEPSEEK_GGUF_MAP_FAILURE_SOURCE_VERIFICATION,
                        YVEX_TENSOR_ROLE_UNKNOWN,
                        YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL,
                        YVEX_DEEPSEEK_GGUF_NO_INDEX,
                        YVEX_DEEPSEEK_GGUF_NO_INDEX,
                        YVEX_DEEPSEEK_GGUF_NO_INDEX,
                        "strict-source-verification", NULL, 1u, 0u);
        goto cleanup;
    }
    rc = yvex_deepseek_v4_ir_build(&ir, verification, &ir_failure, err);
    if (rc != YVEX_OK) {
        map_builder builder;
        memset(&builder, 0, sizeof(builder));
        builder.failure = failure;
        builder.err = err;
        rc = map_reject(&builder,
                        YVEX_DEEPSEEK_GGUF_MAP_FAILURE_ARCHITECTURE,
                        YVEX_TENSOR_ROLE_UNKNOWN,
                        YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL,
                        ir_failure.layer_index,
                        YVEX_DEEPSEEK_GGUF_NO_INDEX,
                        YVEX_DEEPSEEK_GGUF_NO_INDEX,
                        ir_failure.field, NULL,
                        ir_failure.expected, ir_failure.actual);
        goto cleanup;
    }
    rc = yvex_deepseek_tensor_coverage_build(
        &coverage, verification, ir, snapshot, NULL, &coverage_failure, err);
    if (rc != YVEX_OK) {
        map_builder builder;
        memset(&builder, 0, sizeof(builder));
        builder.failure = failure;
        builder.err = err;
        rc = map_reject(&builder, YVEX_DEEPSEEK_GGUF_MAP_FAILURE_COVERAGE,
                        YVEX_TENSOR_ROLE_UNKNOWN,
                        YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL,
                        coverage_failure.layer_index,
                        YVEX_DEEPSEEK_GGUF_NO_INDEX,
                        coverage_failure.expert_index,
                        coverage_failure.tensor_name, NULL,
                        coverage_failure.expected, coverage_failure.actual);
        goto cleanup;
    }
    rc = yvex_deepseek_gguf_map_build(out, ir, coverage, failure, err);
    if (rc != YVEX_OK) goto cleanup;
    if (coverage_out) {
        *coverage_out = coverage;
        coverage = NULL;
    }
cleanup:
    yvex_deepseek_tensor_coverage_close(coverage);
    yvex_deepseek_v4_ir_close(ir);
    yvex_source_tensor_snapshot_release(snapshot);
    return rc;
}

void yvex_deepseek_gguf_map_close(yvex_deepseek_gguf_map *map)
{
    yvex_deepseek_gguf_map_allocator allocator;
    if (!map) return;
    allocator = map->allocator;
    if (map->role_index) allocator.release(map->role_index, allocator.context);
    if (map->emitted_index) allocator.release(map->emitted_index, allocator.context);
    if (map->source_index) allocator.release(map->source_index, allocator.context);
    if (map->source_consumed) allocator.release(map->source_consumed, allocator.context);
    if (map->contributions) allocator.release(map->contributions, allocator.context);
    if (map->descriptors) allocator.release(map->descriptors, allocator.context);
    allocator.release(map, allocator.context);
}

const yvex_deepseek_gguf_map_summary *yvex_deepseek_gguf_map_summary_get(
    const yvex_deepseek_gguf_map *map)
{
    return map ? &map->summary : NULL;
}

const yvex_deepseek_gguf_descriptor *yvex_deepseek_gguf_map_at(
    const yvex_deepseek_gguf_map *map, unsigned long long index)
{
    return map && index < map->summary.descriptor_count
               ? &map->descriptors[index] : NULL;
}

const yvex_deepseek_gguf_contribution *yvex_deepseek_gguf_map_contribution_at(
    const yvex_deepseek_gguf_map *map, unsigned long long index)
{
    return map && index < map->summary.source_contribution_count
               ? &map->contributions[index] : NULL;
}

static const yvex_deepseek_gguf_descriptor *map_find_name(
    const yvex_deepseek_gguf_map *map, const char *name, int emitted)
{
    const map_index_slot *slots;
    unsigned long long capacity;
    unsigned long long hash;
    unsigned long long slot;
    unsigned long long probes;

    if (!map || !name) return NULL;
    slots = emitted ? map->emitted_index : map->source_index;
    capacity = emitted ? map->emitted_index_capacity : map->source_index_capacity;
    hash = map_hash_string(name);
    slot = hash & (capacity - 1u);
    for (probes = 0u; probes < capacity && slots[slot].value_plus_one; ++probes) {
        if (slots[slot].hash == hash) {
            unsigned long long value = slots[slot].value_plus_one - 1u;
            if (emitted) {
                if (strcmp(map->descriptors[value].emitted_name, name) == 0)
                    return &map->descriptors[value];
            } else if (strcmp(map->contributions[value].source_name, name) == 0) {
                return &map->descriptors[map->contributions[value].descriptor_index];
            }
        }
        slot = (slot + 1u) & (capacity - 1u);
    }
    return NULL;
}

const yvex_deepseek_gguf_descriptor *yvex_deepseek_gguf_map_find_source(
    const yvex_deepseek_gguf_map *map, const char *source_name)
{
    return map_find_name(map, source_name, 0);
}

const yvex_deepseek_gguf_descriptor *yvex_deepseek_gguf_map_find_emitted(
    const yvex_deepseek_gguf_map *map, const char *emitted_name)
{
    return map_find_name(map, emitted_name, 1);
}

const yvex_deepseek_gguf_descriptor *yvex_deepseek_gguf_map_find_role(
    const yvex_deepseek_gguf_map *map, yvex_tensor_role role,
    yvex_deepseek_tensor_scope scope, unsigned long long layer_index,
    unsigned long long predictor_index)
{
    unsigned long long hash = 1469598103934665603ull;
    unsigned long long slot;
    unsigned long long probes;
    if (!map) return NULL;
    hash = map_hash_u64(hash, role);
    hash = map_hash_u64(hash, scope);
    hash = map_hash_u64(hash, layer_index);
    hash = map_hash_u64(hash, predictor_index);
    slot = hash & (map->role_index_capacity - 1u);
    for (probes = 0u; probes < map->role_index_capacity &&
         map->role_index[slot].value_plus_one; ++probes) {
        if (map->role_index[slot].hash == hash) {
            const yvex_deepseek_gguf_descriptor *descriptor =
                &map->descriptors[map->role_index[slot].value_plus_one - 1u];
            if (descriptor->role == role && descriptor->scope == scope &&
                descriptor->layer_index == layer_index &&
                descriptor->predictor_index == predictor_index)
                return descriptor;
        }
        slot = (slot + 1u) & (map->role_index_capacity - 1u);
    }
    return NULL;
}

const yvex_deepseek_gguf_metadata *yvex_deepseek_gguf_map_metadata_at(
    const yvex_deepseek_gguf_map *map, unsigned long long index)
{
    return map && index < map->summary.metadata_count ? &map->metadata[index] : NULL;
}

const yvex_deepseek_gguf_metadata *yvex_deepseek_gguf_map_metadata_find(
    const yvex_deepseek_gguf_map *map, const char *key)
{
    unsigned long long i;
    if (!map || !key) return NULL;
    for (i = 0u; i < map->summary.metadata_count; ++i)
        if (strcmp(map->metadata[i].key, key) == 0) return &map->metadata[i];
    return NULL;
}

const char *yvex_deepseek_gguf_transform_name(
    yvex_deepseek_gguf_transform transform)
{
    static const char *names[] = {
        "direct", "fp8-e4m3-e8m0-pair", "expert-mxfp4-repack",
        "i64-to-i32"
    };
    return transform <= YVEX_DEEPSEEK_GGUF_TRANSFORM_I64_TO_I32
               ? names[transform] : "unknown";
}

const char *yvex_deepseek_gguf_map_failure_name(
    yvex_deepseek_gguf_map_failure_code code)
{
    static const char *names[] = {
        "none", "invalid-argument", "source-verification-refused",
        "architecture-incomplete", "coverage-incomplete",
        "coverage-row-mismatch", "missing-source", "duplicate-source",
        "source-dtype-mismatch", "expert-sequence-mismatch", "name-refused",
        "duplicate-name",
        "layout-refused", "metadata-refused", "accounting-mismatch",
        "arithmetic-overflow", "allocation-failure"
    };
    return code <= YVEX_DEEPSEEK_GGUF_MAP_FAILURE_ALLOCATION
               ? names[code] : "unknown";
}
