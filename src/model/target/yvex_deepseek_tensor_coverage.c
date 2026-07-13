/*
 * yvex_deepseek_tensor_coverage.c - exact DeepSeek source tensor coverage.
 *
 * Owner:
 *   src/model/target
 *
 * Owns:
 *   deterministic requirement derivation from the canonical IR, O(1)-average
 *   snapshot reconciliation, typed geometry/storage refusal, and immutable
 *   coverage lifetime.
 *
 * Does not own:
 *   source IO, architecture interpretation outside the IR, GGUF naming or
 *   layout, payload reads, conversion, materialization, runtime, or rendering.
 *
 * Invariants:
 *   each requirement consumes one unique snapshot row and every snapshot row
 *   is consumed; successful construction reads zero payload bytes.
 *
 * Boundary:
 *   coverage closes source obligations only and cannot promote mapping,
 *   artifact, materialization, runtime, or generation support.
 */
#include "yvex_deepseek_tensor_coverage.h"

#include "yvex_model_target_catalog.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEEPSEEK_COVERAGE_DEFAULT_LIMIT 100000ull

struct yvex_deepseek_tensor_coverage {
    yvex_deepseek_tensor_coverage_options options;
    yvex_source_tensor_snapshot *snapshot;
    yvex_deepseek_tensor_coverage_row *rows;
    const yvex_deepseek_tensor_coverage_row **row_by_source;
    yvex_deepseek_tensor_coverage_summary summary;
};

typedef struct {
    yvex_deepseek_tensor_coverage *coverage;
    unsigned char *matched;
    yvex_deepseek_tensor_coverage_failure *failure;
    yvex_error *err;
} coverage_builder;

static void *coverage_allocate(size_t size, void *context)
{
    (void)context;
    return malloc(size);
}

static void coverage_release(void *allocation, void *context)
{
    (void)context;
    free(allocation);
}

static void coverage_failure_clear(
    yvex_deepseek_tensor_coverage_failure *failure)
{
    if (!failure) return;
    memset(failure, 0, sizeof(*failure));
    failure->layer_index = YVEX_DEEPSEEK_TENSOR_NO_INDEX;
    failure->expert_index = YVEX_DEEPSEEK_TENSOR_NO_INDEX;
    failure->dimension_index = YVEX_DEEPSEEK_TENSOR_NO_INDEX;
}

static int coverage_reject(
    yvex_deepseek_tensor_coverage_failure *failure,
    yvex_deepseek_tensor_coverage_failure_code code,
    yvex_deepseek_tensor_collection collection,
    yvex_deepseek_tensor_scope scope,
    const char *name,
    unsigned long long layer,
    unsigned long long expert,
    unsigned long long dimension,
    unsigned long long expected,
    unsigned long long actual,
    yvex_error *err)
{
    yvex_status status = code == YVEX_DEEPSEEK_COVERAGE_FAILURE_ALLOCATION
                             ? YVEX_ERR_NOMEM
                             : (code == YVEX_DEEPSEEK_COVERAGE_FAILURE_INVALID_ARGUMENT
                                    ? YVEX_ERR_INVALID_ARG
                                    : YVEX_ERR_FORMAT);

    if (failure) {
        coverage_failure_clear(failure);
        failure->code = code;
        failure->collection = collection;
        failure->scope = scope;
        (void)snprintf(failure->tensor_name, sizeof(failure->tensor_name),
                       "%s", name ? name : "");
        failure->layer_index = layer;
        failure->expert_index = expert;
        failure->dimension_index = dimension;
        failure->expected = expected;
        failure->actual = actual;
    }
    yvex_error_setf(err, status, "deepseek_tensor_coverage",
                    "%s tensor=%s layer=%llu expert=%llu dimension=%llu expected=%llu actual=%llu",
                    yvex_deepseek_tensor_coverage_failure_name(code),
                    name ? name : "none", layer, expert, dimension,
                    expected, actual);
    return status;
}

static unsigned long long coverage_hash_bytes(unsigned long long hash,
                                              const void *data,
                                              size_t length)
{
    const unsigned char *bytes = (const unsigned char *)data;
    size_t i;

    for (i = 0u; i < length; ++i) {
        hash ^= bytes[i];
        hash *= 1099511628211ull;
    }
    return hash;
}

static unsigned long long coverage_hash_u64(unsigned long long hash,
                                            unsigned long long value)
{
    unsigned char bytes[8];
    unsigned int i;

    for (i = 0u; i < sizeof(bytes); ++i)
        bytes[i] = (unsigned char)((value >> (i * 8u)) & 0xffu);
    return coverage_hash_bytes(hash, bytes, sizeof(bytes));
}

static const char *coverage_scope_name(yvex_deepseek_tensor_scope scope)
{
    static const char *names[] = {"global", "main-layer", "mtp"};

    return scope <= YVEX_DEEPSEEK_TENSOR_SCOPE_MTP ? names[scope] : "unknown";
}

/*
 * Reconciles one IR-derived slot against the retained snapshot and publishes
 * one borrowed row. Performs no IO or allocation; refusal mutates only the
 * unpublished summary and typed failure facts.
 */
static int coverage_require(coverage_builder *builder,
                            const char *name,
                            yvex_deepseek_tensor_collection collection,
                            yvex_deepseek_tensor_scope scope,
                            unsigned long long layer,
                            unsigned long long expert,
                            yvex_native_dtype dtype,
                            unsigned int rank,
                            const unsigned long long *dims)
{
    yvex_deepseek_tensor_coverage *coverage = builder->coverage;
    const yvex_native_weight_info *source;
    unsigned long long source_index;
    unsigned int dimension;
    unsigned long long row_index = coverage->summary.required_tensor_count;

    if (!yvex_source_tensor_snapshot_find_index(
            coverage->snapshot, name, &source_index)) {
        coverage->summary.missing_count++;
        return coverage_reject(
            builder->failure, YVEX_DEEPSEEK_COVERAGE_FAILURE_MISSING_REQUIREMENT,
            collection, scope, name, layer, expert,
            YVEX_DEEPSEEK_TENSOR_NO_INDEX, 1u, 0u, builder->err);
    }
    if (row_index >= coverage->summary.source_tensor_count) {
        return coverage_reject(
            builder->failure, YVEX_DEEPSEEK_COVERAGE_FAILURE_RESOURCE_LIMIT,
            collection, scope, name, layer, expert,
            YVEX_DEEPSEEK_TENSOR_NO_INDEX,
            coverage->summary.source_tensor_count, row_index + 1u,
            builder->err);
    }
    source = yvex_source_tensor_snapshot_at(coverage->snapshot, source_index);
    if (builder->matched[source_index]) {
        coverage->summary.ambiguous_count++;
        return coverage_reject(
            builder->failure, YVEX_DEEPSEEK_COVERAGE_FAILURE_AMBIGUOUS_MATCH,
            collection, scope, name, layer, expert,
            YVEX_DEEPSEEK_TENSOR_NO_INDEX, 1u, 2u, builder->err);
    }
    if (source->rank != rank) {
        return coverage_reject(
            builder->failure, YVEX_DEEPSEEK_COVERAGE_FAILURE_RANK_MISMATCH,
            collection, scope, name, layer, expert,
            YVEX_DEEPSEEK_TENSOR_NO_INDEX, rank, source->rank, builder->err);
    }
    for (dimension = 0u; dimension < rank; ++dimension) {
        if (source->dims[dimension] != dims[dimension]) {
            return coverage_reject(
                builder->failure, YVEX_DEEPSEEK_COVERAGE_FAILURE_SHAPE_MISMATCH,
                collection, scope, name, layer, expert, dimension,
                dims[dimension], source->dims[dimension], builder->err);
        }
    }
    if (source->dtype != dtype) {
        return coverage_reject(
            builder->failure, YVEX_DEEPSEEK_COVERAGE_FAILURE_DTYPE_MISMATCH,
            collection, scope, name, layer, expert,
            YVEX_DEEPSEEK_TENSOR_NO_INDEX, (unsigned long long)dtype,
            (unsigned long long)source->dtype, builder->err);
    }
    builder->matched[source_index] = 1u;
    coverage->rows[row_index].source = source;
    coverage->rows[row_index].collection = collection;
    coverage->rows[row_index].scope = scope;
    coverage->rows[row_index].layer_index = layer;
    coverage->rows[row_index].expert_index = expert;
    coverage->row_by_source[source_index] = &coverage->rows[row_index];
    coverage->summary.required_tensor_count++;
    coverage->summary.matched_tensor_count++;
    coverage->summary.collection_counts[collection]++;
    return YVEX_OK;
}

