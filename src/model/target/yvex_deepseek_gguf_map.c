/*
 * yvex_deepseek_gguf_map.c - DeepSeek Transformation IR to GGUF lowering.
 *
 * Owner:
 *   src/model/target
 *
 * Owns:
 *   deterministic GGUF names, qtype/layout projection, metadata, contribution
 *   projection, mapping identity, indexes, and typed lowering refusal.
 *
 * Does not own:
 *   transformation semantics, source parsing or IO, payload reads, physical
 *   precision selection, numeric conversion, writer bytes, runtime, or generation.
 *
 * Invariants:
 *   one sealed artifact-neutral terminal becomes one GGUF descriptor; all IR
 *   source inputs are projected exactly once and mapping identity is preserved.
 *
 * Boundary:
 *   this immutable physical-format blueprint is not an emitted artifact.
 */
#include "yvex_deepseek_gguf_map.h"

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
    const yvex_deepseek_v4_ir *architecture;
    const yvex_transform_ir *transform_ir;
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
                                         const void *data,
                                         size_t size)
{
    const unsigned char *bytes = (const unsigned char *)data;
    size_t index;

    for (index = 0u; index < size; ++index) {
        hash ^= bytes[index];
        hash *= 1099511628211ull;
    }
    return hash;
}

static unsigned long long map_hash_string(const char *text)
{
    return map_hash_bytes(1469598103934665603ull, text, strlen(text) + 1u);
}

/* Retains the versioned legacy mapping encoding for identity continuity. */
static unsigned long long map_hash_u64(unsigned long long hash,
                                       unsigned long long value)
{
    return map_hash_bytes(hash, &value, sizeof(value));
}

static void map_failure_clear(yvex_deepseek_gguf_map_failure *failure)
{
    if (!failure) return;
    memset(failure, 0, sizeof(*failure));
    failure->layer_index = YVEX_DEEPSEEK_GGUF_NO_INDEX;
    failure->predictor_index = YVEX_DEEPSEEK_GGUF_NO_INDEX;
    failure->expert_index = YVEX_DEEPSEEK_GGUF_NO_INDEX;
}

/* Records one format-lowering refusal without publishing a partial map. */
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
            ? YVEX_ERR_INVALID_ARG : YVEX_ERR_FORMAT);
    yvex_deepseek_gguf_map_failure *failure =
        builder ? builder->failure : NULL;

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
                    "deepseek_gguf_lowering",
                    "%s role=%s source=%s emitted=%s layer=%llu expert=%llu expected=%llu actual=%llu",
                    yvex_deepseek_gguf_map_failure_name(code),
                    yvex_tensor_role_name(role),
                    source_name ? source_name : "none",
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

static int map_index_insert(map_index_slot *slots,
                            unsigned long long capacity,
                            unsigned long long hash,
                            unsigned long long value)
{
    unsigned long long slot;
    unsigned long long probe;

    if (!slots || !capacity || (capacity & (capacity - 1u)) != 0u) return 0;
    slot = hash & (capacity - 1u);
    for (probe = 0u; probe < capacity; ++probe) {
        if (!slots[slot].value_plus_one) {
            slots[slot].hash = hash;
            slots[slot].value_plus_one = value + 1u;
            return 1;
        }
        slot = (slot + 1u) & (capacity - 1u);
    }
    return 0;
}

static int map_emitted_index_insert(yvex_deepseek_gguf_map *map,
                                    unsigned long long hash,
                                    unsigned long long value)
{
    unsigned long long slot = hash & (map->emitted_index_capacity - 1u);
    unsigned long long probe;

    for (probe = 0u; probe < map->emitted_index_capacity; ++probe) {
        map_index_slot *entry = &map->emitted_index[slot];
        if (!entry->value_plus_one) {
            entry->hash = hash;
            entry->value_plus_one = value + 1u;
            return 1;
        }
        if (entry->hash == hash &&
            strcmp(map->descriptors[entry->value_plus_one - 1u].emitted_name,
                   map->descriptors[value].emitted_name) == 0) return 0;
        slot = (slot + 1u) & (map->emitted_index_capacity - 1u);
    }
    return 0;
}

static int map_role_index_insert(yvex_deepseek_gguf_map *map,
                                 unsigned long long hash,
                                 unsigned long long value)
{
    const yvex_deepseek_gguf_descriptor *candidate = &map->descriptors[value];
    unsigned long long slot = hash & (map->role_index_capacity - 1u);
    unsigned long long probe;

    for (probe = 0u; probe < map->role_index_capacity; ++probe) {
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
                current->predictor_index == candidate->predictor_index)
                return 0;
        }
        slot = (slot + 1u) & (map->role_index_capacity - 1u);
    }
    return 0;
}

static yvex_deepseek_tensor_scope map_scope(yvex_transform_scope scope)
{
    if (scope == YVEX_TRANSFORM_SCOPE_MAIN_LAYER)
        return YVEX_DEEPSEEK_TENSOR_SCOPE_MAIN_LAYER;
    if (scope == YVEX_TRANSFORM_SCOPE_AUXILIARY)
        return YVEX_DEEPSEEK_TENSOR_SCOPE_MTP;
    return YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL;
}

