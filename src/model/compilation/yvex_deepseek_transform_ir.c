/*
 * yvex_deepseek_transform_ir.c - complete DeepSeek Transformation IR builder.
 *
 * Owner:
 *   src/model/compilation
 *
 * Owns:
 *   typed DeepSeek logical tensor keys, source-value registration, direct,
 *   scale-paired, checked-cast, and expert-aggregation operation construction.
 *
 * Does not own:
 *   source discovery or IO, GGUF naming/qtypes/layout, numerical execution,
 *   quantization, artifact writing, runtime state, rendering, or generation.
 *
 * Invariants:
 *   construction consumes admitted typed owners only, uses every covered
 *   source tensor exactly once, and seals only the exact target-scale graph.
 *
 * Boundary:
 *   operation metadata states required semantics without executing payloads.
 */
#include "yvex_deepseek_transform_ir.h"
#include "yvex_transform_ir_internal.h"

#include "yvex_sha256.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    yvex_transform_builder *builder;
    const yvex_source_verification *verification;
    const yvex_deepseek_v4_ir *architecture;
    const yvex_deepseek_v4_model_spec *model;
    const yvex_deepseek_tensor_coverage *coverage;
    const yvex_deepseek_tensor_coverage_summary *coverage_summary;
    yvex_transform_allocator temporary_allocator;
    yvex_transform_failure *failure;
    yvex_error *err;
    unsigned long long terminal_ordinal;
} deepseek_transform_builder;

static void *deepseek_default_allocate(size_t size, void *context)
{
    (void)context;
    return malloc(size);
}

static void deepseek_default_release(void *allocation, void *context)
{
    (void)context;
    free(allocation);
}

static int deepseek_identity_u64(yvex_sha256 *hash,
                                 unsigned long long value)
{
    unsigned char bytes[8];
    unsigned int index;

    for (index = 0u; index < 8u; ++index)
        bytes[7u - index] =
            (unsigned char)((value >> (index * 8u)) & 0xffu);
    return yvex_sha256_update(hash, bytes, sizeof(bytes));
}

static int deepseek_identity_text(yvex_sha256 *hash, const char *text)
{
    size_t length;

    if (!text) return 0;
    length = strlen(text);
    return deepseek_identity_u64(hash, (unsigned long long)length) &&
           yvex_sha256_update(hash, text, length);
}

static int deepseek_identity_double(yvex_sha256 *hash, double value)
{
    uint64_t bits;

    memcpy(&bits, &value, sizeof(bits));
    return deepseek_identity_u64(hash, (unsigned long long)bits);
}

/* Encodes the admitted logical architecture without native structure bytes. */
static int deepseek_architecture_identity(
    const yvex_deepseek_v4_ir *architecture,
    char output[YVEX_TRANSFORM_IR_IDENTITY_CAP])
{
    static const char domain[] = "yvex.logical-model.deepseek-v4-flash.v1";
    const yvex_deepseek_v4_model_spec *model =
        yvex_deepseek_v4_ir_model(architecture);
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    unsigned long long index;

    if (!model || !output) return 0;
    yvex_sha256_init(&hash);
#define ID_U64(value) do { if (!deepseek_identity_u64(                       \
        &hash, (unsigned long long)(value))) return 0; } while (0)
#define ID_TEXT(value) do { if (!deepseek_identity_text(&hash, (value)))     \
        return 0; } while (0)