static int coverage_vector(coverage_builder *builder,
                           const char *name,
                           yvex_deepseek_tensor_collection collection,
                           yvex_deepseek_tensor_scope scope,
                           unsigned long long layer,
                           unsigned long long expert,
                           yvex_native_dtype dtype,
                           unsigned long long width)
{
    unsigned long long dims[1] = {width};
    return coverage_require(builder, name, collection, scope, layer, expert,
                            dtype, 1u, dims);
}

static int coverage_matrix(coverage_builder *builder,
                           const char *name,
                           yvex_deepseek_tensor_collection collection,
                           yvex_deepseek_tensor_scope scope,
                           unsigned long long layer,
                           unsigned long long expert,
                           yvex_native_dtype dtype,
                           unsigned long long rows,
                           unsigned long long columns)
{
    unsigned long long dims[2] = {rows, columns};
    return coverage_require(builder, name, collection, scope, layer, expert,
                            dtype, 2u, dims);
}

/*
 * Admits a quantization companion as an independent source obligation. No
 * payload is read; absence prevents publication of complete coverage.
 */
static int coverage_companion_matrix(
    coverage_builder *builder,
    const char *name,
    yvex_deepseek_tensor_collection collection,
    yvex_deepseek_tensor_scope scope,
    unsigned long long layer,
    unsigned long long expert,
    yvex_native_dtype dtype,
    unsigned long long rows,
    unsigned long long columns)
{
    if (!yvex_source_tensor_snapshot_find(builder->coverage->snapshot, name)) {
        builder->coverage->summary.missing_count++;
        return coverage_reject(
            builder->failure,
            YVEX_DEEPSEEK_COVERAGE_FAILURE_SCALE_COMPANION,
            collection, scope, name, layer, expert,
            YVEX_DEEPSEEK_TENSOR_NO_INDEX, 1u, 0u, builder->err);
    }
    return coverage_matrix(builder, name, collection, scope, layer, expert,
                           dtype, rows, columns);
}

static int coverage_fp8_pair(coverage_builder *builder,
                             const char *base,
                             yvex_deepseek_tensor_collection collection,
                             yvex_deepseek_tensor_scope scope,
                             unsigned long long layer,
                             unsigned long long expert,
                             unsigned long long rows,
                             unsigned long long columns,
                             const yvex_deepseek_v4_source_constraint *storage)
{
    char name[256];
    int rc;

    if (!storage->quant_block_rows || !storage->quant_block_columns ||
        rows % storage->quant_block_rows != 0u ||
        columns % storage->quant_block_columns != 0u) {
        return coverage_reject(
            builder->failure, YVEX_DEEPSEEK_COVERAGE_FAILURE_SCALE_COMPANION,
            collection, scope, base, layer, expert,
            YVEX_DEEPSEEK_TENSOR_NO_INDEX, 0u, 0u, builder->err);
    }
    (void)snprintf(name, sizeof(name), "%s.weight", base);
    rc = coverage_matrix(builder, name, collection, scope, layer, expert,
                         YVEX_NATIVE_DTYPE_F8_E4M3, rows, columns);
    if (rc != YVEX_OK) return rc;
    (void)snprintf(name, sizeof(name), "%s.scale", base);
    return coverage_companion_matrix(
        builder, name, collection, scope, layer, expert, storage->scale_dtype,
        rows / storage->quant_block_rows,
        columns / storage->quant_block_columns);
}

/*
 * Derives pinned FP4 physical and E8M0 scale shapes with checked divisibility.
 * Invalid storage geometry is typed before either requirement can close.
 */
static int coverage_fp4_pair(coverage_builder *builder,
                             const char *base,
                             yvex_deepseek_tensor_scope scope,
                             unsigned long long layer,
                             unsigned long long expert,
                             unsigned long long rows,
                             unsigned long long columns,
                             const yvex_deepseek_v4_source_constraint *storage)
{
    char name[256];
    int rc;

    if (!storage->fp4_packing_factor || !storage->fp4_scale_group_width ||
        columns % storage->fp4_packing_factor != 0u ||
        columns % storage->fp4_scale_group_width != 0u) {
        return coverage_reject(
            builder->failure, YVEX_DEEPSEEK_COVERAGE_FAILURE_SCALE_COMPANION,
            YVEX_DEEPSEEK_TENSOR_COLLECTION_ROUTED_EXPERT, scope, base,
            layer, expert, YVEX_DEEPSEEK_TENSOR_NO_INDEX, 0u, columns,
            builder->err);
    }
    (void)snprintf(name, sizeof(name), "%s.weight", base);
    rc = coverage_matrix(
        builder, name, YVEX_DEEPSEEK_TENSOR_COLLECTION_ROUTED_EXPERT,
        scope, layer, expert, storage->fp4_physical_dtype, rows,
        columns / storage->fp4_packing_factor);
    if (rc != YVEX_OK) return rc;
    (void)snprintf(name, sizeof(name), "%s.scale", base);
    return coverage_companion_matrix(
        builder, name, YVEX_DEEPSEEK_TENSOR_COLLECTION_ROUTED_EXPERT,
        scope, layer, expert, storage->scale_dtype, rows,
        columns / storage->fp4_scale_group_width);
}

static int coverage_mhc(coverage_builder *builder,
                        const char *prefix,
                        const char *kind,
                        yvex_deepseek_tensor_scope scope,
                        unsigned long long layer,
                        const yvex_deepseek_v4_mhc_spec *mhc)
{
    char name[256];
    int rc;

    (void)snprintf(name, sizeof(name), "%s.hc_%s_fn", prefix, kind);
    rc = coverage_matrix(builder, name,
                         YVEX_DEEPSEEK_TENSOR_COLLECTION_MHC, scope, layer,
                         YVEX_DEEPSEEK_TENSOR_NO_INDEX, YVEX_NATIVE_DTYPE_F32,
                         mhc->mixing_rows, mhc->mixing_columns);
    if (rc != YVEX_OK) return rc;
    (void)snprintf(name, sizeof(name), "%s.hc_%s_base", prefix, kind);
    rc = coverage_vector(builder, name,
                         YVEX_DEEPSEEK_TENSOR_COLLECTION_MHC, scope, layer,
                         YVEX_DEEPSEEK_TENSOR_NO_INDEX, YVEX_NATIVE_DTYPE_F32,
                         mhc->base_width);
    if (rc != YVEX_OK) return rc;
    (void)snprintf(name, sizeof(name), "%s.hc_%s_scale", prefix, kind);
    return coverage_vector(builder, name,
                           YVEX_DEEPSEEK_TENSOR_COLLECTION_MHC, scope, layer,
                           YVEX_DEEPSEEK_TENSOR_NO_INDEX,
                           YVEX_NATIVE_DTYPE_F32, mhc->scale_width);
}

static int coverage_mhc_head(
    coverage_builder *builder,
    const char *prefix,
    yvex_deepseek_tensor_scope scope,
    unsigned long long layer,
    const yvex_deepseek_v4_mhc_head_spec *head,
    yvex_deepseek_tensor_collection collection)
{
    char name[256];
    int rc;

    if (!head->required) return YVEX_OK;
    (void)snprintf(name, sizeof(name), "%shc_head_fn", prefix);
    rc = coverage_matrix(builder, name, collection, scope, layer,
                         YVEX_DEEPSEEK_TENSOR_NO_INDEX, YVEX_NATIVE_DTYPE_F32,
                         head->function_rows, head->function_columns);
    if (rc != YVEX_OK) return rc;
    (void)snprintf(name, sizeof(name), "%shc_head_base", prefix);
    rc = coverage_vector(builder, name, collection, scope, layer,
                         YVEX_DEEPSEEK_TENSOR_NO_INDEX, YVEX_NATIVE_DTYPE_F32,
                         head->base_width);
    if (rc != YVEX_OK) return rc;
    (void)snprintf(name, sizeof(name), "%shc_head_scale", prefix);
    return coverage_vector(builder, name, collection, scope, layer,
                           YVEX_DEEPSEEK_TENSOR_NO_INDEX,
                           YVEX_NATIVE_DTYPE_F32, head->scale_width);
}