static yvex_deepseek_tensor_collection map_collection(
    yvex_transform_subsystem subsystem)
{
    switch (subsystem) {
    case YVEX_TRANSFORM_SUBSYSTEM_GLOBAL:
    case YVEX_TRANSFORM_SUBSYSTEM_OUTPUT:
        return YVEX_DEEPSEEK_TENSOR_COLLECTION_GLOBAL;
    case YVEX_TRANSFORM_SUBSYSTEM_ATTENTION:
        return YVEX_DEEPSEEK_TENSOR_COLLECTION_ATTENTION;
    case YVEX_TRANSFORM_SUBSYSTEM_COMPRESSOR:
        return YVEX_DEEPSEEK_TENSOR_COLLECTION_COMPRESSOR;
    case YVEX_TRANSFORM_SUBSYSTEM_INDEXER:
        return YVEX_DEEPSEEK_TENSOR_COLLECTION_INDEXER;
    case YVEX_TRANSFORM_SUBSYSTEM_NORMALIZATION:
        return YVEX_DEEPSEEK_TENSOR_COLLECTION_NORM;
    case YVEX_TRANSFORM_SUBSYSTEM_RESIDUAL:
        return YVEX_DEEPSEEK_TENSOR_COLLECTION_MHC;
    case YVEX_TRANSFORM_SUBSYSTEM_ROUTER:
        return YVEX_DEEPSEEK_TENSOR_COLLECTION_ROUTER;
    case YVEX_TRANSFORM_SUBSYSTEM_ROUTED_EXPERT:
        return YVEX_DEEPSEEK_TENSOR_COLLECTION_ROUTED_EXPERT;
    case YVEX_TRANSFORM_SUBSYSTEM_SHARED_EXPERT:
        return YVEX_DEEPSEEK_TENSOR_COLLECTION_SHARED_EXPERT;
    case YVEX_TRANSFORM_SUBSYSTEM_AUXILIARY:
        return YVEX_DEEPSEEK_TENSOR_COLLECTION_AUXILIARY;
    default:
        return YVEX_DEEPSEEK_TENSOR_COLLECTION_COUNT;
    }
}

static int map_transform(const yvex_transform_node *node,
                         yvex_deepseek_gguf_transform *transform,
                         unsigned int *qtype)
{
    if (!node || !transform || !qtype) return 0;
    *qtype = YVEX_GGUF_NO_FORCED_QTYPE;
    switch (node->kind) {
    case YVEX_TRANSFORM_OP_IDENTITY:
        *transform = YVEX_DEEPSEEK_GGUF_TRANSFORM_DIRECT;
        return 1;
    case YVEX_TRANSFORM_OP_DECODE_SCALE_PAIR:
        *transform = YVEX_DEEPSEEK_GGUF_TRANSFORM_FP8_E4M3_E8M0;
        return 1;
    case YVEX_TRANSFORM_OP_EXPERT_AGGREGATE:
        *transform = YVEX_DEEPSEEK_GGUF_TRANSFORM_EXPERT_MXFP4;
        *qtype = 39u;
        return 1;
    case YVEX_TRANSFORM_OP_CHECKED_CAST:
        *transform = YVEX_DEEPSEEK_GGUF_TRANSFORM_I64_TO_I32;
        *qtype = 26u;
        return 1;
    default:
        return 0;
    }
}

static yvex_deepseek_gguf_contribution_kind map_contribution_kind(
    yvex_deepseek_gguf_transform transform,
    unsigned long long input)
{
    if (transform == YVEX_DEEPSEEK_GGUF_TRANSFORM_I64_TO_I32)
        return YVEX_DEEPSEEK_GGUF_CONTRIBUTION_ROUTING_TABLE;
    if (transform == YVEX_DEEPSEEK_GGUF_TRANSFORM_FP8_E4M3_E8M0)
        return input ? YVEX_DEEPSEEK_GGUF_CONTRIBUTION_SCALE
                     : YVEX_DEEPSEEK_GGUF_CONTRIBUTION_PRIMARY;
    if (transform == YVEX_DEEPSEEK_GGUF_TRANSFORM_EXPERT_MXFP4)
        return input & 1u
            ? YVEX_DEEPSEEK_GGUF_CONTRIBUTION_EXPERT_SCALE
            : YVEX_DEEPSEEK_GGUF_CONTRIBUTION_EXPERT_WEIGHT;
    return YVEX_DEEPSEEK_GGUF_CONTRIBUTION_PRIMARY;
}