#define ID_DOUBLE(value) do { if (!deepseek_identity_double(&hash, (value))) \
        return 0; } while (0)
    ID_TEXT(domain);
    ID_TEXT(model->target_id);
    ID_TEXT(model->family);
    ID_TEXT(model->architecture);
    ID_TEXT(model->repository);
    ID_TEXT(model->revision);
    ID_U64(model->hidden_size);
    ID_U64(model->vocabulary_size);
    ID_U64(model->maximum_context);
    ID_U64(model->main_layer_count);
    ID_U64(model->auxiliary_layer_count);
    ID_U64(model->embedding.required);
    ID_U64(model->embedding.vocabulary_size);
    ID_U64(model->embedding.hidden_size);
    ID_U64(model->output.required);
    ID_U64(model->output.tied_to_embedding);
    ID_U64(model->output.input_width);
    ID_U64(model->output.vocabulary_size);
    ID_U64(model->source_constraint.weight_dtype);
    ID_U64(model->source_constraint.expert_dtype);
    ID_U64(model->source_constraint.quantization);
    ID_U64(model->source_constraint.quant_block_rows);
    ID_U64(model->source_constraint.quant_block_columns);
    ID_U64(model->source_constraint.fp4_packing_factor);
    ID_U64(model->source_constraint.fp4_scale_group_width);
    ID_U64(model->source_constraint.fp4_physical_dtype);
    ID_U64(model->source_constraint.scale_dtype);
    ID_TEXT(model->tokenizer.tokenizer_class);
    ID_TEXT(model->tokenizer.model_type);
    ID_U64(model->tokenizer.vocabulary_size);
    ID_U64(model->tokenizer.base_vocab_entries);
    ID_U64(model->tokenizer.added_token_entries);
    ID_U64(model->tokenizer.maximum_token_id);
    ID_U64(model->tokenizer.maximum_context);
    ID_U64(model->tokenizer.bos_token_id);
    ID_U64(model->tokenizer.eos_token_id);
    ID_U64(model->tokenizer.bos_required);
    ID_U64(model->tokenizer.eos_required);
    ID_U64(model->final_mhc.residual_streams);
    ID_U64(model->final_mhc.stream_width);
    ID_U64(model->final_mhc.expanded_width);
    ID_U64(model->final_mhc.mixing_rows);
    ID_U64(model->final_mhc.mixing_columns);
    ID_U64(model->final_mhc.base_width);
    ID_U64(model->final_mhc.scale_width);
    ID_U64(model->final_mhc.sinkhorn_iterations);
    ID_DOUBLE(model->final_mhc.epsilon);
    ID_DOUBLE(model->final_mhc.residual_post_multiplier);
    ID_U64(model->final_mhc.entry);
    ID_U64(model->final_mhc.attention_pre_and_post);
    ID_U64(model->final_mhc.ffn_pre_and_deferred_post);
    ID_U64(model->final_mhc_head.required);
    ID_U64(model->final_mhc_head.function_rows);
    ID_U64(model->final_mhc_head.function_columns);
    ID_U64(model->final_mhc_head.base_width);
    ID_U64(model->final_mhc_head.scale_width);
    ID_DOUBLE(model->final_norm_epsilon);
    ID_U64(model->use_cache);
    ID_U64(model->final_mhc_post_required);
    ID_U64(model->final_mhc_head_required);
    ID_U64(model->final_norm_after_mhc_head);

    for (index = 0u; index < model->main_layer_count; ++index) {
        const yvex_deepseek_v4_layer_spec *layer =
            yvex_deepseek_v4_ir_layer_at(architecture, index);
        if (!layer) return 0;
        ID_U64(layer->layer_index);
        ID_U64(layer->attention_class);
        ID_U64(layer->compression_ratio);
        ID_U64(layer->query_heads);
        ID_U64(layer->kv_heads);
        ID_U64(layer->head_dimension);
        ID_U64(layer->rope_head_dimension);
        ID_U64(layer->non_rope_head_dimension);
        ID_U64(layer->query_lora_rank);
        ID_U64(layer->output_lora_rank);
        ID_U64(layer->output_groups);
        ID_U64(layer->output_heads_per_group);
        ID_U64(layer->output_group_input_width);
        ID_U64(layer->indexer_heads);
        ID_U64(layer->indexer_head_dimension);
        ID_U64(layer->indexer_topk);
        ID_U64(layer->attention_sink_count);
        ID_DOUBLE(layer->attention_dropout);
        ID_U64(layer->causal);
        ID_U64(layer->attention_bias);
        ID_U64(layer->query_norm_required);
        ID_U64(layer->kv_norm_required);
        ID_U64(layer->compressor_required);
        ID_U64(layer->indexer_required);
        ID_U64(layer->position.rope_dimension);
        ID_U64(layer->position.theta);
        ID_U64(layer->position.scaling_factor);
        ID_U64(layer->position.original_context);
        ID_U64(layer->position.beta_fast);
        ID_U64(layer->position.beta_slow);
        ID_U64(layer->position.maximum_context);
        ID_U64(layer->position.partial_rope);
        ID_U64(layer->position.inverse_output_rotation);
        ID_U64(layer->kv.class_id);
        ID_U64(layer->kv.compression_ratio);
        ID_U64(layer->kv.sliding_window);
        ID_U64(layer->kv.requires_state_cache);
        ID_U64(layer->kv.requires_uncompressed_tail);
        ID_U64(layer->kv.requires_compressed_core);
        ID_U64(layer->kv.requires_indexer_cache);
        ID_U64(layer->mhc.residual_streams);
        ID_U64(layer->mhc.stream_width);
        ID_U64(layer->mhc.expanded_width);
        ID_U64(layer->mhc.mixing_rows);
        ID_U64(layer->mhc.mixing_columns);
        ID_U64(layer->mhc.base_width);
        ID_U64(layer->mhc.scale_width);
        ID_U64(layer->mhc.sinkhorn_iterations);
        ID_U64(layer->mhc.entry);
        ID_U64(layer->mhc.attention_pre_and_post);
        ID_U64(layer->mhc.ffn_pre_and_deferred_post);
        ID_DOUBLE(layer->mhc.epsilon);
        ID_DOUBLE(layer->mhc.residual_post_multiplier);
        ID_U64(layer->moe.router_class);
        ID_U64(layer->moe.scoring);
        ID_U64(layer->moe.topk_policy);
        ID_U64(layer->moe.activation);
        ID_U64(layer->moe.routed_experts);
        ID_U64(layer->moe.shared_experts);
        ID_U64(layer->moe.experts_per_token);
        ID_U64(layer->moe.expert_intermediate_size);
        ID_U64(layer->moe.shared_intermediate_size);
        ID_U64(layer->moe.hash_table_rows);
        ID_U64(layer->moe.hash_table_columns);
        ID_U64(layer->moe.correction_bias_width);
        ID_DOUBLE(layer->moe.routed_scaling_factor);
        ID_DOUBLE(layer->moe.activation_limit);
        ID_U64(layer->moe.requires_token_ids);
        ID_U64(layer->moe.requires_hidden_state);
        ID_U64(layer->moe.requires_correction_bias);
        ID_U64(layer->moe.normalize_topk_probabilities);
        ID_U64(layer->attention_input_norm.required);
        ID_U64(layer->attention_input_norm.width);
        ID_U64(layer->post_attention_ffn_norm.required);
        ID_U64(layer->post_attention_ffn_norm.width);
        ID_U64(layer->tensors.q_a_rows);
        ID_U64(layer->tensors.q_a_columns);
        ID_U64(layer->tensors.q_b_rows);
        ID_U64(layer->tensors.q_b_columns);
        ID_U64(layer->tensors.kv_rows);
        ID_U64(layer->tensors.kv_columns);
        ID_U64(layer->tensors.o_a_rows);
        ID_U64(layer->tensors.o_a_columns);
        ID_U64(layer->tensors.o_b_rows);
        ID_U64(layer->tensors.o_b_columns);
        ID_U64(layer->tensors.compressor_ape_rows);
        ID_U64(layer->tensors.compressor_ape_columns);
        ID_U64(layer->tensors.compressor_norm_width);
        ID_U64(layer->tensors.compressor_projection_rows);
        ID_U64(layer->tensors.compressor_projection_columns);
        ID_U64(layer->tensors.indexer_ape_rows);
        ID_U64(layer->tensors.indexer_ape_columns);
        ID_U64(layer->tensors.indexer_norm_width);
        ID_U64(layer->tensors.indexer_projection_rows);
        ID_U64(layer->tensors.indexer_projection_columns);
        ID_U64(layer->tensors.indexer_query_rows);
        ID_U64(layer->tensors.indexer_query_columns);
        ID_U64(layer->tensors.indexer_weight_rows);
        ID_U64(layer->tensors.indexer_weight_columns);
        ID_DOUBLE(layer->rms_norm_epsilon);
    }
    for (index = 0u; index < model->auxiliary_layer_count; ++index) {
        const yvex_deepseek_v4_auxiliary_spec *aux =
            yvex_deepseek_v4_ir_auxiliary_at(architecture, index);
        if (!aux) return 0;
        ID_U64(aux->predictor_index);
        ID_U64(aux->layer.layer_index);
        ID_U64(aux->layer.attention_class);
        ID_U64(aux->layer.moe.router_class);
        ID_U64(aux->layer.moe.routed_experts);
        ID_U64(aux->layer.moe.expert_intermediate_size);
        ID_U64(aux->layer.moe.shared_intermediate_size);
        ID_U64(aux->layer.compression_ratio);
        ID_U64(aux->layer.query_heads);
        ID_U64(aux->layer.kv_heads);
        ID_U64(aux->layer.head_dimension);
        ID_U64(aux->layer.rope_head_dimension);
        ID_U64(aux->layer.non_rope_head_dimension);
        ID_U64(aux->layer.query_lora_rank);
        ID_U64(aux->layer.output_lora_rank);
        ID_U64(aux->layer.output_groups);
        ID_U64(aux->layer.output_heads_per_group);
        ID_U64(aux->layer.output_group_input_width);
        ID_U64(aux->layer.indexer_heads);
        ID_U64(aux->layer.indexer_head_dimension);
        ID_U64(aux->layer.indexer_topk);
        ID_U64(aux->layer.attention_sink_count);
        ID_DOUBLE(aux->layer.attention_dropout);
        ID_U64(aux->layer.causal);
        ID_U64(aux->layer.attention_bias);
        ID_U64(aux->layer.query_norm_required);
        ID_U64(aux->layer.kv_norm_required);
        ID_U64(aux->layer.compressor_required);
        ID_U64(aux->layer.indexer_required);
        ID_U64(aux->layer.position.rope_dimension);
        ID_U64(aux->layer.position.theta);
        ID_U64(aux->layer.position.scaling_factor);
        ID_U64(aux->layer.position.original_context);
        ID_U64(aux->layer.position.beta_fast);
        ID_U64(aux->layer.position.beta_slow);
        ID_U64(aux->layer.position.maximum_context);
        ID_U64(aux->layer.position.partial_rope);
        ID_U64(aux->layer.position.inverse_output_rotation);
        ID_U64(aux->layer.kv.class_id);
        ID_U64(aux->layer.kv.compression_ratio);
        ID_U64(aux->layer.kv.sliding_window);
        ID_U64(aux->layer.kv.requires_state_cache);
        ID_U64(aux->layer.kv.requires_uncompressed_tail);
        ID_U64(aux->layer.kv.requires_compressed_core);
        ID_U64(aux->layer.kv.requires_indexer_cache);
        ID_U64(aux->layer.mhc.residual_streams);
        ID_U64(aux->layer.mhc.stream_width);
        ID_U64(aux->layer.mhc.expanded_width);
        ID_U64(aux->layer.mhc.mixing_rows);
        ID_U64(aux->layer.mhc.mixing_columns);
        ID_U64(aux->layer.mhc.base_width);
        ID_U64(aux->layer.mhc.scale_width);
        ID_U64(aux->layer.mhc.sinkhorn_iterations);
        ID_DOUBLE(aux->layer.mhc.epsilon);
        ID_DOUBLE(aux->layer.mhc.residual_post_multiplier);
        ID_U64(aux->layer.mhc.entry);
        ID_U64(aux->layer.mhc.attention_pre_and_post);
        ID_U64(aux->layer.mhc.ffn_pre_and_deferred_post);
        ID_U64(aux->layer.moe.scoring);
        ID_U64(aux->layer.moe.topk_policy);
        ID_U64(aux->layer.moe.activation);
        ID_U64(aux->layer.moe.shared_experts);
        ID_U64(aux->layer.moe.experts_per_token);
        ID_U64(aux->layer.moe.hash_table_rows);
        ID_U64(aux->layer.moe.hash_table_columns);
        ID_U64(aux->layer.moe.correction_bias_width);
        ID_DOUBLE(aux->layer.moe.routed_scaling_factor);
        ID_DOUBLE(aux->layer.moe.activation_limit);
        ID_U64(aux->layer.moe.requires_token_ids);
        ID_U64(aux->layer.moe.requires_hidden_state);
        ID_U64(aux->layer.moe.requires_correction_bias);
        ID_U64(aux->layer.moe.normalize_topk_probabilities);
        ID_U64(aux->layer.attention_input_norm.required);
        ID_U64(aux->layer.attention_input_norm.width);
        ID_U64(aux->layer.post_attention_ffn_norm.required);
        ID_U64(aux->layer.post_attention_ffn_norm.width);
        ID_U64(aux->layer.tensors.q_a_rows);
        ID_U64(aux->layer.tensors.q_a_columns);
        ID_U64(aux->layer.tensors.q_b_rows);
        ID_U64(aux->layer.tensors.q_b_columns);
        ID_U64(aux->layer.tensors.kv_rows);
        ID_U64(aux->layer.tensors.kv_columns);
        ID_U64(aux->layer.tensors.o_a_rows);
        ID_U64(aux->layer.tensors.o_a_columns);
        ID_U64(aux->layer.tensors.o_b_rows);
        ID_U64(aux->layer.tensors.o_b_columns);
        ID_U64(aux->layer.tensors.compressor_ape_rows);
        ID_U64(aux->layer.tensors.compressor_ape_columns);
        ID_U64(aux->layer.tensors.compressor_norm_width);
        ID_U64(aux->layer.tensors.compressor_projection_rows);
        ID_U64(aux->layer.tensors.compressor_projection_columns);
        ID_U64(aux->layer.tensors.indexer_ape_rows);
        ID_U64(aux->layer.tensors.indexer_ape_columns);
        ID_U64(aux->layer.tensors.indexer_norm_width);
        ID_U64(aux->layer.tensors.indexer_projection_rows);
        ID_U64(aux->layer.tensors.indexer_projection_columns);
        ID_U64(aux->layer.tensors.indexer_query_rows);
        ID_U64(aux->layer.tensors.indexer_query_columns);
        ID_U64(aux->layer.tensors.indexer_weight_rows);
        ID_U64(aux->layer.tensors.indexer_weight_columns);
        ID_DOUBLE(aux->layer.rms_norm_epsilon);
        ID_U64(aux->previous_hidden_width);
        ID_U64(aux->embedding_projection_input);
        ID_U64(aux->embedding_projection_output);
        ID_U64(aux->hidden_projection_input);
        ID_U64(aux->hidden_projection_output);
        ID_U64(aux->requires_token_embedding);
        ID_U64(aux->requires_previous_hidden_state);
        ID_U64(aux->requires_embedding_norm);
        ID_U64(aux->requires_hidden_norm);
        ID_U64(aux->requires_separate_mhc_head);
        ID_U64(aux->mhc_head.required);
        ID_U64(aux->mhc_head.function_rows);
        ID_U64(aux->mhc_head.function_columns);
        ID_U64(aux->mhc_head.base_width);
        ID_U64(aux->mhc_head.scale_width);
        ID_U64(aux->shares_output_head);
        ID_U64(aux->shares_final_norm);
    }
