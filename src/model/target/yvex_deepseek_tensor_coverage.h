/*
 * yvex_deepseek_tensor_coverage.h - exact DeepSeek source tensor coverage.
 *
 * Owner:
 *   src/model/target
 *
 * Owns:
 *   immutable IR-derived source tensor requirements, exact header-snapshot
 *   reconciliation, typed coverage failures, lookup, summary, and lifetime.
 *
 * Does not own:
 *   source parsing, architecture derivation, GGUF naming/layout, payload IO,
 *   quantization, artifact emission, materialization, runtime, or rendering.
 *
 * Invariants:
 *   every published row has exactly one source entry; every snapshot entry is
 *   consumed exactly once; construction performs no source IO or payload read.
 *
 * Boundary:
 *   complete source tensor coverage is a pre-mapping admission fact, not an
 *   emitted artifact or execution capability.
 */
#ifndef YVEX_DEEPSEEK_TENSOR_COVERAGE_H
#define YVEX_DEEPSEEK_TENSOR_COVERAGE_H

#include <stddef.h>

#include <yvex/error.h>
#include <yvex/native_weights.h>

#include "../architecture/yvex_deepseek_v4_ir.h"
#include "../../source/yvex_source_inventory.h"

typedef enum {
    YVEX_DEEPSEEK_TENSOR_COLLECTION_GLOBAL = 0,
    YVEX_DEEPSEEK_TENSOR_COLLECTION_ATTENTION,
    YVEX_DEEPSEEK_TENSOR_COLLECTION_COMPRESSOR,
    YVEX_DEEPSEEK_TENSOR_COLLECTION_INDEXER,
    YVEX_DEEPSEEK_TENSOR_COLLECTION_NORM,
    YVEX_DEEPSEEK_TENSOR_COLLECTION_MHC,
    YVEX_DEEPSEEK_TENSOR_COLLECTION_ROUTER,
    YVEX_DEEPSEEK_TENSOR_COLLECTION_ROUTED_EXPERT,
    YVEX_DEEPSEEK_TENSOR_COLLECTION_SHARED_EXPERT,
    YVEX_DEEPSEEK_TENSOR_COLLECTION_AUXILIARY,
    YVEX_DEEPSEEK_TENSOR_COLLECTION_COUNT
} yvex_deepseek_tensor_collection;

typedef enum {
    YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL = 0,
    YVEX_DEEPSEEK_TENSOR_SCOPE_MAIN_LAYER,
    YVEX_DEEPSEEK_TENSOR_SCOPE_MTP
} yvex_deepseek_tensor_scope;

typedef enum {
    YVEX_DEEPSEEK_COVERAGE_FAILURE_NONE = 0,
    YVEX_DEEPSEEK_COVERAGE_FAILURE_INVALID_ARGUMENT,
    YVEX_DEEPSEEK_COVERAGE_FAILURE_SOURCE_IDENTITY,
    YVEX_DEEPSEEK_COVERAGE_FAILURE_INVENTORY_AUTHORITY,
    YVEX_DEEPSEEK_COVERAGE_FAILURE_INVENTORY_DRIFT,
    YVEX_DEEPSEEK_COVERAGE_FAILURE_ARCHITECTURE_INCOMPLETE,
    YVEX_DEEPSEEK_COVERAGE_FAILURE_MISSING_REQUIREMENT,
    YVEX_DEEPSEEK_COVERAGE_FAILURE_AMBIGUOUS_MATCH,
    YVEX_DEEPSEEK_COVERAGE_FAILURE_UNEXPECTED_SOURCE,
    YVEX_DEEPSEEK_COVERAGE_FAILURE_INVALID_INDEX,
    YVEX_DEEPSEEK_COVERAGE_FAILURE_RANK_MISMATCH,
    YVEX_DEEPSEEK_COVERAGE_FAILURE_SHAPE_MISMATCH,
    YVEX_DEEPSEEK_COVERAGE_FAILURE_DTYPE_MISMATCH,
    YVEX_DEEPSEEK_COVERAGE_FAILURE_SCALE_COMPANION,
    YVEX_DEEPSEEK_COVERAGE_FAILURE_ARITHMETIC_OVERFLOW,
    YVEX_DEEPSEEK_COVERAGE_FAILURE_RESOURCE_LIMIT,
    YVEX_DEEPSEEK_COVERAGE_FAILURE_ALLOCATION
} yvex_deepseek_tensor_coverage_failure_code;