/* Begins one GGUF descriptor from a terminal logical key and operation. */
static int map_descriptor_begin(map_builder *builder,
                                const yvex_transform_value *terminal,
                                const yvex_transform_node *node,
                                unsigned long long descriptor_index)
{
    yvex_deepseek_gguf_map *map = builder->map;
    yvex_deepseek_gguf_descriptor *descriptor =
        &map->descriptors[descriptor_index];
    yvex_gguf_name_provenance provenance;
    yvex_deepseek_tensor_scope scope = map_scope(terminal->logical_key.scope);
    yvex_deepseek_tensor_collection collection =
        map_collection(terminal->logical_key.subsystem);
    const char *reason = NULL;
    unsigned long long role_hash = 1469598103934665603ull;
    unsigned int qtype;
    unsigned int dimension;

    memset(descriptor, 0, sizeof(*descriptor));
    descriptor->role = terminal->logical_key.role;
    descriptor->collection = collection;
    descriptor->scope = scope;
    descriptor->layer_index = terminal->logical_key.layer_index;
    descriptor->predictor_index = terminal->logical_key.auxiliary_index;
    descriptor->expert_count = node->expert_count;
    if (collection >= YVEX_DEEPSEEK_TENSOR_COLLECTION_COUNT ||
        !map_transform(node, &descriptor->transform, &qtype) ||
        terminal->shape.rank > YVEX_TENSOR_MAX_DIMS) {
        return map_reject(
            builder, YVEX_DEEPSEEK_GGUF_MAP_FAILURE_LOWERING_DIVERGENCE,
            descriptor->role, scope, descriptor->layer_index,
            descriptor->predictor_index, YVEX_DEEPSEEK_GGUF_NO_INDEX,
            NULL, NULL, 1u, 0u);
    }
    descriptor->forced_qtype = qtype;
    descriptor->logical_rank = terminal->shape.rank;
    descriptor->contribution_offset = map->summary.source_contribution_count;
    for (dimension = 0u; dimension < terminal->shape.rank; ++dimension) {
        unsigned int source_axis = terminal->shape.rank - dimension - 1u;
        descriptor->logical_dims[dimension] =
            terminal->shape.dims[source_axis];
        descriptor->source_axis_for_logical[dimension] = source_axis;
    }
    if (node->kind == YVEX_TRANSFORM_OP_EXPERT_AGGREGATE) {
        descriptor->source_axis_for_logical[0] = 1u;
        descriptor->source_axis_for_logical[1] = 0u;
        descriptor->source_axis_for_logical[2] =
            YVEX_DEEPSEEK_GGUF_AGGREGATED_AXIS;
    }
    if (!yvex_gguf_name_map_resolve(
            descriptor->role, scope == YVEX_DEEPSEEK_TENSOR_SCOPE_MTP,
            descriptor->layer_index, descriptor->predictor_index,
            descriptor->emitted_name, sizeof(descriptor->emitted_name),
            &provenance, &reason)) {
        return map_reject(
            builder, YVEX_DEEPSEEK_GGUF_MAP_FAILURE_NAME,
            descriptor->role, scope, descriptor->layer_index,
            descriptor->predictor_index, YVEX_DEEPSEEK_GGUF_NO_INDEX,
            NULL, reason, 1u, 0u);
    }
    descriptor->name_provenance = provenance;
    if (!yvex_gguf_layout_map_shape_supported(
            descriptor->role, qtype, descriptor->logical_rank,
            descriptor->logical_dims, &reason)) {
        return map_reject(
            builder, YVEX_DEEPSEEK_GGUF_MAP_FAILURE_LAYOUT,
            descriptor->role, scope, descriptor->layer_index,
            descriptor->predictor_index, YVEX_DEEPSEEK_GGUF_NO_INDEX,
            NULL, descriptor->emitted_name, 1u, 0u);
    }
    if (!map_emitted_index_insert(
            map, map_hash_string(descriptor->emitted_name), descriptor_index)) {
        return map_reject(
            builder, YVEX_DEEPSEEK_GGUF_MAP_FAILURE_DUPLICATE_NAME,
            descriptor->role, scope, descriptor->layer_index,
            descriptor->predictor_index, YVEX_DEEPSEEK_GGUF_NO_INDEX,
            NULL, descriptor->emitted_name, 1u, 2u);
    }
    role_hash = map_hash_u64(role_hash, descriptor->role);
    role_hash = map_hash_u64(role_hash, descriptor->scope);
    role_hash = map_hash_u64(role_hash, descriptor->layer_index);
    role_hash = map_hash_u64(role_hash, descriptor->predictor_index);
    if (!map_role_index_insert(map, role_hash, descriptor_index)) {
        return map_reject(
            builder, YVEX_DEEPSEEK_GGUF_MAP_FAILURE_DUPLICATE_NAME,
            descriptor->role, scope, descriptor->layer_index,
            descriptor->predictor_index, YVEX_DEEPSEEK_GGUF_NO_INDEX,
            NULL, descriptor->emitted_name, 1u, 2u);
    }
    descriptor->identity = map_hash_string(descriptor->emitted_name);
    descriptor->identity = map_hash_u64(descriptor->identity,
                                        descriptor->transform);
    descriptor->identity = map_hash_u64(descriptor->identity, qtype);
    for (dimension = 0u; dimension < descriptor->logical_rank; ++dimension)
        descriptor->identity = map_hash_u64(
            descriptor->identity, descriptor->logical_dims[dimension]);
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
    return YVEX_OK;
}

/* Projects one IR source input without reclassifying its transformation role. */
static int map_descriptor_add_source(
    map_builder *builder,
    const yvex_transform_node *node,
    unsigned long long descriptor_index,
    unsigned long long input_index)
{
    yvex_deepseek_gguf_map *map = builder->map;
    yvex_deepseek_gguf_descriptor *descriptor =
        &map->descriptors[descriptor_index];
    const yvex_transform_value *value = yvex_transform_ir_node_input_at(
        builder->transform_ir, node, input_index);
    const yvex_transform_source_value *source;
    yvex_deepseek_gguf_contribution *contribution;
    unsigned long long index = map->summary.source_contribution_count;
    unsigned int dimension;

    if (!value || value->kind != YVEX_TRANSFORM_VALUE_SOURCE)
        return map_reject(
            builder, YVEX_DEEPSEEK_GGUF_MAP_FAILURE_LOWERING_DIVERGENCE,
            descriptor->role, descriptor->scope, descriptor->layer_index,
            descriptor->predictor_index, YVEX_DEEPSEEK_GGUF_NO_INDEX,
            NULL, descriptor->emitted_name, 1u, 0u);
    source = yvex_transform_ir_source_at(
        builder->transform_ir, value->source_index);
    if (!source || index >= YVEX_DEEPSEEK_GGUF_SOURCE_COUNT ||
        source->requirement_index >= YVEX_DEEPSEEK_GGUF_SOURCE_COUNT ||
        source->shape.rank > 2u || source->role_hint != descriptor->role ||
        map_scope(source->scope) != descriptor->scope ||
        map_collection(source->subsystem) != descriptor->collection) {
        return map_reject(
            builder, YVEX_DEEPSEEK_GGUF_MAP_FAILURE_COVERAGE_ROW,
            descriptor->role, descriptor->scope, descriptor->layer_index,
            descriptor->predictor_index,
            source ? source->expert_index : YVEX_DEEPSEEK_GGUF_NO_INDEX,
            source ? source->source_name : NULL, descriptor->emitted_name,
            1u, 0u);
    }
    contribution = &map->contributions[index];
    (void)snprintf(contribution->source_name,
                   sizeof(contribution->source_name), "%s",
                   source->source_name);
    contribution->source_dtype = source->source_dtype;
    contribution->source_rank = source->shape.rank;
    for (dimension = 0u; dimension < source->shape.rank; ++dimension)
        contribution->source_dims[dimension] = source->shape.dims[dimension];
    contribution->kind = map_contribution_kind(descriptor->transform,
                                               input_index);
    contribution->source_row_index = source->requirement_index;
    contribution->descriptor_index = descriptor_index;
    contribution->expert_index = source->expert_index;
    if (!map_index_insert(map->source_index, map->source_index_capacity,
                          map_hash_string(source->source_name), index)) {
        return map_reject(
            builder, YVEX_DEEPSEEK_GGUF_MAP_FAILURE_DUPLICATE_SOURCE,
            descriptor->role, descriptor->scope, descriptor->layer_index,
            descriptor->predictor_index, source->expert_index,
            source->source_name, descriptor->emitted_name, 1u, 2u);
    }
    descriptor->contribution_count++;
    descriptor->identity = map_hash_bytes(
        descriptor->identity, source->source_name,
        strlen(source->source_name) + 1u);
    map->summary.source_contribution_count++;
    return YVEX_OK;
}