#undef ID_DOUBLE
#undef ID_TEXT
#undef ID_U64
    if (!yvex_sha256_final(&hash, digest)) return 0;
    yvex_sha256_hex(digest, output);
    return 1;
}

static unsigned long long deepseek_hash_text(unsigned long long hash,
                                             const char *text)
{
    const unsigned char *cursor = (const unsigned char *)text;

    while (*cursor) {
        hash ^= (unsigned long long)*cursor++;
        hash *= 1099511628211ull;
    }
    hash ^= 0u;
    return hash * 1099511628211ull;
}

static unsigned long long deepseek_hash_u64(unsigned long long hash,
                                            unsigned long long value)
{
    unsigned int index;
    for (index = 0u; index < 8u; ++index) {
        hash ^= (value >> (index * 8u)) & 0xffu;
        hash *= 1099511628211ull;
    }
    return hash;
}

static yvex_transform_scope deepseek_scope(
    yvex_deepseek_tensor_scope scope)
{
    if (scope == YVEX_DEEPSEEK_TENSOR_SCOPE_MAIN_LAYER)
        return YVEX_TRANSFORM_SCOPE_MAIN_LAYER;
    if (scope == YVEX_DEEPSEEK_TENSOR_SCOPE_MTP)
        return YVEX_TRANSFORM_SCOPE_AUXILIARY;
    return YVEX_TRANSFORM_SCOPE_GLOBAL;
}

static yvex_transform_subsystem deepseek_subsystem(
    yvex_deepseek_tensor_collection collection)
{
    static const yvex_transform_subsystem map[] = {
        YVEX_TRANSFORM_SUBSYSTEM_GLOBAL,
        YVEX_TRANSFORM_SUBSYSTEM_ATTENTION,
        YVEX_TRANSFORM_SUBSYSTEM_COMPRESSOR,
        YVEX_TRANSFORM_SUBSYSTEM_INDEXER,
        YVEX_TRANSFORM_SUBSYSTEM_NORMALIZATION,
        YVEX_TRANSFORM_SUBSYSTEM_RESIDUAL,
        YVEX_TRANSFORM_SUBSYSTEM_ROUTER,
        YVEX_TRANSFORM_SUBSYSTEM_ROUTED_EXPERT,
        YVEX_TRANSFORM_SUBSYSTEM_SHARED_EXPERT,
        YVEX_TRANSFORM_SUBSYSTEM_AUXILIARY
    };
    return collection < YVEX_DEEPSEEK_TENSOR_COLLECTION_COUNT
        ? map[collection] : YVEX_TRANSFORM_SUBSYSTEM_COUNT;
}

static yvex_transform_dtype deepseek_dtype(yvex_native_dtype dtype,
                                           int packed_fp4)
{
    if (packed_fp4) return YVEX_TRANSFORM_DTYPE_PACKED_FP4;
    switch (dtype) {
    case YVEX_NATIVE_DTYPE_F32: return YVEX_TRANSFORM_DTYPE_F32;
    case YVEX_NATIVE_DTYPE_F16: return YVEX_TRANSFORM_DTYPE_F16;
    case YVEX_NATIVE_DTYPE_BF16: return YVEX_TRANSFORM_DTYPE_BF16;
    case YVEX_NATIVE_DTYPE_I32: return YVEX_TRANSFORM_DTYPE_I32;
    case YVEX_NATIVE_DTYPE_I64: return YVEX_TRANSFORM_DTYPE_I64;
    case YVEX_NATIVE_DTYPE_F8_E4M3: return YVEX_TRANSFORM_DTYPE_FP8_E4M3;
    case YVEX_NATIVE_DTYPE_F8_E8M0: return YVEX_TRANSFORM_DTYPE_E8M0_SCALE;
    default: return YVEX_TRANSFORM_DTYPE_UNKNOWN;
    }
}

static unsigned int deepseek_physical_classes(yvex_transform_dtype dtype)
{
    switch (dtype) {
    case YVEX_TRANSFORM_DTYPE_F32: return YVEX_TRANSFORM_PHYSICAL_F32;
    case YVEX_TRANSFORM_DTYPE_F16: return YVEX_TRANSFORM_PHYSICAL_F16 |
                                           YVEX_TRANSFORM_PHYSICAL_F32;
    case YVEX_TRANSFORM_DTYPE_BF16: return YVEX_TRANSFORM_PHYSICAL_BF16 |
                                            YVEX_TRANSFORM_PHYSICAL_F32;
    case YVEX_TRANSFORM_DTYPE_I32: return YVEX_TRANSFORM_PHYSICAL_I32;
    default: return YVEX_TRANSFORM_PHYSICAL_F32 |
                    YVEX_TRANSFORM_PHYSICAL_F16 |
                    YVEX_TRANSFORM_PHYSICAL_BF16 |
                    YVEX_TRANSFORM_PHYSICAL_QUANTIZED;
    }
}

static int deepseek_refuse(deepseek_transform_builder *builder,
                           yvex_transform_failure_code code,
                           unsigned long long expected,
                           unsigned long long actual,
                           const char *where)
{
    return yvex_transform_fail(
        builder ? builder->failure : NULL, code,
        YVEX_TRANSFORM_IR_NO_ID, YVEX_TRANSFORM_IR_NO_ID,
        YVEX_TRANSFORM_IR_NO_ID, YVEX_TRANSFORM_IR_NO_ID,
        YVEX_TRANSFORM_IR_NO_ID, expected, actual, 0u,
        builder ? builder->err : NULL, where);
}

