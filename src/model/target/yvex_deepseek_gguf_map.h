/*
 * yvex_deepseek_gguf_map.h - canonical DeepSeek source-to-GGUF plan.
 *
 * Owner: src/model/target.
 * Owns: GGUF names, qtype/layout projection, typed source contributions,
 *   metadata prerequisites, deterministic mapping identity, indexes, and refusal.
 * Does not own: source IO, payload conversion, qtype policy, physical offsets,
 *   writer emission, materialization, runtime, rendering, or generation.
 * Invariants: every sealed Transformation IR terminal is lowered once and all
 *   source inputs are projected without rediscovering transformation semantics.
 * Boundary: this is a physical-format projection, not an emitted artifact.
 */
#ifndef YVEX_DEEPSEEK_GGUF_MAP_H
#define YVEX_DEEPSEEK_GGUF_MAP_H

#include <stddef.h>

#include <yvex/native_weights.h>
#include <yvex/tensor.h>

#include "yvex_deepseek_tensor_coverage.h"
#include "../architecture/yvex_deepseek_v4_ir.h"
#include "../compilation/yvex_transform_ir.h"
#include "../../gguf/yvex_gguf_map.h"

#define YVEX_DEEPSEEK_GGUF_NO_INDEX (~0ull)
#define YVEX_DEEPSEEK_GGUF_AGGREGATED_AXIS (~0u)
#define YVEX_DEEPSEEK_GGUF_DESCRIPTOR_COUNT 1360ull
#define YVEX_DEEPSEEK_GGUF_TRUNK_DESCRIPTOR_COUNT 1328ull
#define YVEX_DEEPSEEK_GGUF_MTP_DESCRIPTOR_COUNT 32ull
#define YVEX_DEEPSEEK_GGUF_SOURCE_COUNT 69187ull
#define YVEX_DEEPSEEK_GGUF_MAPPING_IDENTITY 0x1aecbbe25b04de0dull

typedef enum {
    YVEX_DEEPSEEK_GGUF_TRANSFORM_DIRECT = 0,
    YVEX_DEEPSEEK_GGUF_TRANSFORM_FP8_E4M3_E8M0,
    YVEX_DEEPSEEK_GGUF_TRANSFORM_EXPERT_MXFP4,
    YVEX_DEEPSEEK_GGUF_TRANSFORM_I64_TO_I32
} yvex_deepseek_gguf_transform;

typedef enum {
    YVEX_DEEPSEEK_GGUF_CONTRIBUTION_PRIMARY = 0,
    YVEX_DEEPSEEK_GGUF_CONTRIBUTION_SCALE,
    YVEX_DEEPSEEK_GGUF_CONTRIBUTION_EXPERT_WEIGHT,
    YVEX_DEEPSEEK_GGUF_CONTRIBUTION_EXPERT_SCALE,
    YVEX_DEEPSEEK_GGUF_CONTRIBUTION_ROUTING_TABLE
} yvex_deepseek_gguf_contribution_kind;

typedef enum {
    YVEX_DEEPSEEK_GGUF_METADATA_STRING = 0,
    YVEX_DEEPSEEK_GGUF_METADATA_U64,
    YVEX_DEEPSEEK_GGUF_METADATA_F64,
    YVEX_DEEPSEEK_GGUF_METADATA_BOOL,
    YVEX_DEEPSEEK_GGUF_METADATA_U64_ARRAY,
    YVEX_DEEPSEEK_GGUF_METADATA_F64_ARRAY
} yvex_deepseek_gguf_metadata_type;

typedef enum {
    YVEX_DEEPSEEK_GGUF_MAP_FAILURE_NONE = 0,
    YVEX_DEEPSEEK_GGUF_MAP_FAILURE_INVALID_ARGUMENT,
    YVEX_DEEPSEEK_GGUF_MAP_FAILURE_ARCHITECTURE,
    YVEX_DEEPSEEK_GGUF_MAP_FAILURE_COVERAGE_ROW,
    YVEX_DEEPSEEK_GGUF_MAP_FAILURE_MISSING_SOURCE,
    YVEX_DEEPSEEK_GGUF_MAP_FAILURE_DUPLICATE_SOURCE,
    YVEX_DEEPSEEK_GGUF_MAP_FAILURE_SOURCE_DTYPE,
    YVEX_DEEPSEEK_GGUF_MAP_FAILURE_EXPERT_SEQUENCE,
    YVEX_DEEPSEEK_GGUF_MAP_FAILURE_NAME,
    YVEX_DEEPSEEK_GGUF_MAP_FAILURE_DUPLICATE_NAME,
    YVEX_DEEPSEEK_GGUF_MAP_FAILURE_LAYOUT,
    YVEX_DEEPSEEK_GGUF_MAP_FAILURE_METADATA,
    YVEX_DEEPSEEK_GGUF_MAP_FAILURE_ACCOUNTING,
    YVEX_DEEPSEEK_GGUF_MAP_FAILURE_ARITHMETIC_OVERFLOW,
    YVEX_DEEPSEEK_GGUF_MAP_FAILURE_ALLOCATION,
    YVEX_DEEPSEEK_GGUF_MAP_FAILURE_TRANSFORM_IR,
    YVEX_DEEPSEEK_GGUF_MAP_FAILURE_LOWERING_DIVERGENCE,
    YVEX_DEEPSEEK_GGUF_MAP_FAILURE_MAPPING_IDENTITY
} yvex_deepseek_gguf_map_failure_code;