/* Lowers every terminal in canonical ordinal order from the sealed IR. */
static int map_build_descriptors(map_builder *builder)
{
    const yvex_transform_ir_summary *summary =
        yvex_transform_ir_summary_get(builder->transform_ir);
    unsigned long long ordinal;

    if (!summary || !summary->complete ||
        summary->state != YVEX_TRANSFORM_IR_STATE_SEALED ||
        summary->source_value_count != YVEX_DEEPSEEK_GGUF_SOURCE_COUNT ||
        summary->terminal_count != YVEX_DEEPSEEK_GGUF_DESCRIPTOR_COUNT ||
        summary->edge_count != YVEX_DEEPSEEK_GGUF_SOURCE_COUNT ||
        summary->payload_bytes_read != 0u) {
        return map_reject(
            builder, YVEX_DEEPSEEK_GGUF_MAP_FAILURE_TRANSFORM_IR,
            YVEX_TENSOR_ROLE_UNKNOWN, YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL,
            YVEX_DEEPSEEK_GGUF_NO_INDEX, YVEX_DEEPSEEK_GGUF_NO_INDEX,
            YVEX_DEEPSEEK_GGUF_NO_INDEX, NULL, NULL,
            YVEX_DEEPSEEK_GGUF_DESCRIPTOR_COUNT,
            summary ? summary->terminal_count : 0u);
    }
    for (ordinal = 0u; ordinal < summary->terminal_count; ++ordinal) {
        const yvex_transform_value *terminal =
            yvex_transform_ir_terminal_at(builder->transform_ir, ordinal);
        const yvex_transform_node *node;
        unsigned long long input;
        int rc;

        if (!terminal || terminal->canonical_ordinal != ordinal ||
            terminal->producer_node_id >= summary->node_count) {
            return map_reject(
                builder, YVEX_DEEPSEEK_GGUF_MAP_FAILURE_TRANSFORM_IR,
                terminal ? terminal->logical_key.role : YVEX_TENSOR_ROLE_UNKNOWN,
                terminal ? map_scope(terminal->logical_key.scope)
                         : YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL,
                YVEX_DEEPSEEK_GGUF_NO_INDEX,
                YVEX_DEEPSEEK_GGUF_NO_INDEX,
                YVEX_DEEPSEEK_GGUF_NO_INDEX, NULL, NULL, ordinal,
                terminal ? terminal->canonical_ordinal : ULLONG_MAX);
        }
        node = yvex_transform_ir_node_at(
            builder->transform_ir, terminal->producer_node_id);
        if (!node || node->output_value_id != terminal->id) {
            return map_reject(
                builder, YVEX_DEEPSEEK_GGUF_MAP_FAILURE_TRANSFORM_IR,
                terminal->logical_key.role,
                map_scope(terminal->logical_key.scope),
                terminal->logical_key.layer_index,
                terminal->logical_key.auxiliary_index,
                YVEX_DEEPSEEK_GGUF_NO_INDEX, NULL, NULL, terminal->id,
                node ? node->output_value_id : ULLONG_MAX);
        }
        rc = map_descriptor_begin(builder, terminal, node, ordinal);
        if (rc != YVEX_OK) return rc;
        for (input = 0u; input < node->input_count; ++input) {
            rc = map_descriptor_add_source(builder, node, ordinal, input);
            if (rc != YVEX_OK) return rc;
        }
    }
    return YVEX_OK;
}