/* Registers one exact retained coverage row as an immutable source value. */
static int deepseek_add_source(deepseek_transform_builder *builder,
                               const char *name,
                               yvex_tensor_role role,
                               yvex_deepseek_tensor_collection collection,
                               yvex_deepseek_tensor_scope scope,
                               unsigned long long layer,
                               unsigned long long auxiliary,
                               unsigned long long expert,
                               yvex_native_dtype expected_dtype,
                               int packed_fp4,
                               unsigned long long *value_id)
{
    const yvex_deepseek_tensor_coverage_row *row;
    yvex_transform_source_spec spec;
    unsigned long long requirement_index;
    unsigned long long source_index;
    unsigned long long identity;
    unsigned int dimension;

    row = yvex_deepseek_tensor_coverage_find(builder->coverage, name);
    if (!row || !row->source ||
        !yvex_deepseek_tensor_coverage_find_index(
            builder->coverage, name, &requirement_index) ||
        !yvex_deepseek_tensor_coverage_find_source_index(
            builder->coverage, name, &source_index)) {
        return deepseek_refuse(builder, YVEX_TRANSFORM_FAILURE_MISSING_SOURCE,
                               1u, 0u, "deepseek_transform_source");
    }
    if (row->collection != collection || row->scope != scope ||
        row->layer_index != layer || row->source->dtype != expected_dtype ||
        (expert != YVEX_DEEPSEEK_TENSOR_NO_INDEX &&
         row->expert_index != expert)) {
        return deepseek_refuse(
            builder,
            row->source->dtype != expected_dtype
                ? YVEX_TRANSFORM_FAILURE_UNSUPPORTED_SOURCE_DTYPE
                : YVEX_TRANSFORM_FAILURE_UNEXPECTED_SOURCE,
            (unsigned long long)expected_dtype,
            (unsigned long long)row->source->dtype,
            "deepseek_transform_source");
    }
    memset(&spec, 0, sizeof(spec));
    spec.source_name = row->source->name;
    spec.shard_name = row->source->shard_path;
    spec.source_tensor_index = source_index;
    spec.requirement_index = requirement_index;
    spec.source_snapshot_identity = builder->coverage_summary->source_identity;
    spec.source_dtype = row->source->dtype;
    spec.value_dtype = deepseek_dtype(row->source->dtype, packed_fp4);
    spec.shape.rank = row->source->rank;
    for (dimension = 0u; dimension < row->source->rank; ++dimension)
        spec.shape.dims[dimension] = row->source->dims[dimension];
    spec.relative_begin = row->source->data_start;
    spec.relative_end = row->source->data_end;
    identity = deepseek_hash_text(1469598103934665603ull, name);
    identity = deepseek_hash_u64(identity,
                                 builder->coverage_summary->coverage_identity);
    identity = deepseek_hash_u64(identity, requirement_index);
    spec.requirement_identity = identity;
    spec.scope = deepseek_scope(scope);
    spec.subsystem = deepseek_subsystem(collection);
    spec.role_hint = role;
    spec.layer_index = layer;
    spec.auxiliary_index = auxiliary;
    spec.expert_index = expert;
    spec.required_uses = 1u;
    return yvex_transform_builder_add_source(
        builder->builder, &spec, value_id, builder->failure, builder->err);
}

/* Publishes one terminal logical value and its sole semantic producer node. */
static int deepseek_add_terminal(deepseek_transform_builder *builder,
                                 yvex_tensor_role role,
                                 yvex_deepseek_tensor_collection collection,
                                 yvex_deepseek_tensor_scope scope,
                                 unsigned long long layer,
                                 unsigned long long auxiliary,
                                 const yvex_transform_shape *shape,
                                 yvex_transform_dtype dtype,
                                 const yvex_transform_precision_constraint *precision,
                                 const yvex_transform_node_spec *operation)
{
    yvex_transform_value_spec value;
    yvex_transform_node_spec node = *operation;
    unsigned long long value_id;
    unsigned long long node_id;
    unsigned long long semantic = 1469598103934665603ull;
    int rc;

    memset(&value, 0, sizeof(value));
    value.kind = YVEX_TRANSFORM_VALUE_TERMINAL;
    semantic = deepseek_hash_u64(semantic, (unsigned long long)scope);
    semantic = deepseek_hash_u64(semantic, (unsigned long long)collection);
    semantic = deepseek_hash_u64(semantic, (unsigned long long)role);
    semantic = deepseek_hash_u64(semantic, layer);
    semantic = deepseek_hash_u64(semantic, auxiliary);
    value.semantic_id = semantic;
    value.canonical_ordinal = builder->terminal_ordinal;
    value.shape = *shape;
    value.dtype = dtype;
    value.precision = *precision;
    value.logical_key.scope = deepseek_scope(scope);
    value.logical_key.subsystem = deepseek_subsystem(collection);
    value.logical_key.role = role;
    value.logical_key.layer_index = scope == YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL
        ? YVEX_TRANSFORM_IR_NO_ID : layer;
    value.logical_key.auxiliary_index =
        scope == YVEX_DEEPSEEK_TENSOR_SCOPE_MTP
            ? auxiliary : YVEX_TRANSFORM_IR_NO_ID;
    value.logical_key.group_index = 0u;
    rc = yvex_transform_builder_declare_value(
        builder->builder, &value, &value_id, builder->failure, builder->err);
    if (rc != YVEX_OK) return rc;
    node.output_value_id = value_id;
    rc = yvex_transform_builder_add_node(
        builder->builder, &node, &node_id, builder->failure, builder->err);
    if (rc == YVEX_OK) builder->terminal_ordinal++;
    return rc;
}

/* Adds a direct logical transfer or a checked I64-to-I32 narrowing plan. */
static int deepseek_add_direct(deepseek_transform_builder *builder,
                               yvex_tensor_role role,
                               yvex_deepseek_tensor_collection collection,
                               yvex_deepseek_tensor_scope scope,
                               unsigned long long layer,
                               unsigned long long auxiliary,
                               const char *source_name,
                               yvex_native_dtype source_dtype,
                               int checked_cast)
{
    const yvex_deepseek_tensor_coverage_row *row =
        yvex_deepseek_tensor_coverage_find(builder->coverage, source_name);
    yvex_transform_precision_constraint precision;
    yvex_transform_node_spec node;
    yvex_transform_shape shape;
    unsigned long long input;
    yvex_transform_dtype output_dtype;
    unsigned int dimension;
    int rc;

    if (!row || !row->source)
        return deepseek_refuse(builder, YVEX_TRANSFORM_FAILURE_MISSING_SOURCE,
                               1u, 0u, "deepseek_transform_direct");
    rc = deepseek_add_source(
        builder, source_name, role, collection, scope, layer, auxiliary,
        YVEX_DEEPSEEK_TENSOR_NO_INDEX, source_dtype, 0, &input);
    if (rc != YVEX_OK) return rc;
    memset(&shape, 0, sizeof(shape));
    shape.rank = row->source->rank;
    for (dimension = 0u; dimension < shape.rank; ++dimension)
        shape.dims[dimension] = row->source->dims[dimension];
    output_dtype = checked_cast ? YVEX_TRANSFORM_DTYPE_I32
                                : deepseek_dtype(source_dtype, 0);
    memset(&precision, 0, sizeof(precision));
    precision.allowed_physical_classes = deepseek_physical_classes(output_dtype);
    if (checked_cast) {
        precision.flags = YVEX_TRANSFORM_PRECISION_LOSSLESS |
                          YVEX_TRANSFORM_PRECISION_RANGE_PROOF |
                          YVEX_TRANSFORM_PRECISION_INTEGER_ONLY;
        precision.range_proof_required = 1;
    } else {
        precision.flags = YVEX_TRANSFORM_PRECISION_EXACT;
    }
    memset(&node, 0, sizeof(node));
    node.kind = checked_cast ? YVEX_TRANSFORM_OP_CHECKED_CAST
                             : YVEX_TRANSFORM_OP_IDENTITY;
    node.input_value_ids = &input;
    node.input_count = 1u;
    node.numeric = checked_cast ? YVEX_TRANSFORM_NUMERIC_RANGE_PROOF
                                : YVEX_TRANSFORM_NUMERIC_EXACT;
    node.ordering = YVEX_TRANSFORM_ORDER_INPUT;
    node.payload_execution_required = 1;
    return deepseek_add_terminal(builder, role, collection, scope, layer,
                                 auxiliary, &shape, output_dtype, &precision,
                                 &node);
}

