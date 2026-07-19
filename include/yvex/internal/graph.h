/* Owner: graph family-recipe ABI.
 * Owns: normalized layer-plan geometry and immutable recipe projection.
 * Does not own: model numeric policy, family schedules, kernels, execution state, payload access, persistent KV, or
 *   generation.
 * Invariants: recipes copy admitted model facts without redefining their values.
 * Boundary: graph recipe admission is not attention execution support.
 * Purpose: decouple generic graph planning from family implementation types.
 * Inputs: model-owned numeric facts and one admitted family projection.
 * Effects: declarations only; no allocation, mutation, or I/O.
 * Failure: consumers reject unsupported combinations through their typed owner. */
#ifndef INCLUDE_YVEX_INTERNAL_GRAPH_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_GRAPH_H_INCLUDED

#include <yvex/internal/model.h>

#define YVEX_ATTENTION_NO_LAYER (~0ull)
#define YVEX_ATTENTION_NO_TENSOR_INDEX (~0ull)

typedef struct {
    unsigned long long layer_index;
    yvex_attention_class attention_class;
    unsigned long long compression_ratio;
    unsigned long long sliding_window;
    unsigned long long query_heads;
    unsigned long long kv_heads;
    unsigned long long head_dimension;
    unsigned long long rope_head_dimension;
    unsigned long long query_lora_rank;
    unsigned long long output_lora_rank;
    unsigned long long output_groups;
    unsigned long long hidden_dimension;
    unsigned long long indexer_heads;
    unsigned long long indexer_head_dimension;
    unsigned long long indexer_topk;
    unsigned long long compressor_ape_columns;
    unsigned long long indexer_ape_columns;
    double rms_norm_epsilon;
    int compressor_required;
    int indexer_required;
    yvex_attention_position_policy position;
    yvex_attention_activation_policy attention_kv_activation;
    yvex_attention_activation_policy compressor_activation;
    yvex_attention_activation_policy compressor_rotated_activation;
    yvex_attention_activation_policy indexer_query_activation;
    yvex_attention_topk_policy sparse_topk;
    unsigned long long required_binding_count;
    unsigned long long qtype_compute_refusal_count;
    unsigned long long payload_bytes_bound;
} yvex_attention_layer_plan;

typedef int (*yvex_attention_recipe_identity_fn)(
    const void *context, char output[65]);
typedef int (*yvex_attention_recipe_layer_fn)(
    const void *context, unsigned long long index,
    yvex_attention_layer_plan *output);

typedef struct {
    const void *context;
    unsigned long long layer_count;
    unsigned long long auxiliary_layer_count;
    unsigned long long swa_layer_count;
    unsigned long long csa_layer_count;
    unsigned long long hca_layer_count;
    yvex_attention_recipe_identity_fn identity;
    yvex_attention_recipe_layer_fn layer;
} yvex_attention_recipe;

#endif