#define YVEX_DEEPSEEK_TENSOR_NO_INDEX (~0ull)

typedef struct {
    yvex_deepseek_tensor_coverage_failure_code code;
    yvex_deepseek_tensor_collection collection;
    yvex_deepseek_tensor_scope scope;
    char tensor_name[256];
    unsigned long long layer_index;
    unsigned long long expert_index;
    unsigned long long dimension_index;
    unsigned long long expected;
    unsigned long long actual;
} yvex_deepseek_tensor_coverage_failure;

typedef struct {
    const yvex_native_weight_info *source;
    yvex_deepseek_tensor_collection collection;
    yvex_deepseek_tensor_scope scope;
    unsigned long long layer_index;
    unsigned long long expert_index;
} yvex_deepseek_tensor_coverage_row;

typedef struct {
    unsigned long long source_tensor_count;
    unsigned long long required_tensor_count;
    unsigned long long matched_tensor_count;
    unsigned long long missing_count;
    unsigned long long ambiguous_count;
    unsigned long long unexpected_count;
    unsigned long long collection_counts[YVEX_DEEPSEEK_TENSOR_COLLECTION_COUNT];
    unsigned long long main_layer_count;
    unsigned long long auxiliary_layer_count;
    unsigned long long routed_expert_count;
    unsigned long long shared_expert_count;
    unsigned long long header_scan_count;
    unsigned long long payload_bytes_read;
    unsigned long long source_lookup_count;
    unsigned long long source_collision_count;
    unsigned long long source_maximum_probe;
    unsigned long long source_identity;
    unsigned long long coverage_identity;
    int complete;
} yvex_deepseek_tensor_coverage_summary;

typedef void *(*yvex_deepseek_tensor_coverage_allocate_fn)(size_t size,
                                                            void *context);
typedef void (*yvex_deepseek_tensor_coverage_release_fn)(void *allocation,
                                                         void *context);

typedef struct {
    yvex_deepseek_tensor_coverage_allocate_fn allocate;
    yvex_deepseek_tensor_coverage_release_fn release;
    void *context;
    unsigned long long maximum_tensors;
} yvex_deepseek_tensor_coverage_options;

typedef struct yvex_deepseek_tensor_coverage yvex_deepseek_tensor_coverage;

int yvex_deepseek_tensor_coverage_build(
    yvex_deepseek_tensor_coverage **out,
    const yvex_source_verification *verification,
    const yvex_deepseek_v4_ir *ir,
    yvex_source_tensor_snapshot *snapshot,
    const yvex_deepseek_tensor_coverage_options *options,
    yvex_deepseek_tensor_coverage_failure *failure,
    yvex_error *err);

int yvex_deepseek_tensor_coverage_open_verified_source(
    yvex_deepseek_tensor_coverage **out,
    yvex_source_verification *verification,
    const char *source_path,
    const char *models_root,
    yvex_deepseek_tensor_coverage_failure *failure,
    yvex_error *err);

void yvex_deepseek_tensor_coverage_close(
    yvex_deepseek_tensor_coverage *coverage);
const yvex_deepseek_tensor_coverage_summary *
yvex_deepseek_tensor_coverage_summary_get(
    const yvex_deepseek_tensor_coverage *coverage);
const yvex_deepseek_tensor_coverage_row *
yvex_deepseek_tensor_coverage_at(
    const yvex_deepseek_tensor_coverage *coverage,
    unsigned long long index);
const yvex_deepseek_tensor_coverage_row *
yvex_deepseek_tensor_coverage_find(
    const yvex_deepseek_tensor_coverage *coverage,
    const char *source_name);
int yvex_deepseek_tensor_coverage_find_index(
    const yvex_deepseek_tensor_coverage *coverage,
    const char *source_name,
    unsigned long long *row_index);
const char *yvex_deepseek_tensor_collection_name(
    yvex_deepseek_tensor_collection collection);
const char *yvex_deepseek_tensor_coverage_failure_name(
    yvex_deepseek_tensor_coverage_failure_code code);

#endif