/*
 * Projects the layer attention class and its IR-owned projection, compressor,
 * and indexer geometry into exact source obligations. It performs indexed
 * snapshot lookups only and returns on the first typed mismatch.
 */
static int coverage_attention(
    coverage_builder *builder,
    const char *prefix,
    const yvex_deepseek_v4_layer_spec *layer,
    yvex_deepseek_tensor_scope scope,
    const yvex_deepseek_v4_source_constraint *storage)
{
    char name[256];
    char base[256];
    int rc;

    (void)snprintf(name, sizeof(name), "%s.attn.attn_sink", prefix);
    rc = coverage_vector(builder, name,
                         YVEX_DEEPSEEK_TENSOR_COLLECTION_ATTENTION, scope,
                         layer->layer_index, YVEX_DEEPSEEK_TENSOR_NO_INDEX,
                         YVEX_NATIVE_DTYPE_F32, layer->attention_sink_count);
    if (rc != YVEX_OK) return rc;
    (void)snprintf(name, sizeof(name), "%s.attn.q_norm.weight", prefix);
    rc = coverage_vector(builder, name,
                         YVEX_DEEPSEEK_TENSOR_COLLECTION_ATTENTION, scope,
                         layer->layer_index, YVEX_DEEPSEEK_TENSOR_NO_INDEX,
                         YVEX_NATIVE_DTYPE_BF16, layer->query_lora_rank);
    if (rc != YVEX_OK) return rc;
    (void)snprintf(name, sizeof(name), "%s.attn.kv_norm.weight", prefix);
    rc = coverage_vector(builder, name,
                         YVEX_DEEPSEEK_TENSOR_COLLECTION_ATTENTION, scope,
                         layer->layer_index, YVEX_DEEPSEEK_TENSOR_NO_INDEX,
                         YVEX_NATIVE_DTYPE_BF16, layer->head_dimension);
    if (rc != YVEX_OK) return rc;
#define REQUIRE_ATTN_PAIR(member, suffix)                                      \
    do {                                                                        \
        (void)snprintf(base, sizeof(base), "%s.attn.%s", prefix, suffix);      \
        rc = coverage_fp8_pair(builder, base,                                   \
                               YVEX_DEEPSEEK_TENSOR_COLLECTION_ATTENTION,       \
                               scope, layer->layer_index,                       \
                               YVEX_DEEPSEEK_TENSOR_NO_INDEX,                   \
                               layer->tensors.member##_rows,                    \
                               layer->tensors.member##_columns, storage);       \
        if (rc != YVEX_OK) return rc;                                           \
    } while (0)
    REQUIRE_ATTN_PAIR(kv, "wkv");
    REQUIRE_ATTN_PAIR(q_a, "wq_a");
    REQUIRE_ATTN_PAIR(q_b, "wq_b");
    REQUIRE_ATTN_PAIR(o_a, "wo_a");
    REQUIRE_ATTN_PAIR(o_b, "wo_b");
#undef REQUIRE_ATTN_PAIR
    if (layer->compressor_required) {
        (void)snprintf(name, sizeof(name), "%s.attn.compressor.ape", prefix);
        rc = coverage_matrix(builder, name,
                             YVEX_DEEPSEEK_TENSOR_COLLECTION_COMPRESSOR,
                             scope, layer->layer_index,
                             YVEX_DEEPSEEK_TENSOR_NO_INDEX,
                             YVEX_NATIVE_DTYPE_F32,
                             layer->tensors.compressor_ape_rows,
                             layer->tensors.compressor_ape_columns);
        if (rc != YVEX_OK) return rc;
        (void)snprintf(name, sizeof(name),
                       "%s.attn.compressor.norm.weight", prefix);
        rc = coverage_vector(builder, name,
                             YVEX_DEEPSEEK_TENSOR_COLLECTION_COMPRESSOR,
                             scope, layer->layer_index,
                             YVEX_DEEPSEEK_TENSOR_NO_INDEX,
                             YVEX_NATIVE_DTYPE_BF16,
                             layer->tensors.compressor_norm_width);
        if (rc != YVEX_OK) return rc;
        (void)snprintf(name, sizeof(name),
                       "%s.attn.compressor.wgate.weight", prefix);
        rc = coverage_matrix(builder, name,
                             YVEX_DEEPSEEK_TENSOR_COLLECTION_COMPRESSOR,
                             scope, layer->layer_index,
                             YVEX_DEEPSEEK_TENSOR_NO_INDEX,
                             YVEX_NATIVE_DTYPE_BF16,
                             layer->tensors.compressor_projection_rows,
                             layer->tensors.compressor_projection_columns);
        if (rc != YVEX_OK) return rc;
        (void)snprintf(name, sizeof(name),
                       "%s.attn.compressor.wkv.weight", prefix);
        rc = coverage_matrix(builder, name,
                             YVEX_DEEPSEEK_TENSOR_COLLECTION_COMPRESSOR,
                             scope, layer->layer_index,
                             YVEX_DEEPSEEK_TENSOR_NO_INDEX,
                             YVEX_NATIVE_DTYPE_BF16,
                             layer->tensors.compressor_projection_rows,
                             layer->tensors.compressor_projection_columns);
        if (rc != YVEX_OK) return rc;
    }
    if (layer->indexer_required) {
        (void)snprintf(name, sizeof(name),
                       "%s.attn.indexer.compressor.ape", prefix);
        rc = coverage_matrix(builder, name,
                             YVEX_DEEPSEEK_TENSOR_COLLECTION_INDEXER, scope,
                             layer->layer_index, YVEX_DEEPSEEK_TENSOR_NO_INDEX,
                             YVEX_NATIVE_DTYPE_F32,
                             layer->tensors.indexer_ape_rows,
                             layer->tensors.indexer_ape_columns);
        if (rc != YVEX_OK) return rc;
        (void)snprintf(name, sizeof(name),
                       "%s.attn.indexer.compressor.norm.weight", prefix);
        rc = coverage_vector(builder, name,
                             YVEX_DEEPSEEK_TENSOR_COLLECTION_INDEXER, scope,
                             layer->layer_index, YVEX_DEEPSEEK_TENSOR_NO_INDEX,
                             YVEX_NATIVE_DTYPE_BF16,
                             layer->tensors.indexer_norm_width);
        if (rc != YVEX_OK) return rc;
        (void)snprintf(name, sizeof(name),
                       "%s.attn.indexer.compressor.wgate.weight", prefix);
        rc = coverage_matrix(builder, name,
                             YVEX_DEEPSEEK_TENSOR_COLLECTION_INDEXER, scope,
                             layer->layer_index, YVEX_DEEPSEEK_TENSOR_NO_INDEX,
                             YVEX_NATIVE_DTYPE_BF16,
                             layer->tensors.indexer_projection_rows,
                             layer->tensors.indexer_projection_columns);
        if (rc != YVEX_OK) return rc;
        (void)snprintf(name, sizeof(name),
                       "%s.attn.indexer.compressor.wkv.weight", prefix);
        rc = coverage_matrix(builder, name,
                             YVEX_DEEPSEEK_TENSOR_COLLECTION_INDEXER, scope,
                             layer->layer_index, YVEX_DEEPSEEK_TENSOR_NO_INDEX,
                             YVEX_NATIVE_DTYPE_BF16,
                             layer->tensors.indexer_projection_rows,
                             layer->tensors.indexer_projection_columns);
        if (rc != YVEX_OK) return rc;
        (void)snprintf(base, sizeof(base), "%s.attn.indexer.wq_b", prefix);
        rc = coverage_fp8_pair(builder, base,
                               YVEX_DEEPSEEK_TENSOR_COLLECTION_INDEXER, scope,
                               layer->layer_index,
                               YVEX_DEEPSEEK_TENSOR_NO_INDEX,
                               layer->tensors.indexer_query_rows,
                               layer->tensors.indexer_query_columns, storage);
        if (rc != YVEX_OK) return rc;
        (void)snprintf(name, sizeof(name),
                       "%s.attn.indexer.weights_proj.weight", prefix);
        rc = coverage_matrix(builder, name,
                             YVEX_DEEPSEEK_TENSOR_COLLECTION_INDEXER, scope,
                             layer->layer_index, YVEX_DEEPSEEK_TENSOR_NO_INDEX,
                             YVEX_NATIVE_DTYPE_BF16,
                             layer->tensors.indexer_weight_rows,
                             layer->tensors.indexer_weight_columns);
        if (rc != YVEX_OK) return rc;
    }
    return YVEX_OK;
}

/*
 * Derives router, routed-expert, and shared-expert obligations from one layer
 * IR. Expert iteration is bounded by validated IR counts and never rescans
 * source headers.
 */
static int coverage_moe(coverage_builder *builder,
                        const char *prefix,
                        const yvex_deepseek_v4_layer_spec *layer,
                        yvex_deepseek_tensor_scope scope,
                        const yvex_deepseek_v4_source_constraint *storage,
                        unsigned long long hidden_size)
{
    char base[256];
    char name[256];
    unsigned long long expert;
    int rc;

    for (expert = 0u; expert < layer->moe.routed_experts; ++expert) {
        (void)snprintf(base, sizeof(base), "%s.ffn.experts.%llu.w1", prefix,
                       expert);
        rc = coverage_fp4_pair(builder, base, scope, layer->layer_index,
                               expert, layer->moe.expert_intermediate_size,
                               hidden_size, storage);
        if (rc != YVEX_OK) return rc;
        (void)snprintf(base, sizeof(base), "%s.ffn.experts.%llu.w2", prefix,
                       expert);
        rc = coverage_fp4_pair(builder, base, scope, layer->layer_index,
                               expert, hidden_size,
                               layer->moe.expert_intermediate_size, storage);
        if (rc != YVEX_OK) return rc;
        (void)snprintf(base, sizeof(base), "%s.ffn.experts.%llu.w3", prefix,
                       expert);
        rc = coverage_fp4_pair(builder, base, scope, layer->layer_index,
                               expert, layer->moe.expert_intermediate_size,
                               hidden_size, storage);
        if (rc != YVEX_OK) return rc;
    }
    (void)snprintf(base, sizeof(base), "%s.ffn.shared_experts.w1", prefix);
    rc = coverage_fp8_pair(builder, base,
                           YVEX_DEEPSEEK_TENSOR_COLLECTION_SHARED_EXPERT,
                           scope, layer->layer_index,
                           YVEX_DEEPSEEK_TENSOR_NO_INDEX,
                           layer->moe.shared_intermediate_size, hidden_size,
                           storage);
    if (rc != YVEX_OK) return rc;
    (void)snprintf(base, sizeof(base), "%s.ffn.shared_experts.w2", prefix);
    rc = coverage_fp8_pair(builder, base,
                           YVEX_DEEPSEEK_TENSOR_COLLECTION_SHARED_EXPERT,
                           scope, layer->layer_index,
                           YVEX_DEEPSEEK_TENSOR_NO_INDEX, hidden_size,
                           layer->moe.shared_intermediate_size, storage);
    if (rc != YVEX_OK) return rc;
    (void)snprintf(base, sizeof(base), "%s.ffn.shared_experts.w3", prefix);
    rc = coverage_fp8_pair(builder, base,
                           YVEX_DEEPSEEK_TENSOR_COLLECTION_SHARED_EXPERT,
                           scope, layer->layer_index,
                           YVEX_DEEPSEEK_TENSOR_NO_INDEX,
                           layer->moe.shared_intermediate_size, hidden_size,
                           storage);
    if (rc != YVEX_OK) return rc;
    (void)snprintf(name, sizeof(name), "%s.ffn.gate.weight", prefix);
    rc = coverage_matrix(builder, name,
                         YVEX_DEEPSEEK_TENSOR_COLLECTION_ROUTER, scope,
                         layer->layer_index, YVEX_DEEPSEEK_TENSOR_NO_INDEX,
                         YVEX_NATIVE_DTYPE_BF16, layer->moe.routed_experts,
                         hidden_size);
    if (rc != YVEX_OK) return rc;
    if (layer->moe.router_class == YVEX_DEEPSEEK_V4_ROUTER_HASH_TOKEN_ID) {
        (void)snprintf(name, sizeof(name), "%s.ffn.gate.tid2eid", prefix);
        return coverage_matrix(builder, name,
                               YVEX_DEEPSEEK_TENSOR_COLLECTION_ROUTER, scope,
                               layer->layer_index,
                               YVEX_DEEPSEEK_TENSOR_NO_INDEX,
                               YVEX_NATIVE_DTYPE_I64,
                               layer->moe.hash_table_rows,
                               layer->moe.hash_table_columns);
    }
    (void)snprintf(name, sizeof(name), "%s.ffn.gate.bias", prefix);
    return coverage_vector(builder, name,
                           YVEX_DEEPSEEK_TENSOR_COLLECTION_ROUTER, scope,
                           layer->layer_index,
                           YVEX_DEEPSEEK_TENSOR_NO_INDEX,
                           YVEX_NATIVE_DTYPE_F32,
                           layer->moe.correction_bias_width);
}

static int coverage_layer(coverage_builder *builder,
                          const char *prefix,
                          const yvex_deepseek_v4_layer_spec *layer,
                          yvex_deepseek_tensor_scope scope,
                          const yvex_deepseek_v4_model_spec *model)
{
    char name[256];
    int rc = coverage_attention(builder, prefix, layer, scope,
                                &model->source_constraint);
    if (rc != YVEX_OK) return rc;
    (void)snprintf(name, sizeof(name), "%s.attn_norm.weight", prefix);
    rc = coverage_vector(builder, name,
                         YVEX_DEEPSEEK_TENSOR_COLLECTION_NORM, scope,
                         layer->layer_index, YVEX_DEEPSEEK_TENSOR_NO_INDEX,
                         YVEX_NATIVE_DTYPE_BF16,
                         layer->attention_input_norm.width);
    if (rc != YVEX_OK) return rc;
    (void)snprintf(name, sizeof(name), "%s.ffn_norm.weight", prefix);
    rc = coverage_vector(builder, name,
                         YVEX_DEEPSEEK_TENSOR_COLLECTION_NORM, scope,
                         layer->layer_index, YVEX_DEEPSEEK_TENSOR_NO_INDEX,
                         YVEX_NATIVE_DTYPE_BF16,
                         layer->post_attention_ffn_norm.width);
    if (rc != YVEX_OK) return rc;
    rc = coverage_mhc(builder, prefix, "attn", scope, layer->layer_index,
                      &layer->mhc);
    if (rc != YVEX_OK) return rc;
    rc = coverage_mhc(builder, prefix, "ffn", scope, layer->layer_index,
                      &layer->mhc);
    if (rc != YVEX_OK) return rc;
    return coverage_moe(builder, prefix, layer, scope,
                        &model->source_constraint, model->hidden_size);
}

/*
 * Enumerates the complete global, 43-layer, and auxiliary/MTP requirement set
 * in deterministic order. It mutates only the unpublished builder and stops
 * at the first refusal.
 */
static int coverage_build_requirements(coverage_builder *builder,
                                       const yvex_deepseek_v4_ir *ir)
{
    const yvex_deepseek_v4_model_spec *model = yvex_deepseek_v4_ir_model(ir);
    unsigned long long layer_index;
    char prefix[64];
    int rc;

    rc = coverage_matrix(builder, "embed.weight",
                         YVEX_DEEPSEEK_TENSOR_COLLECTION_GLOBAL,
                         YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL,
                         YVEX_DEEPSEEK_TENSOR_NO_INDEX,
                         YVEX_DEEPSEEK_TENSOR_NO_INDEX,
                         YVEX_NATIVE_DTYPE_BF16,
                         model->embedding.vocabulary_size,
                         model->embedding.hidden_size);
    if (rc != YVEX_OK) return rc;
    rc = coverage_vector(builder, "norm.weight",
                         YVEX_DEEPSEEK_TENSOR_COLLECTION_GLOBAL,
                         YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL,
                         YVEX_DEEPSEEK_TENSOR_NO_INDEX,
                         YVEX_DEEPSEEK_TENSOR_NO_INDEX,
                         YVEX_NATIVE_DTYPE_BF16, model->hidden_size);
    if (rc != YVEX_OK) return rc;
    rc = coverage_matrix(builder, "head.weight",
                         YVEX_DEEPSEEK_TENSOR_COLLECTION_GLOBAL,
                         YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL,
                         YVEX_DEEPSEEK_TENSOR_NO_INDEX,
                         YVEX_DEEPSEEK_TENSOR_NO_INDEX,
                         YVEX_NATIVE_DTYPE_BF16,
                         model->output.vocabulary_size,
                         model->output.input_width);
    if (rc != YVEX_OK) return rc;
    rc = coverage_mhc_head(builder, "",
                           YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL,
                           YVEX_DEEPSEEK_TENSOR_NO_INDEX,
                           &model->final_mhc_head,
                           YVEX_DEEPSEEK_TENSOR_COLLECTION_GLOBAL);
    if (rc != YVEX_OK) return rc;
    for (layer_index = 0u; layer_index < model->main_layer_count;
         ++layer_index) {
        const yvex_deepseek_v4_layer_spec *layer =
            yvex_deepseek_v4_ir_layer_at(ir, layer_index);
        (void)snprintf(prefix, sizeof(prefix), "layers.%llu", layer_index);
        rc = coverage_layer(builder, prefix, layer,
                            YVEX_DEEPSEEK_TENSOR_SCOPE_MAIN_LAYER, model);
        if (rc != YVEX_OK) return rc;
    }
    for (layer_index = 0u; layer_index < model->auxiliary_layer_count;
         ++layer_index) {
        const yvex_deepseek_v4_auxiliary_spec *aux =
            yvex_deepseek_v4_ir_auxiliary_at(ir, layer_index);
        char name[256];
        char base[256];

        (void)snprintf(prefix, sizeof(prefix), "mtp.%llu", layer_index);
        rc = coverage_layer(builder, prefix, &aux->layer,
                            YVEX_DEEPSEEK_TENSOR_SCOPE_MTP, model);
        if (rc != YVEX_OK) return rc;
        (void)snprintf(base, sizeof(base), "%s.e_proj", prefix);
        rc = coverage_fp8_pair(builder, base,
                               YVEX_DEEPSEEK_TENSOR_COLLECTION_AUXILIARY,
                               YVEX_DEEPSEEK_TENSOR_SCOPE_MTP,
                               aux->layer.layer_index,
                               YVEX_DEEPSEEK_TENSOR_NO_INDEX,
                               aux->embedding_projection_output,
                               aux->embedding_projection_input,
                               &model->source_constraint);
        if (rc != YVEX_OK) return rc;
        (void)snprintf(base, sizeof(base), "%s.h_proj", prefix);
        rc = coverage_fp8_pair(builder, base,
                               YVEX_DEEPSEEK_TENSOR_COLLECTION_AUXILIARY,
                               YVEX_DEEPSEEK_TENSOR_SCOPE_MTP,
                               aux->layer.layer_index,
                               YVEX_DEEPSEEK_TENSOR_NO_INDEX,
                               aux->hidden_projection_output,
                               aux->hidden_projection_input,
                               &model->source_constraint);
        if (rc != YVEX_OK) return rc;
        (void)snprintf(name, sizeof(name), "%s.enorm.weight", prefix);
        rc = coverage_vector(builder, name,
                             YVEX_DEEPSEEK_TENSOR_COLLECTION_AUXILIARY,
                             YVEX_DEEPSEEK_TENSOR_SCOPE_MTP,
                             aux->layer.layer_index,
                             YVEX_DEEPSEEK_TENSOR_NO_INDEX,
                             YVEX_NATIVE_DTYPE_BF16,
                             aux->embedding_projection_input);
        if (rc != YVEX_OK) return rc;
        (void)snprintf(name, sizeof(name), "%s.hnorm.weight", prefix);
        rc = coverage_vector(builder, name,
                             YVEX_DEEPSEEK_TENSOR_COLLECTION_AUXILIARY,
                             YVEX_DEEPSEEK_TENSOR_SCOPE_MTP,
                             aux->layer.layer_index,
                             YVEX_DEEPSEEK_TENSOR_NO_INDEX,
                             YVEX_NATIVE_DTYPE_BF16,
                             aux->hidden_projection_input);
        if (rc != YVEX_OK) return rc;
        (void)snprintf(name, sizeof(name), "%s.norm.weight", prefix);
        rc = coverage_vector(builder, name,
                             YVEX_DEEPSEEK_TENSOR_COLLECTION_AUXILIARY,
                             YVEX_DEEPSEEK_TENSOR_SCOPE_MTP,
                             aux->layer.layer_index,
                             YVEX_DEEPSEEK_TENSOR_NO_INDEX,
                             YVEX_NATIVE_DTYPE_BF16, model->hidden_size);
        if (rc != YVEX_OK) return rc;
        (void)snprintf(base, sizeof(base), "%s.", prefix);
        rc = coverage_mhc_head(
            builder, base, YVEX_DEEPSEEK_TENSOR_SCOPE_MTP,
            aux->layer.layer_index, &aux->mhc_head,
            YVEX_DEEPSEEK_TENSOR_COLLECTION_AUXILIARY);
        if (rc != YVEX_OK) return rc;
    }
    return YVEX_OK;
}

/* Parses one decimal name segment and distinguishes arithmetic overflow. */
static int coverage_parse_name_index(const char *text,
                                     unsigned long long *value,
                                     const char **end)
{
    unsigned long long parsed = 0u;
    const char *cursor = text;

    if (!cursor || *cursor < '0' || *cursor > '9') return 0;
    while (*cursor >= '0' && *cursor <= '9') {
        unsigned long long digit = (unsigned long long)(*cursor - '0');
        if (parsed > (ULLONG_MAX - digit) / 10u) return -1;
        parsed = parsed * 10u + digit;
        cursor++;
    }
    *value = parsed;
    *end = cursor;
    return 1;
}

/* Classifies out-of-range structured names before generic unexpected refusal. */
static int coverage_reject_unexpected(coverage_builder *builder,
                                      const yvex_deepseek_v4_ir *ir,
                                      const yvex_native_weight_info *source)
{
    const yvex_deepseek_v4_model_spec *model =
        yvex_deepseek_v4_ir_model(ir);
    const yvex_deepseek_v4_layer_spec *layer_spec = NULL;
    const yvex_deepseek_v4_auxiliary_spec *aux =
        yvex_deepseek_v4_ir_auxiliary_at(ir, 0u);
    yvex_deepseek_tensor_scope scope = YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL;
    unsigned long long layer = YVEX_DEEPSEEK_TENSOR_NO_INDEX;
    unsigned long long expert = YVEX_DEEPSEEK_TENSOR_NO_INDEX;
    const char *tail = NULL;
    const char *expert_text;
    int parsed;

    if (!source || !source->name || !model) {
        return coverage_reject(
            builder->failure,
            YVEX_DEEPSEEK_COVERAGE_FAILURE_UNEXPECTED_SOURCE,
            YVEX_DEEPSEEK_TENSOR_COLLECTION_GLOBAL, scope, "unknown",
            layer, expert, YVEX_DEEPSEEK_TENSOR_NO_INDEX, 0u, 1u,
            builder->err);
    }
    if (strncmp(source->name, "layers.", 7u) == 0) {
        scope = YVEX_DEEPSEEK_TENSOR_SCOPE_MAIN_LAYER;
        parsed = coverage_parse_name_index(source->name + 7u, &layer, &tail);
        if (parsed < 0) goto arithmetic_overflow;
        if (parsed > 0 && *tail == '.' && layer >= model->main_layer_count) {
            return coverage_reject(
                builder->failure,
                YVEX_DEEPSEEK_COVERAGE_FAILURE_INVALID_INDEX,
                YVEX_DEEPSEEK_TENSOR_COLLECTION_GLOBAL, scope, source->name,
                layer, expert, YVEX_DEEPSEEK_TENSOR_NO_INDEX,
                model->main_layer_count - 1u, layer, builder->err);
        }
        if (parsed > 0 && *tail == '.')
            layer_spec = yvex_deepseek_v4_ir_layer_at(ir, layer);
    } else if (strncmp(source->name, "mtp.", 4u) == 0) {
        unsigned long long predictor = 0u;
        scope = YVEX_DEEPSEEK_TENSOR_SCOPE_MTP;
        parsed = coverage_parse_name_index(source->name + 4u,
                                           &predictor, &tail);
        if (parsed < 0) goto arithmetic_overflow;
        layer = aux ? aux->layer.layer_index : YVEX_DEEPSEEK_TENSOR_NO_INDEX;
        if (parsed > 0 && *tail == '.' &&
            (!aux || predictor != aux->predictor_index)) {
            return coverage_reject(
                builder->failure,
                YVEX_DEEPSEEK_COVERAGE_FAILURE_INVALID_INDEX,
                YVEX_DEEPSEEK_TENSOR_COLLECTION_AUXILIARY, scope,
                source->name, layer, predictor,
                YVEX_DEEPSEEK_TENSOR_NO_INDEX,
                aux ? aux->predictor_index : 0u, predictor, builder->err);
        }
        if (parsed > 0 && *tail == '.' && aux) layer_spec = &aux->layer;
    }
    expert_text = tail ? strstr(tail, ".ffn.experts.") : NULL;
    if (expert_text && layer_spec) {
        parsed = coverage_parse_name_index(
            expert_text + strlen(".ffn.experts."), &expert, &tail);
        if (parsed < 0) goto arithmetic_overflow;
        if (parsed > 0 && *tail == '.' &&
            expert >= layer_spec->moe.routed_experts) {
            return coverage_reject(
                builder->failure,
                YVEX_DEEPSEEK_COVERAGE_FAILURE_INVALID_INDEX,
                YVEX_DEEPSEEK_TENSOR_COLLECTION_ROUTED_EXPERT, scope,
                source->name, layer, expert,
                YVEX_DEEPSEEK_TENSOR_NO_INDEX,
                layer_spec->moe.routed_experts - 1u, expert, builder->err);
        }
    }
    return coverage_reject(
        builder->failure, YVEX_DEEPSEEK_COVERAGE_FAILURE_UNEXPECTED_SOURCE,
        YVEX_DEEPSEEK_TENSOR_COLLECTION_GLOBAL, scope, source->name,
        layer, expert, YVEX_DEEPSEEK_TENSOR_NO_INDEX, 0u, 1u, builder->err);

arithmetic_overflow:
    return coverage_reject(
        builder->failure,
        YVEX_DEEPSEEK_COVERAGE_FAILURE_ARITHMETIC_OVERFLOW,
        YVEX_DEEPSEEK_TENSOR_COLLECTION_GLOBAL, scope, source->name,
        layer, expert, YVEX_DEEPSEEK_TENSOR_NO_INDEX, ULLONG_MAX, 0u,
        builder->err);
}

static int coverage_validate_inputs(
    const yvex_source_verification *verification,
    const yvex_deepseek_v4_ir *ir,
    yvex_source_tensor_snapshot *snapshot,
    yvex_source_tensor_snapshot_facts *facts,
    yvex_deepseek_tensor_coverage_failure *failure,
    yvex_error *err)
{
    const yvex_model_target_identity *identity =
        yvex_model_target_release_identity();
    const yvex_deepseek_v4_model_spec *model = yvex_deepseek_v4_ir_model(ir);

    if (!verification || !ir || !snapshot || !model) {
        return coverage_reject(
            failure, YVEX_DEEPSEEK_COVERAGE_FAILURE_INVALID_ARGUMENT,
            YVEX_DEEPSEEK_TENSOR_COLLECTION_GLOBAL,
            YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL, "coverage-input",
            YVEX_DEEPSEEK_TENSOR_NO_INDEX, YVEX_DEEPSEEK_TENSOR_NO_INDEX,
            YVEX_DEEPSEEK_TENSOR_NO_INDEX, 1u, 0u, err);
    }
    if (!verification->verified || verification->blocker_count != 0u ||
        strcmp(verification->manifest_target_id, identity->target_id) != 0 ||
        strcmp(verification->repository_id, identity->upstream_repo_id) != 0 ||
        strcmp(verification->revision, identity->upstream_revision) != 0 ||
        strcmp(model->target_id, identity->target_id) != 0 ||
        strcmp(model->revision, identity->upstream_revision) != 0) {
        return coverage_reject(
            failure, YVEX_DEEPSEEK_COVERAGE_FAILURE_SOURCE_IDENTITY,
            YVEX_DEEPSEEK_TENSOR_COLLECTION_GLOBAL,
            YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL, "pinned-source-identity",
            YVEX_DEEPSEEK_TENSOR_NO_INDEX, YVEX_DEEPSEEK_TENSOR_NO_INDEX,
            YVEX_DEEPSEEK_TENSOR_NO_INDEX, 1u, 0u, err);
    }
    if (strcmp(verification->inventory_authority, "upstream-index") != 0 ||
        !verification->upstream_index_identity_verified ||
        strcmp(verification->upstream_index_oid,
               identity->upstream_index_oid) != 0 ||
        strcmp(verification->local_index_oid,
               identity->upstream_index_oid) != 0) {
        return coverage_reject(
            failure, YVEX_DEEPSEEK_COVERAGE_FAILURE_INVENTORY_AUTHORITY,
            YVEX_DEEPSEEK_TENSOR_COLLECTION_GLOBAL,
            YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL, "pinned-upstream-index",
            YVEX_DEEPSEEK_TENSOR_NO_INDEX, YVEX_DEEPSEEK_TENSOR_NO_INDEX,
            YVEX_DEEPSEEK_TENSOR_NO_INDEX, 1u, 0u, err);
    }
    if (yvex_source_tensor_snapshot_facts_get(snapshot, facts, err) != YVEX_OK)
        return yvex_error_code(err);
    if (facts->tensor_count != verification->header_tensor_count ||
        facts->shard_count != verification->header_shard_count ||
        facts->header_scan_count != verification->header_scan_count ||
        facts->payload_bytes_read != 0u) {
        return coverage_reject(
            failure, YVEX_DEEPSEEK_COVERAGE_FAILURE_INVENTORY_DRIFT,
            YVEX_DEEPSEEK_TENSOR_COLLECTION_GLOBAL,
            YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL, "snapshot-verification-facts",
            YVEX_DEEPSEEK_TENSOR_NO_INDEX, YVEX_DEEPSEEK_TENSOR_NO_INDEX,
            YVEX_DEEPSEEK_TENSOR_NO_INDEX, verification->header_tensor_count,
            facts->tensor_count, err);
    }
    if (model->main_layer_count != 43u || model->auxiliary_layer_count != 1u ||
        model->source_constraint.quant_block_rows != 128u ||
        model->source_constraint.quant_block_columns != 128u ||
        model->source_constraint.fp4_packing_factor != 2u ||
        model->source_constraint.fp4_scale_group_width != 32u ||
        model->source_constraint.scale_dtype != YVEX_NATIVE_DTYPE_F8_E8M0 ||
        !model->final_mhc_head.required) {
        return coverage_reject(
            failure, YVEX_DEEPSEEK_COVERAGE_FAILURE_ARCHITECTURE_INCOMPLETE,
            YVEX_DEEPSEEK_TENSOR_COLLECTION_GLOBAL,
            YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL, "tensor-relevant-ir",
            YVEX_DEEPSEEK_TENSOR_NO_INDEX, YVEX_DEEPSEEK_TENSOR_NO_INDEX,
            YVEX_DEEPSEEK_TENSOR_NO_INDEX, 1u, 0u, err);
    }
    return YVEX_OK;
}

/*
 * Allocates and publishes an immutable result only after one-to-one
 * reconciliation. The result retains the snapshot; every partial allocation
 * and retained reference is released on failure.
 */
int yvex_deepseek_tensor_coverage_build(
    yvex_deepseek_tensor_coverage **out,
    const yvex_source_verification *verification,
    const yvex_deepseek_v4_ir *ir,
    yvex_source_tensor_snapshot *snapshot,
    const yvex_deepseek_tensor_coverage_options *options,
    yvex_deepseek_tensor_coverage_failure *failure,
    yvex_error *err)
{
    yvex_deepseek_tensor_coverage_options actual;
    yvex_source_tensor_snapshot_facts source_facts;
    yvex_deepseek_tensor_coverage *coverage;
    coverage_builder builder;
    unsigned long long index;
    unsigned long long hash = 1469598103934665603ull;
    int rc;

    if (!out) return coverage_reject(
        failure, YVEX_DEEPSEEK_COVERAGE_FAILURE_INVALID_ARGUMENT,
        YVEX_DEEPSEEK_TENSOR_COLLECTION_GLOBAL,
        YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL, "output",
        YVEX_DEEPSEEK_TENSOR_NO_INDEX, YVEX_DEEPSEEK_TENSOR_NO_INDEX,
        YVEX_DEEPSEEK_TENSOR_NO_INDEX, 1u, 0u, err);
    *out = NULL;
    coverage_failure_clear(failure);
    memset(&source_facts, 0, sizeof(source_facts));
    rc = coverage_validate_inputs(verification, ir, snapshot, &source_facts,
                                  failure, err);
    if (rc != YVEX_OK) return rc;
    actual.allocate = coverage_allocate;
    actual.release = coverage_release;
    actual.context = NULL;
    actual.maximum_tensors = DEEPSEEK_COVERAGE_DEFAULT_LIMIT;
    if (options) {
        if ((options->allocate == NULL) != (options->release == NULL)) {
            return coverage_reject(
                failure, YVEX_DEEPSEEK_COVERAGE_FAILURE_INVALID_ARGUMENT,
                YVEX_DEEPSEEK_TENSOR_COLLECTION_GLOBAL,
                YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL, "allocator",
                YVEX_DEEPSEEK_TENSOR_NO_INDEX,
                YVEX_DEEPSEEK_TENSOR_NO_INDEX,
                YVEX_DEEPSEEK_TENSOR_NO_INDEX, 1u, 0u, err);
        }
        if (options->allocate) {
            actual.allocate = options->allocate;
            actual.release = options->release;
            actual.context = options->context;
        }
        if (options->maximum_tensors)
            actual.maximum_tensors = options->maximum_tensors;
    }
    if (source_facts.tensor_count > actual.maximum_tensors ||
        source_facts.tensor_count > (unsigned long long)(SIZE_MAX /
                                     sizeof(yvex_deepseek_tensor_coverage_row))) {
        return coverage_reject(
            failure, YVEX_DEEPSEEK_COVERAGE_FAILURE_RESOURCE_LIMIT,
            YVEX_DEEPSEEK_TENSOR_COLLECTION_GLOBAL,
            YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL, "tensor-count",
            YVEX_DEEPSEEK_TENSOR_NO_INDEX, YVEX_DEEPSEEK_TENSOR_NO_INDEX,
            YVEX_DEEPSEEK_TENSOR_NO_INDEX, actual.maximum_tensors,
            source_facts.tensor_count, err);
    }
    coverage = (yvex_deepseek_tensor_coverage *)actual.allocate(
        sizeof(*coverage), actual.context);
    if (!coverage) return coverage_reject(
        failure, YVEX_DEEPSEEK_COVERAGE_FAILURE_ALLOCATION,
        YVEX_DEEPSEEK_TENSOR_COLLECTION_GLOBAL,
        YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL, "coverage",
        YVEX_DEEPSEEK_TENSOR_NO_INDEX, YVEX_DEEPSEEK_TENSOR_NO_INDEX,
        YVEX_DEEPSEEK_TENSOR_NO_INDEX, sizeof(*coverage), 0u, err);
    memset(coverage, 0, sizeof(*coverage));
    coverage->options = actual;
    coverage->rows = (yvex_deepseek_tensor_coverage_row *)actual.allocate(
        (size_t)source_facts.tensor_count * sizeof(coverage->rows[0]),
        actual.context);
    coverage->row_by_source =
        (const yvex_deepseek_tensor_coverage_row **)actual.allocate(
            (size_t)source_facts.tensor_count *
                sizeof(coverage->row_by_source[0]), actual.context);
    builder.matched = (unsigned char *)actual.allocate(
        (size_t)source_facts.tensor_count, actual.context);
    if (!coverage->rows || !coverage->row_by_source || !builder.matched) {
        if (builder.matched) actual.release(builder.matched, actual.context);
        yvex_deepseek_tensor_coverage_close(coverage);
        return coverage_reject(
            failure, YVEX_DEEPSEEK_COVERAGE_FAILURE_ALLOCATION,
            YVEX_DEEPSEEK_TENSOR_COLLECTION_GLOBAL,
            YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL, "coverage-tables",
            YVEX_DEEPSEEK_TENSOR_NO_INDEX, YVEX_DEEPSEEK_TENSOR_NO_INDEX,
            YVEX_DEEPSEEK_TENSOR_NO_INDEX, source_facts.tensor_count, 0u, err);
    }
    memset(coverage->rows, 0, (size_t)source_facts.tensor_count *
                                    sizeof(coverage->rows[0]));
    memset(coverage->row_by_source, 0, (size_t)source_facts.tensor_count *
                                             sizeof(coverage->row_by_source[0]));
    memset(builder.matched, 0, (size_t)source_facts.tensor_count);
    coverage->snapshot = snapshot;
    yvex_source_tensor_snapshot_retain(snapshot);
    coverage->summary.source_tensor_count = source_facts.tensor_count;
    coverage->summary.main_layer_count = 43u;
    coverage->summary.auxiliary_layer_count = 1u;
    coverage->summary.header_scan_count = source_facts.header_scan_count;
    coverage->summary.payload_bytes_read = source_facts.payload_bytes_read;
    coverage->summary.source_identity = source_facts.identity;
    builder.coverage = coverage;
    builder.failure = failure;
    builder.err = err;
    rc = coverage_build_requirements(&builder, ir);
    if (rc == YVEX_OK) {
        for (index = 0u; index < source_facts.tensor_count; ++index) {
            if (!builder.matched[index]) {
                const yvex_native_weight_info *unexpected =
                    yvex_source_tensor_snapshot_at(snapshot, index);
                coverage->summary.unexpected_count++;
                rc = coverage_reject_unexpected(&builder, ir, unexpected);
                break;
            }
        }
    }
    actual.release(builder.matched, actual.context);
    if (rc != YVEX_OK) {
        yvex_deepseek_tensor_coverage_close(coverage);
        return rc;
    }
    if (coverage->summary.required_tensor_count != source_facts.tensor_count ||
        coverage->summary.matched_tensor_count != source_facts.tensor_count) {
        unsigned long long matched = coverage->summary.matched_tensor_count;
        yvex_deepseek_tensor_coverage_close(coverage);
        return coverage_reject(
            failure, YVEX_DEEPSEEK_COVERAGE_FAILURE_INVENTORY_DRIFT,
            YVEX_DEEPSEEK_TENSOR_COLLECTION_GLOBAL,
            YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL, "one-to-one-count",
            YVEX_DEEPSEEK_TENSOR_NO_INDEX, YVEX_DEEPSEEK_TENSOR_NO_INDEX,
            YVEX_DEEPSEEK_TENSOR_NO_INDEX, source_facts.tensor_count,
            matched, err);
    }
    hash = coverage_hash_u64(hash, source_facts.identity);
    for (index = 0u; index < coverage->summary.required_tensor_count; ++index) {
        const yvex_native_weight_info *source = coverage->rows[index].source;
        const char *collection = yvex_deepseek_tensor_collection_name(
            coverage->rows[index].collection);
        const char *scope = coverage_scope_name(coverage->rows[index].scope);
        hash = coverage_hash_bytes(hash, source->name, strlen(source->name) + 1u);
        hash = coverage_hash_bytes(hash, collection, strlen(collection) + 1u);
        hash = coverage_hash_bytes(hash, scope, strlen(scope) + 1u);
        hash = coverage_hash_u64(hash, coverage->rows[index].layer_index);
        hash = coverage_hash_u64(hash, coverage->rows[index].expert_index);
    }
    coverage->summary.routed_expert_count =
        coverage->summary.collection_counts[
            YVEX_DEEPSEEK_TENSOR_COLLECTION_ROUTED_EXPERT];
    coverage->summary.shared_expert_count =
        coverage->summary.collection_counts[
            YVEX_DEEPSEEK_TENSOR_COLLECTION_SHARED_EXPERT];
    if (yvex_source_tensor_snapshot_facts_get(snapshot, &source_facts, err) !=
        YVEX_OK) {
        yvex_deepseek_tensor_coverage_close(coverage);
        return yvex_error_code(err);
    }
    coverage->summary.source_lookup_count = source_facts.lookup_count;
    coverage->summary.source_collision_count = source_facts.collision_count;
    coverage->summary.source_maximum_probe = source_facts.maximum_probe;
    coverage->summary.coverage_identity = hash;
    coverage->summary.complete = 1;
    *out = coverage;
    yvex_error_clear(err);
    return YVEX_OK;
}

/*
 * Coordinates one strict header scan, one IR construction, and exact coverage.
 * Source IO remains in the source owner; all temporary owners are released
 * before return.
 */
int yvex_deepseek_tensor_coverage_open_verified_source(
    yvex_deepseek_tensor_coverage **out,
    yvex_source_verification *verification,
    const char *source_path,
    const char *models_root,
    yvex_deepseek_tensor_coverage_failure *failure,
    yvex_error *err)
{
    yvex_source_verify_options source_options;
    yvex_source_tensor_snapshot *snapshot = NULL;
    yvex_deepseek_v4_ir *ir = NULL;
    yvex_deepseek_v4_ir_failure ir_failure;
    int rc;

    if (!out || !verification || !source_path || !source_path[0]) {
        return coverage_reject(
            failure, YVEX_DEEPSEEK_COVERAGE_FAILURE_INVALID_ARGUMENT,
            YVEX_DEEPSEEK_TENSOR_COLLECTION_GLOBAL,
            YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL, "verified-source-path",
            YVEX_DEEPSEEK_TENSOR_NO_INDEX, YVEX_DEEPSEEK_TENSOR_NO_INDEX,
            YVEX_DEEPSEEK_TENSOR_NO_INDEX, 1u, 0u, err);
    }
    *out = NULL;
    memset(&source_options, 0, sizeof(source_options));
    source_options.identity = yvex_model_target_release_identity();
    source_options.source_path = source_path;
    source_options.models_root = models_root && models_root[0]
                                     ? models_root
                                     : "models";
    source_options.promote_manifest = 0;
    rc = yvex_source_verify_with_snapshot(&source_options, verification,
                                          &snapshot, err);
    if (rc != YVEX_OK) goto cleanup;
    if (!verification->verified || !snapshot) {
        rc = coverage_reject(
            failure, YVEX_DEEPSEEK_COVERAGE_FAILURE_SOURCE_IDENTITY,
            YVEX_DEEPSEEK_TENSOR_COLLECTION_GLOBAL,
            YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL, "strict-source-verification",
            YVEX_DEEPSEEK_TENSOR_NO_INDEX, YVEX_DEEPSEEK_TENSOR_NO_INDEX,
            YVEX_DEEPSEEK_TENSOR_NO_INDEX, 1u, 0u, err);
        goto cleanup;
    }
    rc = yvex_deepseek_v4_ir_build(&ir, verification, &ir_failure, err);
    if (rc != YVEX_OK) {
        rc = coverage_reject(
            failure, YVEX_DEEPSEEK_COVERAGE_FAILURE_ARCHITECTURE_INCOMPLETE,
            YVEX_DEEPSEEK_TENSOR_COLLECTION_GLOBAL,
            YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL,
            ir_failure.field ? ir_failure.field : "architecture-ir",
            ir_failure.layer_index, YVEX_DEEPSEEK_TENSOR_NO_INDEX,
            YVEX_DEEPSEEK_TENSOR_NO_INDEX, ir_failure.expected,
            ir_failure.actual, err);
        goto cleanup;
    }
    rc = yvex_deepseek_tensor_coverage_build(
        out, verification, ir, snapshot, NULL, failure, err);
cleanup:
    yvex_deepseek_v4_ir_close(ir);
    yvex_source_tensor_snapshot_release(snapshot);
    return rc;
}

/* Releases owned rows and the retained snapshot; null input is a no-op. */
void yvex_deepseek_tensor_coverage_close(
    yvex_deepseek_tensor_coverage *coverage)
{
    yvex_deepseek_tensor_coverage_options options;

    if (!coverage) return;
    options = coverage->options;
    yvex_source_tensor_snapshot_release(coverage->snapshot);
    if (coverage->row_by_source)
        options.release((void *)coverage->row_by_source, options.context);
    if (coverage->rows) options.release(coverage->rows, options.context);
    options.release(coverage, options.context);
}

const yvex_deepseek_tensor_coverage_summary *
yvex_deepseek_tensor_coverage_summary_get(
    const yvex_deepseek_tensor_coverage *coverage)
{
    return coverage ? &coverage->summary : NULL;
}

const yvex_deepseek_tensor_coverage_row *
yvex_deepseek_tensor_coverage_at(
    const yvex_deepseek_tensor_coverage *coverage,
    unsigned long long index)
{
    if (!coverage || index >= coverage->summary.required_tensor_count)
        return NULL;
    return &coverage->rows[index];
}

const yvex_deepseek_tensor_coverage_row *
yvex_deepseek_tensor_coverage_find(
    const yvex_deepseek_tensor_coverage *coverage,
    const char *source_name)
{
    unsigned long long index;

    if (!coverage || !source_name ||
        !yvex_source_tensor_snapshot_find_index(coverage->snapshot,
                                                source_name, &index))
        return NULL;
    return coverage->row_by_source[index];
}

/* Resolves a source name to its deterministic requirement-row index. */
int yvex_deepseek_tensor_coverage_find_index(
    const yvex_deepseek_tensor_coverage *coverage,
    const char *source_name,
    unsigned long long *row_index)
{
    unsigned long long source_index;
    const yvex_deepseek_tensor_coverage_row *row;

    if (!coverage || !source_name || !row_index || !coverage->rows ||
        !yvex_source_tensor_snapshot_find_index(coverage->snapshot,
                                                source_name, &source_index))
        return 0;
    row = coverage->row_by_source[source_index];
    if (!row) return 0;
    *row_index = (unsigned long long)(row - coverage->rows);
    return 1;
}

const char *yvex_deepseek_tensor_collection_name(
    yvex_deepseek_tensor_collection collection)
{
    static const char *names[] = {
        "global", "attention", "compressor", "indexer", "norm", "mhc",
        "router", "routed-expert", "shared-expert", "auxiliary"
    };
    return collection < YVEX_DEEPSEEK_TENSOR_COLLECTION_COUNT
               ? names[collection]
               : "unknown";
}

const char *yvex_deepseek_tensor_coverage_failure_name(
    yvex_deepseek_tensor_coverage_failure_code code)
{
    static const char *names[] = {
        "none", "invalid-argument", "wrong-source-identity",
        "invalid-inventory-authority", "inventory-drift",
        "architecture-incomplete", "missing-requirement", "ambiguous-match",
        "unexpected-source", "invalid-index", "rank-mismatch",
        "shape-mismatch", "dtype-mismatch",
        "scale-companion-mismatch", "arithmetic-overflow", "resource-limit",
        "allocation-failure"
    };
    return code <= YVEX_DEEPSEEK_COVERAGE_FAILURE_ALLOCATION
               ? names[code]
               : "unknown";
}