/* Adds one paired FP8 E4M3 weight and E8M0 scale decode requirement. */
static int deepseek_add_fp8(deepseek_transform_builder *builder,
                            yvex_tensor_role role,
                            yvex_deepseek_tensor_collection collection,
                            yvex_deepseek_tensor_scope scope,
                            unsigned long long layer,
                            unsigned long long auxiliary,
                            const char *base)
{
    char weight[256];
    char scale[256];
    const yvex_deepseek_tensor_coverage_row *row;
    yvex_transform_precision_constraint precision;
    yvex_transform_node_spec node;
    yvex_transform_shape shape;
    unsigned long long inputs[2];
    unsigned int dimension;
    int rc;

    (void)snprintf(weight, sizeof(weight), "%s.weight", base);
    (void)snprintf(scale, sizeof(scale), "%s.scale", base);
    row = yvex_deepseek_tensor_coverage_find(builder->coverage, weight);
    if (!row || !row->source)
        return deepseek_refuse(builder, YVEX_TRANSFORM_FAILURE_MISSING_SOURCE,
                               1u, 0u, "deepseek_transform_fp8");
    rc = deepseek_add_source(
        builder, weight, role, collection, scope, layer, auxiliary,
        YVEX_DEEPSEEK_TENSOR_NO_INDEX, YVEX_NATIVE_DTYPE_F8_E4M3, 0,
        &inputs[0]);
    if (rc == YVEX_OK)
        rc = deepseek_add_source(
            builder, scale, role, collection, scope, layer, auxiliary,
            YVEX_DEEPSEEK_TENSOR_NO_INDEX, YVEX_NATIVE_DTYPE_F8_E8M0, 0,
            &inputs[1]);
    if (rc != YVEX_OK) return rc;
    memset(&shape, 0, sizeof(shape));
    shape.rank = row->source->rank;
    for (dimension = 0u; dimension < shape.rank; ++dimension)
        shape.dims[dimension] = row->source->dims[dimension];
    memset(&precision, 0, sizeof(precision));
    precision.flags = YVEX_TRANSFORM_PRECISION_SCALE_PAIRED |
                      YVEX_TRANSFORM_PRECISION_QUANTIZABLE_WEIGHT |
                      YVEX_TRANSFORM_PRECISION_REFERENCE_COMPUTE;
    precision.allowed_physical_classes =
        YVEX_TRANSFORM_PHYSICAL_F32 | YVEX_TRANSFORM_PHYSICAL_F16 |
        YVEX_TRANSFORM_PHYSICAL_BF16 | YVEX_TRANSFORM_PHYSICAL_QUANTIZED;
    precision.approximation_allowed = 1;
    precision.reference_compute_required = 1;
    memset(&node, 0, sizeof(node));
    node.kind = YVEX_TRANSFORM_OP_DECODE_SCALE_PAIR;
    node.input_value_ids = inputs;
    node.input_count = 2u;
    node.scale_block_rows = builder->model->source_constraint.quant_block_rows;
    node.scale_block_columns =
        builder->model->source_constraint.quant_block_columns;
    node.numeric = YVEX_TRANSFORM_NUMERIC_LOSSLESS;
    node.ordering = YVEX_TRANSFORM_ORDER_INPUT;
    node.payload_execution_required = 1;
    return deepseek_add_terminal(
        builder, role, collection, scope, layer, auxiliary, &shape,
        YVEX_TRANSFORM_DTYPE_REAL, &precision, &node);
}

/* Adds one deterministic 256-expert, weight/scale-paired aggregation plan. */
static int deepseek_add_experts(deepseek_transform_builder *builder,
                                yvex_tensor_role role,
                                yvex_deepseek_tensor_scope scope,
                                unsigned long long layer,
                                unsigned long long auxiliary,
                                const char *prefix,
                                const char *projection,
                                unsigned long long expert_count)
{
    char weight[256];
    char scale[256];
    const yvex_deepseek_tensor_coverage_row *first;
    yvex_transform_precision_constraint precision;
    yvex_transform_node_spec node;
    yvex_transform_shape shape;
    unsigned long long *inputs = NULL;
    unsigned long long input_count;
    unsigned long long logical_width;
    unsigned long long expert;
    size_t bytes;
    int rc = YVEX_OK;

    if (!expert_count || expert_count > ULLONG_MAX / 2u)
        return deepseek_refuse(
            builder, YVEX_TRANSFORM_FAILURE_INVALID_AGGREGATION,
            1u, expert_count, "deepseek_transform_experts");
    input_count = expert_count * 2u;
    if (input_count > (unsigned long long)(SIZE_MAX / sizeof(inputs[0])))
        return deepseek_refuse(
            builder, YVEX_TRANSFORM_FAILURE_RESOURCE_BUDGET,
            SIZE_MAX, input_count, "deepseek_transform_experts");
    bytes = (size_t)input_count * sizeof(inputs[0]);
    inputs = (unsigned long long *)builder->temporary_allocator.allocate(
        bytes, builder->temporary_allocator.context);
    if (!inputs)
        return deepseek_refuse(builder, YVEX_TRANSFORM_FAILURE_ALLOCATION,
                               bytes, 0u, "deepseek_transform_experts");
    (void)snprintf(weight, sizeof(weight), "%s.ffn.experts.0.%s.weight",
                   prefix, projection);
    first = yvex_deepseek_tensor_coverage_find(builder->coverage, weight);
    if (!first || !first->source || first->source->rank != 2u ||
        first->source->dims[1] > ULLONG_MAX /
            builder->model->source_constraint.fp4_packing_factor) {
        rc = deepseek_refuse(
            builder, first ? YVEX_TRANSFORM_FAILURE_DIMENSION_OVERFLOW
                           : YVEX_TRANSFORM_FAILURE_MISSING_SOURCE,
            1u, 0u, "deepseek_transform_experts");
        goto cleanup;
    }
    for (expert = 0u; expert < expert_count; ++expert) {
        (void)snprintf(weight, sizeof(weight),
                       "%s.ffn.experts.%llu.%s.weight", prefix, expert,
                       projection);
        (void)snprintf(scale, sizeof(scale),
                       "%s.ffn.experts.%llu.%s.scale", prefix, expert,
                       projection);
        rc = deepseek_add_source(
            builder, weight, role,
            YVEX_DEEPSEEK_TENSOR_COLLECTION_ROUTED_EXPERT, scope, layer,
            auxiliary, expert, YVEX_NATIVE_DTYPE_I8, 1,
            &inputs[expert * 2u]);
        if (rc == YVEX_OK)
            rc = deepseek_add_source(
                builder, scale, role,
                YVEX_DEEPSEEK_TENSOR_COLLECTION_ROUTED_EXPERT, scope, layer,
                auxiliary, expert, YVEX_NATIVE_DTYPE_F8_E8M0, 0,
                &inputs[expert * 2u + 1u]);
        if (rc != YVEX_OK) goto cleanup;
    }
    logical_width = first->source->dims[1] *
                    builder->model->source_constraint.fp4_packing_factor;
    memset(&shape, 0, sizeof(shape));
    shape.rank = 3u;
    shape.dims[0] = expert_count;
    shape.dims[1] = first->source->dims[0];
    shape.dims[2] = logical_width;
    memset(&precision, 0, sizeof(precision));
    precision.flags = YVEX_TRANSFORM_PRECISION_SCALE_PAIRED |
                      YVEX_TRANSFORM_PRECISION_QUANTIZABLE_WEIGHT |
                      YVEX_TRANSFORM_PRECISION_REFERENCE_COMPUTE;
    precision.allowed_physical_classes =
        YVEX_TRANSFORM_PHYSICAL_F32 | YVEX_TRANSFORM_PHYSICAL_F16 |
        YVEX_TRANSFORM_PHYSICAL_BF16 | YVEX_TRANSFORM_PHYSICAL_QUANTIZED;
    precision.approximation_allowed = 1;
    precision.reference_compute_required = 1;
    memset(&node, 0, sizeof(node));
    node.kind = YVEX_TRANSFORM_OP_EXPERT_AGGREGATE;
    node.input_value_ids = inputs;
    node.input_count = input_count;
    node.axis = 0u;
    node.expert_count = expert_count;
    node.packing_factor =
        builder->model->source_constraint.fp4_packing_factor;
    node.scale_group_width =
        builder->model->source_constraint.fp4_scale_group_width;
    node.numeric = YVEX_TRANSFORM_NUMERIC_LOSSLESS;
    node.ordering = YVEX_TRANSFORM_ORDER_EXPERT_INDEX;
    node.payload_execution_required = 1;
    rc = deepseek_add_terminal(
        builder, role, YVEX_DEEPSEEK_TENSOR_COLLECTION_ROUTED_EXPERT,
        scope, layer, auxiliary, &shape, YVEX_TRANSFORM_DTYPE_REAL,
        &precision, &node);

cleanup:
    builder->temporary_allocator.release(
        inputs, builder->temporary_allocator.context);
    return rc;
}

static int deepseek_add_mhc(deepseek_transform_builder *builder,
                            const char *prefix,
                            const char *kind,
                            yvex_deepseek_tensor_scope scope,
                            unsigned long long layer,
                            unsigned long long auxiliary)
{
    const yvex_tensor_role roles[3] = {
        strcmp(kind, "attn") == 0
            ? YVEX_TENSOR_ROLE_HC_ATTENTION_FUNCTION
            : YVEX_TENSOR_ROLE_HC_FFN_FUNCTION,
        strcmp(kind, "attn") == 0
            ? YVEX_TENSOR_ROLE_HC_ATTENTION_BASE
            : YVEX_TENSOR_ROLE_HC_FFN_BASE,
        strcmp(kind, "attn") == 0
            ? YVEX_TENSOR_ROLE_HC_ATTENTION_SCALE
            : YVEX_TENSOR_ROLE_HC_FFN_SCALE
    };
    const char *suffixes[3] = {"fn", "base", "scale"};
    char name[256];
    unsigned int index;
    int rc;

    for (index = 0u; index < 3u; ++index) {
        (void)snprintf(name, sizeof(name), "%s.hc_%s_%s", prefix, kind,
                       suffixes[index]);
        rc = deepseek_add_direct(
            builder, roles[index], YVEX_DEEPSEEK_TENSOR_COLLECTION_MHC,
            scope, layer, auxiliary, name, YVEX_NATIVE_DTYPE_F32, 0);
        if (rc != YVEX_OK) return rc;
    }
    return YVEX_OK;
}