static int map_add_metadata_string(map_builder *builder,
                                   const char *key,
                                   const char *value)
{
    yvex_deepseek_gguf_map *map = builder->map;
    yvex_deepseek_gguf_metadata *entry;
    unsigned long long index;

    if (!key || !value || map->summary.metadata_count >= MAP_METADATA_CAP)
        return map_reject(
            builder, YVEX_DEEPSEEK_GGUF_MAP_FAILURE_METADATA,
            YVEX_TENSOR_ROLE_UNKNOWN, YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL,
            YVEX_DEEPSEEK_GGUF_NO_INDEX, YVEX_DEEPSEEK_GGUF_NO_INDEX,
            YVEX_DEEPSEEK_GGUF_NO_INDEX, key, NULL, MAP_METADATA_CAP,
            map->summary.metadata_count + 1u);
    for (index = 0u; index < map->summary.metadata_count; ++index)
        if (strcmp(map->metadata[index].key, key) == 0)
            return map_reject(
                builder, YVEX_DEEPSEEK_GGUF_MAP_FAILURE_METADATA,
                YVEX_TENSOR_ROLE_UNKNOWN, YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL,
                YVEX_DEEPSEEK_GGUF_NO_INDEX, YVEX_DEEPSEEK_GGUF_NO_INDEX,
                YVEX_DEEPSEEK_GGUF_NO_INDEX, key, NULL, 1u, 2u);
    entry = &map->metadata[map->summary.metadata_count++];
    (void)snprintf(entry->key, sizeof(entry->key), "%s", key);
    entry->type = YVEX_DEEPSEEK_GGUF_METADATA_STRING;
    (void)snprintf(entry->string_value, sizeof(entry->string_value), "%s",
                   value);
    return YVEX_OK;
}