typedef struct {
    yvex_deepseek_gguf_map_failure_code code;
    yvex_tensor_role role;
    yvex_deepseek_tensor_scope scope;
    unsigned long long layer_index;
    unsigned long long predictor_index;
    unsigned long long expert_index;
    unsigned long long expected;
    unsigned long long actual;
    char source_name[256];
    char emitted_name[192];
} yvex_deepseek_gguf_map_failure;

typedef struct {
    char source_name[256];
    yvex_native_dtype source_dtype;
    unsigned int source_rank;
    unsigned long long source_dims[2];
    yvex_deepseek_gguf_contribution_kind kind;
    unsigned long long source_row_index;
    unsigned long long descriptor_index;
    unsigned long long expert_index;
} yvex_deepseek_gguf_contribution;

typedef struct {
    yvex_tensor_role role;
    yvex_deepseek_tensor_collection collection;
    yvex_deepseek_tensor_scope scope;
    unsigned long long layer_index;
    unsigned long long predictor_index;
    unsigned long long expert_count;
    char emitted_name[192];
    yvex_deepseek_gguf_transform transform;
    yvex_gguf_name_provenance name_provenance;
    unsigned int forced_qtype;
    unsigned int logical_rank;
    unsigned long long logical_dims[YVEX_TENSOR_MAX_DIMS];
    unsigned int source_axis_for_logical[YVEX_TENSOR_MAX_DIMS];
    unsigned long long contribution_offset;
    unsigned long long contribution_count;
    unsigned long long identity;
} yvex_deepseek_gguf_descriptor;

typedef struct {
    char key[128];
    yvex_deepseek_gguf_metadata_type type;
    char string_value[192];
    unsigned long long u64_value;
    double f64_value;
    int bool_value;
    unsigned long long array_values[64];
    double f64_array_values[64];
    unsigned int array_count;
} yvex_deepseek_gguf_metadata;

typedef struct {
    unsigned long long source_contribution_count;
    unsigned long long descriptor_count;
    unsigned long long trunk_descriptor_count;
    unsigned long long mtp_descriptor_count;
    unsigned long long pinned_standard_count;
    unsigned long long semantic_standard_count;
    unsigned long long extension_count;
    unsigned long long collection_counts[YVEX_DEEPSEEK_TENSOR_COLLECTION_COUNT];
    unsigned long long metadata_count;
    unsigned long long header_scan_count;
    unsigned long long payload_bytes_read;
    unsigned long long source_identity;
    unsigned long long coverage_identity;
    unsigned long long mapping_identity;
    int complete;
} yvex_deepseek_gguf_map_summary;

typedef void *(*yvex_deepseek_gguf_map_allocate_fn)(size_t, void *);
typedef void (*yvex_deepseek_gguf_map_release_fn)(void *, void *);

typedef struct {
    yvex_deepseek_gguf_map_allocate_fn allocate;
    yvex_deepseek_gguf_map_release_fn release;
    void *context;
} yvex_deepseek_gguf_map_allocator;

typedef struct yvex_deepseek_gguf_map yvex_deepseek_gguf_map;

int yvex_deepseek_gguf_map_build(
    yvex_deepseek_gguf_map **out,
    const yvex_deepseek_v4_ir *ir,
    const yvex_transform_ir *transform_ir,
    yvex_deepseek_gguf_map_failure *failure,
    yvex_error *err);
int yvex_deepseek_gguf_map_build_with_allocator(
    yvex_deepseek_gguf_map **out,
    const yvex_deepseek_v4_ir *ir,
    const yvex_transform_ir *transform_ir,
    const yvex_deepseek_gguf_map_allocator *allocator,
    yvex_deepseek_gguf_map_failure *failure,
    yvex_error *err);
void yvex_deepseek_gguf_map_close(yvex_deepseek_gguf_map *map);

const yvex_deepseek_gguf_map_summary *yvex_deepseek_gguf_map_summary_get(
    const yvex_deepseek_gguf_map *map);
const yvex_deepseek_gguf_descriptor *yvex_deepseek_gguf_map_at(
    const yvex_deepseek_gguf_map *map, unsigned long long index);
const yvex_deepseek_gguf_contribution *yvex_deepseek_gguf_map_contribution_at(
    const yvex_deepseek_gguf_map *map, unsigned long long index);
const yvex_deepseek_gguf_descriptor *yvex_deepseek_gguf_map_find_source(
    const yvex_deepseek_gguf_map *map, const char *source_name);
const yvex_deepseek_gguf_descriptor *yvex_deepseek_gguf_map_find_emitted(
    const yvex_deepseek_gguf_map *map, const char *emitted_name);
const yvex_deepseek_gguf_descriptor *yvex_deepseek_gguf_map_find_role(
    const yvex_deepseek_gguf_map *map, yvex_tensor_role role,
    yvex_deepseek_tensor_scope scope, unsigned long long layer_index,
    unsigned long long predictor_index);
const yvex_deepseek_gguf_metadata *yvex_deepseek_gguf_map_metadata_at(
    const yvex_deepseek_gguf_map *map, unsigned long long index);
const yvex_deepseek_gguf_metadata *yvex_deepseek_gguf_map_metadata_find(
    const yvex_deepseek_gguf_map *map, const char *key);

const char *yvex_deepseek_gguf_transform_name(
    yvex_deepseek_gguf_transform transform);
const char *yvex_deepseek_gguf_map_failure_name(
    yvex_deepseek_gguf_map_failure_code code);

#endif