static int deepseek_add_attention(
    deepseek_transform_builder *builder,
    const char *prefix,
    const yvex_deepseek_v4_layer_spec *layer,
    yvex_deepseek_tensor_scope scope,
    unsigned long long auxiliary)
{
    char name[256];
    char base[256];
    int rc;

#define DIRECT(role_id, collection_id, suffix, dtype_id) do {                 \
    (void)snprintf(name, sizeof(name), "%s.%s", prefix, suffix);             \
    rc = deepseek_add_direct(builder, role_id, collection_id, scope,          \
                             layer->layer_index, auxiliary, name, dtype_id, 0);\
    if (rc != YVEX_OK) return rc;                                               \
} while (0)
#define FP8(role_id, collection_id, suffix) do {                               \
    (void)snprintf(base, sizeof(base), "%s.%s", prefix, suffix);             \
    rc = deepseek_add_fp8(builder, role_id, collection_id, scope,              \
                          layer->layer_index, auxiliary, base);                 \
    if (rc != YVEX_OK) return rc;                                               \
} while (0)
    DIRECT(YVEX_TENSOR_ROLE_ATTENTION_SINKS,
           YVEX_DEEPSEEK_TENSOR_COLLECTION_ATTENTION,
           "attn.attn_sink", YVEX_NATIVE_DTYPE_F32);
    DIRECT(YVEX_TENSOR_ROLE_ATTENTION_Q_A_NORM,
           YVEX_DEEPSEEK_TENSOR_COLLECTION_ATTENTION,
           "attn.q_norm.weight", YVEX_NATIVE_DTYPE_BF16);
    DIRECT(YVEX_TENSOR_ROLE_ATTENTION_KV_NORM,
           YVEX_DEEPSEEK_TENSOR_COLLECTION_ATTENTION,
           "attn.kv_norm.weight", YVEX_NATIVE_DTYPE_BF16);
    FP8(YVEX_TENSOR_ROLE_ATTENTION_KV,
        YVEX_DEEPSEEK_TENSOR_COLLECTION_ATTENTION, "attn.wkv");
    FP8(YVEX_TENSOR_ROLE_ATTENTION_Q_A,
        YVEX_DEEPSEEK_TENSOR_COLLECTION_ATTENTION, "attn.wq_a");
    FP8(YVEX_TENSOR_ROLE_ATTENTION_Q_B,
        YVEX_DEEPSEEK_TENSOR_COLLECTION_ATTENTION, "attn.wq_b");
    FP8(YVEX_TENSOR_ROLE_ATTENTION_OUT_A,
        YVEX_DEEPSEEK_TENSOR_COLLECTION_ATTENTION, "attn.wo_a");
    FP8(YVEX_TENSOR_ROLE_ATTENTION_OUT_B,
        YVEX_DEEPSEEK_TENSOR_COLLECTION_ATTENTION, "attn.wo_b");
    if (layer->compressor_required) {
        DIRECT(YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_APE,
               YVEX_DEEPSEEK_TENSOR_COLLECTION_COMPRESSOR,
               "attn.compressor.ape", YVEX_NATIVE_DTYPE_F32);
        DIRECT(YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_NORM,
               YVEX_DEEPSEEK_TENSOR_COLLECTION_COMPRESSOR,
               "attn.compressor.norm.weight", YVEX_NATIVE_DTYPE_BF16);
        DIRECT(YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_GATE,
               YVEX_DEEPSEEK_TENSOR_COLLECTION_COMPRESSOR,
               "attn.compressor.wgate.weight", YVEX_NATIVE_DTYPE_BF16);
        DIRECT(YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_KV,
               YVEX_DEEPSEEK_TENSOR_COLLECTION_COMPRESSOR,
               "attn.compressor.wkv.weight", YVEX_NATIVE_DTYPE_BF16);
    }
    if (layer->indexer_required) {
        DIRECT(YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_APE,
               YVEX_DEEPSEEK_TENSOR_COLLECTION_INDEXER,
               "attn.indexer.compressor.ape", YVEX_NATIVE_DTYPE_F32);
        DIRECT(YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_NORM,
               YVEX_DEEPSEEK_TENSOR_COLLECTION_INDEXER,
               "attn.indexer.compressor.norm.weight", YVEX_NATIVE_DTYPE_BF16);
        DIRECT(YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_GATE,
               YVEX_DEEPSEEK_TENSOR_COLLECTION_INDEXER,
               "attn.indexer.compressor.wgate.weight", YVEX_NATIVE_DTYPE_BF16);
        DIRECT(YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_KV,
               YVEX_DEEPSEEK_TENSOR_COLLECTION_INDEXER,
               "attn.indexer.compressor.wkv.weight", YVEX_NATIVE_DTYPE_BF16);
        FP8(YVEX_TENSOR_ROLE_INDEXER_ATTENTION_Q_B,
            YVEX_DEEPSEEK_TENSOR_COLLECTION_INDEXER,
            "attn.indexer.wq_b");
        DIRECT(YVEX_TENSOR_ROLE_INDEXER_PROJECTION,
               YVEX_DEEPSEEK_TENSOR_COLLECTION_INDEXER,
               "attn.indexer.weights_proj.weight", YVEX_NATIVE_DTYPE_BF16);
    }
#undef FP8
#undef DIRECT
    return YVEX_OK;
}

static int deepseek_add_moe(deepseek_transform_builder *builder,
                            const char *prefix,
                            const yvex_deepseek_v4_layer_spec *layer,
                            yvex_deepseek_tensor_scope scope,
                            unsigned long long auxiliary)
{
    char name[256];
    char base[256];
    int rc;

    rc = deepseek_add_experts(
        builder, YVEX_TENSOR_ROLE_MOE_EXPERT_GATE, scope,
        layer->layer_index, auxiliary, prefix, "w1", layer->moe.routed_experts);
    if (rc != YVEX_OK) return rc;
    rc = deepseek_add_experts(
        builder, YVEX_TENSOR_ROLE_MOE_EXPERT_DOWN, scope,
        layer->layer_index, auxiliary, prefix, "w2", layer->moe.routed_experts);
    if (rc != YVEX_OK) return rc;
    rc = deepseek_add_experts(
        builder, YVEX_TENSOR_ROLE_MOE_EXPERT_UP, scope,
        layer->layer_index, auxiliary, prefix, "w3", layer->moe.routed_experts);
    if (rc != YVEX_OK) return rc;
    (void)snprintf(base, sizeof(base), "%s.ffn.shared_experts.w1", prefix);
    rc = deepseek_add_fp8(
        builder, YVEX_TENSOR_ROLE_MOE_SHARED_EXPERT_GATE,
        YVEX_DEEPSEEK_TENSOR_COLLECTION_SHARED_EXPERT, scope,
        layer->layer_index, auxiliary, base);
    if (rc != YVEX_OK) return rc;
    (void)snprintf(base, sizeof(base), "%s.ffn.shared_experts.w2", prefix);
    rc = deepseek_add_fp8(
        builder, YVEX_TENSOR_ROLE_MOE_SHARED_EXPERT_DOWN,
        YVEX_DEEPSEEK_TENSOR_COLLECTION_SHARED_EXPERT, scope,
        layer->layer_index, auxiliary, base);
    if (rc != YVEX_OK) return rc;
    (void)snprintf(base, sizeof(base), "%s.ffn.shared_experts.w3", prefix);
    rc = deepseek_add_fp8(
        builder, YVEX_TENSOR_ROLE_MOE_SHARED_EXPERT_UP,
        YVEX_DEEPSEEK_TENSOR_COLLECTION_SHARED_EXPERT, scope,
        layer->layer_index, auxiliary, base);
    if (rc != YVEX_OK) return rc;
    (void)snprintf(name, sizeof(name), "%s.ffn.gate.weight", prefix);
    rc = deepseek_add_direct(
        builder, YVEX_TENSOR_ROLE_MOE_ROUTER,
        YVEX_DEEPSEEK_TENSOR_COLLECTION_ROUTER, scope, layer->layer_index,
        auxiliary, name, YVEX_NATIVE_DTYPE_BF16, 0);
    if (rc != YVEX_OK) return rc;
    if (layer->moe.router_class == YVEX_DEEPSEEK_V4_ROUTER_HASH_TOKEN_ID) {
        (void)snprintf(name, sizeof(name), "%s.ffn.gate.tid2eid", prefix);
        return deepseek_add_direct(
            builder, YVEX_TENSOR_ROLE_MOE_ROUTER_TABLE,
            YVEX_DEEPSEEK_TENSOR_COLLECTION_ROUTER, scope,
            layer->layer_index, auxiliary, name, YVEX_NATIVE_DTYPE_I64, 1);
    }
    (void)snprintf(name, sizeof(name), "%s.ffn.gate.bias", prefix);
    return deepseek_add_direct(
        builder, YVEX_TENSOR_ROLE_MOE_ROUTER_BIAS,
        YVEX_DEEPSEEK_TENSOR_COLLECTION_ROUTER, scope, layer->layer_index,
        auxiliary, name, YVEX_NATIVE_DTYPE_F32, 0);
}