static int map_add_metadata_u64(map_builder *builder,
                                const char *key,
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

static int map_add_metadata_bool(map_builder *builder,
                                 const char *key,
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

static int map_add_metadata_f64(map_builder *builder,
                                const char *key,
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

static int map_add_metadata_array(map_builder *builder,
                                  const char *key,
                                  const unsigned long long *values,
                                  unsigned int count)
{
    yvex_deepseek_gguf_metadata *entry;
    int rc;
    if (!values || !count || count > 64u)
        return map_reject(
            builder, YVEX_DEEPSEEK_GGUF_MAP_FAILURE_METADATA,
            YVEX_TENSOR_ROLE_UNKNOWN, YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL,
            YVEX_DEEPSEEK_GGUF_NO_INDEX, YVEX_DEEPSEEK_GGUF_NO_INDEX,
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

static int map_add_metadata_f64_array(map_builder *builder,
                                      const char *key,
                                      const double *values,
                                      unsigned int count)
{
    yvex_deepseek_gguf_metadata *entry;
    unsigned int index;
    int rc;
    if (!values || !count || count > 64u)
        return map_reject(
            builder, YVEX_DEEPSEEK_GGUF_MAP_FAILURE_METADATA,
            YVEX_TENSOR_ROLE_UNKNOWN, YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL,
            YVEX_DEEPSEEK_GGUF_NO_INDEX, YVEX_DEEPSEEK_GGUF_NO_INDEX,
            YVEX_DEEPSEEK_GGUF_NO_INDEX, key, NULL, 64u, count);
    rc = map_add_metadata_string(builder, key, "");
    if (rc != YVEX_OK) return rc;
    entry = &builder->map->metadata[builder->map->summary.metadata_count - 1u];
    entry->type = YVEX_DEEPSEEK_GGUF_METADATA_F64_ARRAY;
    for (index = 0u; index < count; ++index)
        entry->f64_array_values[index] = values[index];
    entry->array_count = count;
    return YVEX_OK;
}

/* Projects architecture/tokenizer facts; transformation truth is not involved. */
static int map_build_metadata(map_builder *builder)
{
    const yvex_deepseek_v4_model_spec *model =
        yvex_deepseek_v4_ir_model(builder->architecture);
    const yvex_deepseek_v4_layer_spec *first =
        yvex_deepseek_v4_ir_layer_at(builder->architecture, 0u);
    const yvex_deepseek_v4_layer_spec *first_csa =
        yvex_deepseek_v4_ir_layer_at(builder->architecture, 2u);
    unsigned long long ratios[64];
    double clamp[64];
    unsigned long long index;
    int rc;

#define META_STR(k, v) do { rc = map_add_metadata_string(builder, k, v);     \
    if (rc != YVEX_OK) return rc; } while (0)
#define META_U64(k, v) do { rc = map_add_metadata_u64(builder, k, v);         \
    if (rc != YVEX_OK) return rc; } while (0)
#define META_BOOL(k, v) do { rc = map_add_metadata_bool(builder, k, v);       \
    if (rc != YVEX_OK) return rc; } while (0)
#define META_F64(k, v) do { rc = map_add_metadata_f64(builder, k, v);         \
    if (rc != YVEX_OK) return rc; } while (0)
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
    for (index = 0u; index < model->main_layer_count; ++index)
        ratios[index] = yvex_deepseek_v4_ir_layer_at(
            builder->architecture, index)->compression_ratio;
    rc = map_add_metadata_array(builder,
                                "deepseek4.attention.compress_ratios",
                                ratios, (unsigned int)model->main_layer_count);
    if (rc != YVEX_OK) return rc;
    META_U64("deepseek4.attention.sliding_window", first->kv.sliding_window);
    META_U64("deepseek4.expert_count", first->moe.routed_experts);
    META_U64("deepseek4.expert_used_count", first->moe.experts_per_token);
    META_U64("deepseek4.expert_shared_count", first->moe.shared_experts);
    META_U64("deepseek4.expert_feed_forward_length",
             first->moe.expert_intermediate_size);
    META_F64("deepseek4.expert_weights_scale",
             first->moe.routed_scaling_factor);
    META_BOOL("deepseek4.expert_weights_norm",
              first->moe.normalize_topk_probabilities);
    META_U64("deepseek4.expert_gating_func", 4u);
    for (index = 0u; index < model->main_layer_count; ++index)
        clamp[index] = yvex_deepseek_v4_ir_layer_at(
            builder->architecture, index)->moe.activation_limit;
    rc = map_add_metadata_f64_array(builder,
                                    "deepseek4.swiglu_clamp_exp", clamp,
                                    (unsigned int)model->main_layer_count);
    if (rc != YVEX_OK) return rc;
    rc = map_add_metadata_f64_array(builder,
                                    "deepseek4.swiglu_clamp_shexp", clamp,
                                    (unsigned int)model->main_layer_count);
    if (rc != YVEX_OK) return rc;
    META_U64("deepseek4.hash_layer_count", model->hash_router_layer_count);
    META_F64("deepseek4.attention.compress_rope_freq_base",
             (double)first_csa->position.theta);
    META_U64("deepseek4.hyper_connection.count", first->mhc.residual_streams);
    META_U64("deepseek4.hyper_connection.sinkhorn_iterations",
             first->mhc.sinkhorn_iterations);
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
    META_U64("yvex.deepseek4.mtp.predictor_count",
             model->auxiliary_layer_count);
    META_U64("yvex.deepseek4.mtp.descriptor_count",
             YVEX_DEEPSEEK_GGUF_MTP_DESCRIPTOR_COUNT);
    META_BOOL("yvex.deepseek4.mtp.runtime_supported", 0);
    META_STR("yvex.deepseek4.mtp.name_prefix", "yvex.mtp.v1");
#undef META_F64
#undef META_BOOL
#undef META_U64
#undef META_STR
    return YVEX_OK;
}

/* Verifies exhaustive projection and the pre-cutover mapping identity. */
static int map_finalize(map_builder *builder)
{
    yvex_deepseek_gguf_map *map = builder->map;
    unsigned long long trunk[YVEX_DEEPSEEK_TENSOR_COLLECTION_COUNT] = {0};
    unsigned long long identity = 1469598103934665603ull;
    unsigned long long index;

    if (map->summary.source_contribution_count !=
            YVEX_DEEPSEEK_GGUF_SOURCE_COUNT ||
        map->summary.descriptor_count !=
            YVEX_DEEPSEEK_GGUF_DESCRIPTOR_COUNT ||
        map->summary.trunk_descriptor_count !=
            YVEX_DEEPSEEK_GGUF_TRUNK_DESCRIPTOR_COUNT ||
        map->summary.mtp_descriptor_count !=
            YVEX_DEEPSEEK_GGUF_MTP_DESCRIPTOR_COUNT ||
        map->summary.pinned_standard_count !=
            YVEX_DEEPSEEK_GGUF_TRUNK_DESCRIPTOR_COUNT ||
        map->summary.extension_count !=
            YVEX_DEEPSEEK_GGUF_MTP_DESCRIPTOR_COUNT) {
        return map_reject(
            builder, YVEX_DEEPSEEK_GGUF_MAP_FAILURE_ACCOUNTING,
            YVEX_TENSOR_ROLE_UNKNOWN, YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL,
            YVEX_DEEPSEEK_GGUF_NO_INDEX, YVEX_DEEPSEEK_GGUF_NO_INDEX,
            YVEX_DEEPSEEK_GGUF_NO_INDEX, NULL, NULL,
            YVEX_DEEPSEEK_GGUF_DESCRIPTOR_COUNT,
            map->summary.descriptor_count);
    }
    for (index = 0u; index < map->summary.descriptor_count; ++index) {
        const yvex_deepseek_gguf_descriptor *descriptor =
            &map->descriptors[index];
        if (descriptor->scope != YVEX_DEEPSEEK_TENSOR_SCOPE_MTP)
            trunk[descriptor->collection]++;
        identity = map_hash_u64(identity, descriptor->identity);
    }
    if (trunk[YVEX_DEEPSEEK_TENSOR_COLLECTION_GLOBAL] != 6u ||
        trunk[YVEX_DEEPSEEK_TENSOR_COLLECTION_ATTENTION] != 344u ||
        trunk[YVEX_DEEPSEEK_TENSOR_COLLECTION_MHC] != 258u ||
        trunk[YVEX_DEEPSEEK_TENSOR_COLLECTION_NORM] != 86u ||
        trunk[YVEX_DEEPSEEK_TENSOR_COLLECTION_ROUTED_EXPERT] != 129u ||
        trunk[YVEX_DEEPSEEK_TENSOR_COLLECTION_SHARED_EXPERT] != 129u ||
        trunk[YVEX_DEEPSEEK_TENSOR_COLLECTION_ROUTER] != 86u ||
        trunk[YVEX_DEEPSEEK_TENSOR_COLLECTION_COMPRESSOR] != 164u ||
        trunk[YVEX_DEEPSEEK_TENSOR_COLLECTION_INDEXER] != 126u) {
        return map_reject(
            builder, YVEX_DEEPSEEK_GGUF_MAP_FAILURE_ACCOUNTING,
            YVEX_TENSOR_ROLE_UNKNOWN,
            YVEX_DEEPSEEK_TENSOR_SCOPE_MAIN_LAYER,
            YVEX_DEEPSEEK_GGUF_NO_INDEX, YVEX_DEEPSEEK_GGUF_NO_INDEX,
            YVEX_DEEPSEEK_GGUF_NO_INDEX, NULL, NULL, 1328u, 0u);
    }
    identity = map_hash_u64(identity, map->summary.source_identity);
    identity = map_hash_u64(identity, map->summary.coverage_identity);
    map->summary.mapping_identity = identity;
    map->summary.complete = 1;
    return YVEX_OK;
}

int yvex_deepseek_gguf_map_build_with_allocator(
    yvex_deepseek_gguf_map **out,
    const yvex_deepseek_v4_ir *architecture,
    const yvex_transform_ir *transform_ir,
    const yvex_deepseek_gguf_map_allocator *allocator,
    yvex_deepseek_gguf_map_failure *failure,
    yvex_error *err)
{
    const yvex_deepseek_v4_model_spec *model;
    const yvex_transform_ir_summary *transform_summary;
    yvex_deepseek_gguf_map *map;
    map_builder builder;
    size_t bytes;
    int rc;

    if (out) *out = NULL;
    map_failure_clear(failure);
    yvex_error_clear(err);
    memset(&builder, 0, sizeof(builder));
    builder.failure = failure;
    builder.err = err;
    if (!out || !architecture || !transform_ir || !allocator ||
        !allocator->allocate || !allocator->release) {
        return map_reject(
            &builder, YVEX_DEEPSEEK_GGUF_MAP_FAILURE_INVALID_ARGUMENT,
            YVEX_TENSOR_ROLE_UNKNOWN, YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL,
            YVEX_DEEPSEEK_GGUF_NO_INDEX, YVEX_DEEPSEEK_GGUF_NO_INDEX,
            YVEX_DEEPSEEK_GGUF_NO_INDEX, NULL, NULL, 1u, 0u);
    }
    model = yvex_deepseek_v4_ir_model(architecture);
    transform_summary = yvex_transform_ir_summary_get(transform_ir);
    if (!model || model->main_layer_count != 43u ||
        model->auxiliary_layer_count != 1u) {
        return map_reject(
            &builder, YVEX_DEEPSEEK_GGUF_MAP_FAILURE_ARCHITECTURE,
            YVEX_TENSOR_ROLE_UNKNOWN, YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL,
            YVEX_DEEPSEEK_GGUF_NO_INDEX, YVEX_DEEPSEEK_GGUF_NO_INDEX,
            YVEX_DEEPSEEK_GGUF_NO_INDEX, NULL, NULL, 44u,
            model ? model->main_layer_count + model->auxiliary_layer_count : 0u);
    }
    if (!transform_summary || !transform_summary->complete ||
        transform_summary->source_value_count !=
            YVEX_DEEPSEEK_GGUF_SOURCE_COUNT ||
        transform_summary->terminal_count !=
            YVEX_DEEPSEEK_GGUF_DESCRIPTOR_COUNT) {
        return map_reject(
            &builder, YVEX_DEEPSEEK_GGUF_MAP_FAILURE_TRANSFORM_IR,
            YVEX_TENSOR_ROLE_UNKNOWN, YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL,
            YVEX_DEEPSEEK_GGUF_NO_INDEX, YVEX_DEEPSEEK_GGUF_NO_INDEX,
            YVEX_DEEPSEEK_GGUF_NO_INDEX, NULL, NULL,
            YVEX_DEEPSEEK_GGUF_DESCRIPTOR_COUNT,
            transform_summary ? transform_summary->terminal_count : 0u);
    }
    map = (yvex_deepseek_gguf_map *)allocator->allocate(
        sizeof(*map), allocator->context);
    if (!map)
        return map_reject(
            &builder, YVEX_DEEPSEEK_GGUF_MAP_FAILURE_ALLOCATION,
            YVEX_TENSOR_ROLE_UNKNOWN, YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL,
            YVEX_DEEPSEEK_GGUF_NO_INDEX, YVEX_DEEPSEEK_GGUF_NO_INDEX,
            YVEX_DEEPSEEK_GGUF_NO_INDEX, "map", NULL, sizeof(*map), 0u);
    memset(map, 0, sizeof(*map));
    map->allocator = *allocator;
    map->source_index_capacity =
        map_index_capacity(YVEX_DEEPSEEK_GGUF_SOURCE_COUNT);
    map->emitted_index_capacity =
        map_index_capacity(YVEX_DEEPSEEK_GGUF_DESCRIPTOR_COUNT);
    map->role_index_capacity =
        map_index_capacity(YVEX_DEEPSEEK_GGUF_DESCRIPTOR_COUNT);
    map->descriptors = (yvex_deepseek_gguf_descriptor *)map_allocate_zero(
        map, (size_t)YVEX_DEEPSEEK_GGUF_DESCRIPTOR_COUNT *
             sizeof(*map->descriptors));
    map->contributions = (yvex_deepseek_gguf_contribution *)map_allocate_zero(
        map, (size_t)YVEX_DEEPSEEK_GGUF_SOURCE_COUNT *
             sizeof(*map->contributions));
    bytes = (size_t)map->source_index_capacity * sizeof(*map->source_index);
    map->source_index = (map_index_slot *)map_allocate_zero(map, bytes);
    bytes = (size_t)map->emitted_index_capacity * sizeof(*map->emitted_index);
    map->emitted_index = (map_index_slot *)map_allocate_zero(map, bytes);
    bytes = (size_t)map->role_index_capacity * sizeof(*map->role_index);
    map->role_index = (map_index_slot *)map_allocate_zero(map, bytes);
    builder.map = map;
    builder.architecture = architecture;
    builder.transform_ir = transform_ir;
    if (!map->descriptors || !map->contributions || !map->source_index ||
        !map->emitted_index || !map->role_index) {
        rc = map_reject(
            &builder, YVEX_DEEPSEEK_GGUF_MAP_FAILURE_ALLOCATION,
            YVEX_TENSOR_ROLE_UNKNOWN, YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL,
            YVEX_DEEPSEEK_GGUF_NO_INDEX, YVEX_DEEPSEEK_GGUF_NO_INDEX,
            YVEX_DEEPSEEK_GGUF_NO_INDEX, "mapping-tables", NULL, 1u, 0u);
        yvex_deepseek_gguf_map_close(map);
        return rc;
    }
    map->summary.header_scan_count = transform_summary->header_scan_count;
    map->summary.payload_bytes_read = transform_summary->payload_bytes_read;
    map->summary.source_identity = transform_summary->source_snapshot_identity;
    map->summary.coverage_identity = transform_summary->coverage_identity;
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
    yvex_deepseek_gguf_map **out,
    const yvex_deepseek_v4_ir *architecture,
    const yvex_transform_ir *transform_ir,
    yvex_deepseek_gguf_map_failure *failure,
    yvex_error *err)
{
    yvex_deepseek_gguf_map_allocator allocator;
    allocator.allocate = map_default_allocate;
    allocator.release = map_default_release;
    allocator.context = NULL;
    return yvex_deepseek_gguf_map_build_with_allocator(
        out, architecture, transform_ir, &allocator, failure, err);
}

void yvex_deepseek_gguf_map_close(yvex_deepseek_gguf_map *map)
{
    yvex_deepseek_gguf_map_allocator allocator;
    if (!map) return;
    allocator = map->allocator;
    if (map->role_index) allocator.release(map->role_index, allocator.context);
    if (map->emitted_index)
        allocator.release(map->emitted_index, allocator.context);
    if (map->source_index)
        allocator.release(map->source_index, allocator.context);
    if (map->contributions)
        allocator.release(map->contributions, allocator.context);
    if (map->descriptors)
        allocator.release(map->descriptors, allocator.context);
    allocator.release(map, allocator.context);
}

const yvex_deepseek_gguf_map_summary *yvex_deepseek_gguf_map_summary_get(
    const yvex_deepseek_gguf_map *map)
{
    return map ? &map->summary : NULL;
}

const yvex_deepseek_gguf_descriptor *yvex_deepseek_gguf_map_at(
    const yvex_deepseek_gguf_map *map,
    unsigned long long index)
{
    return map && index < map->summary.descriptor_count
        ? &map->descriptors[index] : NULL;
}

const yvex_deepseek_gguf_contribution *
yvex_deepseek_gguf_map_contribution_at(
    const yvex_deepseek_gguf_map *map,
    unsigned long long index)
{
    return map && index < map->summary.source_contribution_count
        ? &map->contributions[index] : NULL;
}

static const yvex_deepseek_gguf_descriptor *map_find_name(
    const yvex_deepseek_gguf_map *map,
    const char *name,
    int emitted)
{
    const map_index_slot *slots;
    unsigned long long capacity;
    unsigned long long hash;
    unsigned long long slot;
    unsigned long long probe;

    if (!map || !name) return NULL;
    slots = emitted ? map->emitted_index : map->source_index;
    capacity = emitted ? map->emitted_index_capacity
                       : map->source_index_capacity;
    hash = map_hash_string(name);
    slot = hash & (capacity - 1u);
    for (probe = 0u; probe < capacity && slots[slot].value_plus_one; ++probe) {
        if (slots[slot].hash == hash) {
            unsigned long long value = slots[slot].value_plus_one - 1u;
            if (emitted) {
                if (strcmp(map->descriptors[value].emitted_name, name) == 0)
                    return &map->descriptors[value];
            } else if (strcmp(map->contributions[value].source_name,
                              name) == 0) {
                return &map->descriptors[
                    map->contributions[value].descriptor_index];
            }
        }
        slot = (slot + 1u) & (capacity - 1u);
    }
    return NULL;
}

const yvex_deepseek_gguf_descriptor *yvex_deepseek_gguf_map_find_source(
    const yvex_deepseek_gguf_map *map,
    const char *source_name)
{
    return map_find_name(map, source_name, 0);
}

const yvex_deepseek_gguf_descriptor *yvex_deepseek_gguf_map_find_emitted(
    const yvex_deepseek_gguf_map *map,
    const char *emitted_name)
{
    return map_find_name(map, emitted_name, 1);
}

const yvex_deepseek_gguf_descriptor *yvex_deepseek_gguf_map_find_role(
    const yvex_deepseek_gguf_map *map,
    yvex_tensor_role role,
    yvex_deepseek_tensor_scope scope,
    unsigned long long layer_index,
    unsigned long long predictor_index)
{
    unsigned long long hash = 1469598103934665603ull;
    unsigned long long slot;
    unsigned long long probe;
    if (!map) return NULL;
    hash = map_hash_u64(hash, role);
    hash = map_hash_u64(hash, scope);
    hash = map_hash_u64(hash, layer_index);
    hash = map_hash_u64(hash, predictor_index);
    slot = hash & (map->role_index_capacity - 1u);
    for (probe = 0u; probe < map->role_index_capacity &&
         map->role_index[slot].value_plus_one; ++probe) {
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
    const yvex_deepseek_gguf_map *map,
    unsigned long long index)
{
    return map && index < map->summary.metadata_count
        ? &map->metadata[index] : NULL;
}

const yvex_deepseek_gguf_metadata *yvex_deepseek_gguf_map_metadata_find(
    const yvex_deepseek_gguf_map *map,
    const char *key)
{
    unsigned long long index;
    if (!map || !key) return NULL;
    for (index = 0u; index < map->summary.metadata_count; ++index)
        if (strcmp(map->metadata[index].key, key) == 0)
            return &map->metadata[index];
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
        "none", "invalid-argument", "architecture-incomplete",
        "coverage-row-mismatch", "missing-source", "duplicate-source",
        "source-dtype-mismatch", "expert-sequence-mismatch", "name-refused",
        "duplicate-name", "layout-refused", "metadata-refused",
        "accounting-mismatch", "arithmetic-overflow", "allocation-failure",
        "transform-ir-refused", "lowering-divergence",
        "mapping-identity-mismatch"
    };
    return code <= YVEX_DEEPSEEK_GGUF_MAP_FAILURE_MAPPING_IDENTITY
        ? names[code] : "unknown";
}