static int deepseek_add_layer(deepseek_transform_builder *builder,
                              const char *prefix,
                              const yvex_deepseek_v4_layer_spec *layer,
                              yvex_deepseek_tensor_scope scope,
                              unsigned long long auxiliary)
{
    char name[256];
    int rc;

    rc = deepseek_add_attention(builder, prefix, layer, scope, auxiliary);
    if (rc != YVEX_OK) return rc;
    (void)snprintf(name, sizeof(name), "%s.attn_norm.weight", prefix);
    rc = deepseek_add_direct(
        builder, YVEX_TENSOR_ROLE_ATTENTION_NORM,
        YVEX_DEEPSEEK_TENSOR_COLLECTION_NORM, scope, layer->layer_index,
        auxiliary, name, YVEX_NATIVE_DTYPE_BF16, 0);
    if (rc != YVEX_OK) return rc;
    (void)snprintf(name, sizeof(name), "%s.ffn_norm.weight", prefix);
    rc = deepseek_add_direct(
        builder, YVEX_TENSOR_ROLE_FFN_NORM,
        YVEX_DEEPSEEK_TENSOR_COLLECTION_NORM, scope, layer->layer_index,
        auxiliary, name, YVEX_NATIVE_DTYPE_BF16, 0);
    if (rc != YVEX_OK) return rc;
    rc = deepseek_add_mhc(builder, prefix, "attn", scope,
                          layer->layer_index, auxiliary);
    if (rc != YVEX_OK) return rc;
    rc = deepseek_add_mhc(builder, prefix, "ffn", scope,
                          layer->layer_index, auxiliary);
    if (rc != YVEX_OK) return rc;
    return deepseek_add_moe(builder, prefix, layer, scope, auxiliary);
}

/* Enumerates the exact canonical terminal set without physical-format facts. */
static int deepseek_build_graph(deepseek_transform_builder *builder)
{
    const yvex_tensor_role head_roles[3] = {
        YVEX_TENSOR_ROLE_HC_HEAD_FUNCTION,
        YVEX_TENSOR_ROLE_HC_HEAD_BASE,
        YVEX_TENSOR_ROLE_HC_HEAD_SCALE
    };
    const char *head_names[3] = {
        "hc_head_fn", "hc_head_base", "hc_head_scale"
    };
    unsigned long long layer;
    unsigned int index;
    int rc;

    rc = deepseek_add_direct(
        builder, YVEX_TENSOR_ROLE_TOKEN_EMBEDDING,
        YVEX_DEEPSEEK_TENSOR_COLLECTION_GLOBAL,
        YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL, YVEX_DEEPSEEK_TENSOR_NO_INDEX,
        YVEX_DEEPSEEK_TENSOR_NO_INDEX, "embed.weight",
        YVEX_NATIVE_DTYPE_BF16, 0);
    if (rc != YVEX_OK) return rc;
    rc = deepseek_add_direct(
        builder, YVEX_TENSOR_ROLE_OUTPUT_NORM,
        YVEX_DEEPSEEK_TENSOR_COLLECTION_GLOBAL,
        YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL, YVEX_DEEPSEEK_TENSOR_NO_INDEX,
        YVEX_DEEPSEEK_TENSOR_NO_INDEX, "norm.weight",
        YVEX_NATIVE_DTYPE_BF16, 0);
    if (rc != YVEX_OK) return rc;
    rc = deepseek_add_direct(
        builder, YVEX_TENSOR_ROLE_OUTPUT_HEAD,
        YVEX_DEEPSEEK_TENSOR_COLLECTION_GLOBAL,
        YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL, YVEX_DEEPSEEK_TENSOR_NO_INDEX,
        YVEX_DEEPSEEK_TENSOR_NO_INDEX, "head.weight",
        YVEX_NATIVE_DTYPE_BF16, 0);
    if (rc != YVEX_OK) return rc;
    for (index = 0u; index < 3u; ++index) {
        rc = deepseek_add_direct(
            builder, head_roles[index],
            YVEX_DEEPSEEK_TENSOR_COLLECTION_GLOBAL,
            YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL,
            YVEX_DEEPSEEK_TENSOR_NO_INDEX, YVEX_DEEPSEEK_TENSOR_NO_INDEX,
            head_names[index], YVEX_NATIVE_DTYPE_F32, 0);
        if (rc != YVEX_OK) return rc;
    }
    for (layer = 0u; layer < builder->model->main_layer_count; ++layer) {
        char prefix[64];
        (void)snprintf(prefix, sizeof(prefix), "layers.%llu", layer);
        rc = deepseek_add_layer(
            builder, prefix,
            yvex_deepseek_v4_ir_layer_at(builder->architecture, layer),
            YVEX_DEEPSEEK_TENSOR_SCOPE_MAIN_LAYER,
            YVEX_DEEPSEEK_TENSOR_NO_INDEX);
        if (rc != YVEX_OK) return rc;
    }
    for (layer = 0u; layer < builder->model->auxiliary_layer_count; ++layer) {
        const yvex_deepseek_v4_auxiliary_spec *aux =
            yvex_deepseek_v4_ir_auxiliary_at(builder->architecture, layer);
        char prefix[64];
        char name[128];
        char base[128];

        if (!aux)
            return deepseek_refuse(
                builder, YVEX_TRANSFORM_FAILURE_ARCHITECTURE_NOT_ADMITTED,
                1u, 0u, "deepseek_transform_auxiliary");
        (void)snprintf(prefix, sizeof(prefix), "mtp.%llu", layer);
        rc = deepseek_add_layer(builder, prefix, &aux->layer,
                                YVEX_DEEPSEEK_TENSOR_SCOPE_MTP, layer);
        if (rc != YVEX_OK) return rc;
        (void)snprintf(base, sizeof(base), "%s.e_proj", prefix);
        rc = deepseek_add_fp8(
            builder, YVEX_TENSOR_ROLE_MTP_EMBEDDING_PROJECTION,
            YVEX_DEEPSEEK_TENSOR_COLLECTION_AUXILIARY,
            YVEX_DEEPSEEK_TENSOR_SCOPE_MTP, aux->layer.layer_index, layer,
            base);
        if (rc != YVEX_OK) return rc;
        (void)snprintf(base, sizeof(base), "%s.h_proj", prefix);
        rc = deepseek_add_fp8(
            builder, YVEX_TENSOR_ROLE_MTP_HIDDEN_PROJECTION,
            YVEX_DEEPSEEK_TENSOR_COLLECTION_AUXILIARY,
            YVEX_DEEPSEEK_TENSOR_SCOPE_MTP, aux->layer.layer_index, layer,
            base);
        if (rc != YVEX_OK) return rc;
#define MTP_DIRECT(role_id, suffix) do {                                       \
    (void)snprintf(name, sizeof(name), "%s.%s", prefix, suffix);             \
    rc = deepseek_add_direct(                                                   \
        builder, role_id, YVEX_DEEPSEEK_TENSOR_COLLECTION_AUXILIARY,          \
        YVEX_DEEPSEEK_TENSOR_SCOPE_MTP, aux->layer.layer_index, layer, name,   \
        YVEX_NATIVE_DTYPE_BF16, 0);                                             \
    if (rc != YVEX_OK) return rc;                                               \
} while (0)
        MTP_DIRECT(YVEX_TENSOR_ROLE_MTP_EMBEDDING_NORM, "enorm.weight");
        MTP_DIRECT(YVEX_TENSOR_ROLE_MTP_HIDDEN_NORM, "hnorm.weight");
        MTP_DIRECT(YVEX_TENSOR_ROLE_MTP_OUTPUT_NORM, "norm.weight");
#undef MTP_DIRECT
        for (index = 0u; index < 3u; ++index) {
            (void)snprintf(name, sizeof(name), "%s.hc_head_%s", prefix,
                           index == 0u ? "fn" :
                           (index == 1u ? "base" : "scale"));
            rc = deepseek_add_direct(
                builder, head_roles[index],
                YVEX_DEEPSEEK_TENSOR_COLLECTION_AUXILIARY,
                YVEX_DEEPSEEK_TENSOR_SCOPE_MTP, aux->layer.layer_index,
                layer, name, YVEX_NATIVE_DTYPE_F32, 0);
            if (rc != YVEX_OK) return rc;
        }
    }
    return YVEX_OK;
}

/* Validates identity/trust owners before any mutable graph allocation escapes. */
static int deepseek_validate_inputs(
    deepseek_transform_builder *builder,
    const yvex_source_verification *verification,
    const yvex_deepseek_v4_ir *architecture,
    const yvex_deepseek_tensor_coverage *coverage,
    yvex_transform_failure *failure,
    yvex_error *err)
{
    const yvex_deepseek_v4_model_spec *model =
        yvex_deepseek_v4_ir_model(architecture);
    const yvex_deepseek_tensor_coverage_summary *summary =
        yvex_deepseek_tensor_coverage_summary_get(coverage);

    memset(builder, 0, sizeof(*builder));
    builder->failure = failure;
    builder->err = err;
    if (!verification || !architecture || !coverage || !model || !summary)
        return deepseek_refuse(
            builder, YVEX_TRANSFORM_FAILURE_INVALID_ARGUMENT,
            1u, 0u, "deepseek_transform_build");
    if (!verification->verified || verification->blocker_count != 0u ||
        model->main_layer_count != 43u ||
        model->auxiliary_layer_count != 1u) {
        return deepseek_refuse(
            builder, YVEX_TRANSFORM_FAILURE_ARCHITECTURE_NOT_ADMITTED,
            44u, model->main_layer_count + model->auxiliary_layer_count,
            "deepseek_transform_build");
    }
    if (!summary->complete ||
        summary->source_tensor_count != YVEX_DEEPSEEK_TRANSFORM_SOURCE_COUNT ||
        summary->matched_tensor_count != YVEX_DEEPSEEK_TRANSFORM_SOURCE_COUNT ||
        summary->missing_count || summary->ambiguous_count ||
        summary->unexpected_count || summary->header_scan_count != 1u ||
        summary->payload_bytes_read != 0u) {
        return deepseek_refuse(
            builder, YVEX_TRANSFORM_FAILURE_COVERAGE_INCOMPLETE,
            YVEX_DEEPSEEK_TRANSFORM_SOURCE_COUNT,
            summary->matched_tensor_count, "deepseek_transform_build");
    }
    if (verification->source_snapshot_identity != summary->source_identity)
        return deepseek_refuse(
            builder, YVEX_TRANSFORM_FAILURE_SOURCE_IDENTITY_MISMATCH,
            verification->source_snapshot_identity, summary->source_identity,
            "deepseek_transform_build");
    if (!verification->manifest_payload_trusted ||
        !yvex_sha256_hex_valid(verification->manifest_payload_identity) ||
        verification->manifest_payload_source_snapshot_identity !=
            summary->source_identity ||
        (strcmp(verification->manifest_payload_trust_class,
                "upstream_payload_verified") != 0 &&
         strcmp(verification->manifest_payload_trust_class,
                "local_payload_snapshot_sealed") != 0)) {
        return deepseek_refuse(
            builder, YVEX_TRANSFORM_FAILURE_PAYLOAD_IDENTITY_MISMATCH,
            summary->source_identity,
            verification->manifest_payload_source_snapshot_identity,
            "deepseek_transform_build");
    }
    builder->verification = verification;
    builder->architecture = architecture;
    builder->model = model;
    builder->coverage = coverage;
    builder->coverage_summary = summary;
    return YVEX_OK;
}

int yvex_deepseek_transform_ir_build(
    yvex_transform_ir **out,
    const yvex_source_verification *verification,
    const yvex_deepseek_v4_ir *architecture,
    const yvex_deepseek_tensor_coverage *coverage,
    const yvex_transform_builder_options *options,
    yvex_transform_failure *failure,
    yvex_error *err)
{
    deepseek_transform_builder deepseek;
    yvex_transform_header header;
    char logical_identity[YVEX_TRANSFORM_IR_IDENTITY_CAP];
    int rc;

    if (out) *out = NULL;
    if (failure) memset(failure, 0, sizeof(*failure));
    yvex_error_clear(err);
    if (!out)
        return yvex_transform_fail(
            failure, YVEX_TRANSFORM_FAILURE_INVALID_ARGUMENT,
            YVEX_TRANSFORM_IR_NO_ID, YVEX_TRANSFORM_IR_NO_ID,
            YVEX_TRANSFORM_IR_NO_ID, YVEX_TRANSFORM_IR_NO_ID,
            YVEX_TRANSFORM_IR_NO_ID, 1u, 0u, 0u, err,
            "deepseek_transform_build");
    rc = deepseek_validate_inputs(&deepseek, verification, architecture,
                                  coverage, failure, err);
    if (rc != YVEX_OK) return rc;
    if (!deepseek_architecture_identity(architecture, logical_identity))
        return deepseek_refuse(
            &deepseek, YVEX_TRANSFORM_FAILURE_IDENTITY_ENCODING,
            1u, 0u, "deepseek_transform_architecture_identity");
    deepseek.temporary_allocator.allocate = deepseek_default_allocate;
    deepseek.temporary_allocator.release = deepseek_default_release;
    deepseek.temporary_allocator.context = NULL;
    if (options && options->allocator.allocate)
        deepseek.temporary_allocator = options->allocator;
    memset(&header, 0, sizeof(header));
    header.schema_version = YVEX_TRANSFORM_IR_SCHEMA_VERSION;
    header.logical_model_identity = logical_identity;
    header.source_snapshot_identity =
        deepseek.coverage_summary->source_identity;
    header.coverage_identity = deepseek.coverage_summary->coverage_identity;
    header.required_payload_identity =
        verification->manifest_payload_identity;
    header.payload_trust_class = verification->manifest_payload_trust_class;
    header.expected_source_count = YVEX_DEEPSEEK_TRANSFORM_SOURCE_COUNT;
    header.expected_terminal_count = YVEX_DEEPSEEK_TRANSFORM_TERMINAL_COUNT;
    header.header_scan_count = deepseek.coverage_summary->header_scan_count;
    rc = yvex_transform_builder_create(
        &deepseek.builder, &header, options, failure, err);
    if (rc == YVEX_OK) rc = deepseek_build_graph(&deepseek);
    if (rc == YVEX_OK &&
        deepseek.terminal_ordinal != YVEX_DEEPSEEK_TRANSFORM_TERMINAL_COUNT)
        rc = deepseek_refuse(
            &deepseek, YVEX_TRANSFORM_FAILURE_MISSING_TERMINAL,
            YVEX_DEEPSEEK_TRANSFORM_TERMINAL_COUNT,
            deepseek.terminal_ordinal, "deepseek_transform_build");
    if (rc == YVEX_OK)
        rc = yvex_transform_builder_seal(
            deepseek.builder, out, failure, err);
    yvex_transform_builder_release(&deepseek.builder);
    if (rc == YVEX_OK) {
        const yvex_transform_ir_summary *summary =
            yvex_transform_ir_summary_get(*out);
        if (!summary || !summary->complete ||
            summary->source_value_count !=
                YVEX_DEEPSEEK_TRANSFORM_SOURCE_COUNT ||
            summary->node_count != YVEX_DEEPSEEK_TRANSFORM_TERMINAL_COUNT ||
            summary->edge_count != YVEX_DEEPSEEK_TRANSFORM_SOURCE_COUNT ||
            summary->terminal_count !=
                YVEX_DEEPSEEK_TRANSFORM_TERMINAL_COUNT ||
            summary->maximum_fan_in != 512u ||
            summary->payload_bytes_read != 0u) {
            yvex_transform_ir_release(out);
            return deepseek_refuse(
                &deepseek, YVEX_TRANSFORM_FAILURE_SEAL,
                YVEX_DEEPSEEK_TRANSFORM_SOURCE_COUNT,
                summary ? summary->edge_count : 0u,
                "deepseek_transform_build");
        }
    }
    return rc;
}
