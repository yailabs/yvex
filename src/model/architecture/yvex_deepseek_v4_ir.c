/*
 * yvex_deepseek_v4_ir.c - canonical DeepSeek-V4-Flash architecture IR.
 *
 * Owner:
 *   src/model/architecture
 *
 * Owns:
 *   validation, normalization, allocation, immutable lifetime, layer
 *   derivation, and typed accessors for the exact DeepSeek-V4-Flash topology.
 *
 * Does not own:
 *   source/config IO, tensor-name discovery, tensor-role coverage, GGUF
 *   mapping, payload reads, qtype policy, materialization, runtime descriptors,
 *   graph execution, tokenizer execution, or generation.
 *
 * Invariants:
 *   only a successful exact-source verification may construct the IR; all 43
 *   main layers and the MTP layer are explicit; every rejected build publishes
 *   no object and releases partial allocations.
 *
 * Boundary:
 *   this module specifies what later artifact and runtime owners must consume;
 *   it does not implement or claim those capabilities.
 */
#include "yvex_deepseek_v4_ir.h"

#include "../../source/yvex_source_verify.h"
#include "../target/yvex_model_target_catalog.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEEPSEEK_V4_FLASH_MAIN_LAYERS 43ull
#define DEEPSEEK_V4_FLASH_AUX_LAYERS 1ull
#define DEEPSEEK_V4_MHC_SCALE_WIDTH 3ull
#define DEEPSEEK_V4_MHC_POST_MULTIPLIER 2.0

static const char deepseek_v4_paper_revision[] = "arXiv:2606.19348v1";
static const char deepseek_v4_sglang_revision[] =
    "96a04cb13f9c3ed86028e090784a9eb059cf5318";
static const char deepseek_v4_vllm_revision[] =
    "8df14cfc8c8a09b4e57f082e59593a3abce4ffb3";

struct yvex_deepseek_v4_ir {
    yvex_deepseek_v4_ir_allocator allocator;
    yvex_deepseek_v4_model_spec model;
    yvex_deepseek_v4_layer_spec *layers;
    yvex_deepseek_v4_auxiliary_spec *auxiliary;
};

typedef struct {
    double attention_dropout;
    double hc_epsilon;
    double rms_norm_epsilon;
    double routed_scaling_factor;
    double activation_limit;
    unsigned long long expanded_width;
    unsigned long long mixing_rows;
    unsigned long long shared_intermediate_size;
    unsigned long long output_heads_per_group;
    unsigned long long output_group_input_width;
    unsigned long long query_width;
    unsigned long long grouped_output_width;
    unsigned long long csa_indexer_rows;
    unsigned long long indexer_query_width;
} deepseek_v4_derived_geometry;

static void *deepseek_v4_default_allocate(size_t size, void *context)
{
    (void)context;
    return malloc(size);
}

static void deepseek_v4_default_release(void *allocation, void *context)
{
    (void)context;
    free(allocation);
}

static yvex_deepseek_v4_ir_allocator deepseek_v4_default_allocator(void)
{
    yvex_deepseek_v4_ir_allocator allocator;

    allocator.allocate = deepseek_v4_default_allocate;
    allocator.release = deepseek_v4_default_release;
    allocator.context = NULL;
    return allocator;
}

static void deepseek_v4_failure_clear(yvex_deepseek_v4_ir_failure *failure)
{
    if (!failure) return;
    memset(failure, 0, sizeof(*failure));
    failure->layer_index = YVEX_DEEPSEEK_V4_IR_NO_LAYER;
}

/* Records one typed construction refusal and the existing generic error view. */
static int deepseek_v4_reject(yvex_deepseek_v4_ir_failure *failure,
                              yvex_deepseek_v4_ir_failure_code code,
                              yvex_deepseek_v4_ir_component component,
                              const char *field,
                              unsigned long long layer_index,
                              unsigned long long expected,
                              unsigned long long actual,
                              yvex_error *err)
{
    yvex_status status = code == YVEX_DEEPSEEK_V4_IR_FAILURE_ALLOCATION
                             ? YVEX_ERR_NOMEM
                             : (code == YVEX_DEEPSEEK_V4_IR_FAILURE_INVALID_ARGUMENT
                                    ? YVEX_ERR_INVALID_ARG
                                    : YVEX_ERR_FORMAT);

    if (failure) {
        failure->code = code;
        failure->component = component;
        failure->field = field;
        failure->layer_index = layer_index;
        failure->expected = expected;
        failure->actual = actual;
    }
    yvex_error_setf(err, status, "deepseek_v4_arch_ir",
                    "%s:%s field=%s layer=%llu expected=%llu actual=%llu",
                    yvex_deepseek_v4_ir_component_name(component),
                    yvex_deepseek_v4_ir_failure_name(code),
                    field ? field : "none", layer_index, expected, actual);
    return status;
}

static int deepseek_v4_checked_add(unsigned long long left,
                                   unsigned long long right,
                                   unsigned long long *out)
{
    if (!out || ULLONG_MAX - left < right) return 0;
    *out = left + right;
    return 1;
}

static int deepseek_v4_checked_mul(unsigned long long left,
                                   unsigned long long right,
                                   unsigned long long *out)
{
    if (!out || (left != 0u && right > ULLONG_MAX / left)) return 0;
    *out = left * right;
    return 1;
}

static int deepseek_v4_parse_double(const char *text, double *out)
{
    char *end = NULL;
    double value;

    if (!text || !text[0] || !out) return 0;
    errno = 0;
    value = strtod(text, &end);
    if (errno == ERANGE || !end || *end != '\0' || !isfinite(value)) return 0;
    *out = value;
    return 1;
}

static void deepseek_v4_copy(char *out, size_t cap, const char *value)
{
    if (!out || cap == 0u) return;
    (void)snprintf(out, cap, "%s", value ? value : "");
}

/* Requires the full strict-source result without reopening any source owner. */
static int deepseek_v4_validate_source(
    const yvex_source_verification *verification,
    yvex_deepseek_v4_ir_failure *failure,
    yvex_error *err)
{
    const yvex_model_target_identity *identity =
        yvex_model_target_release_identity();

    if (!verification) {
        return deepseek_v4_reject(
            failure, YVEX_DEEPSEEK_V4_IR_FAILURE_INVALID_ARGUMENT,
            YVEX_DEEPSEEK_V4_IR_COMPONENT_SOURCE, "verification",
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, 1u, 0u, err);
    }
    if (!verification->verified || verification->blocker_count != 0u ||
        !verification->path_verified || !verification->repository_verified ||
        !verification->revision_verified || !verification->manifest_verified ||
        !verification->manifest_reopened || !verification->config_valid ||
        !verification->tokenizer_json_valid ||
        !verification->tokenizer_config_valid ||
        !verification->generation_config_valid ||
        !verification->shard_index_headers_match ||
        verification->header_scan_count != 1u ||
        verification->header_shard_count == 0u ||
        verification->header_tensor_count == 0u) {
        return deepseek_v4_reject(
            failure, YVEX_DEEPSEEK_V4_IR_FAILURE_SOURCE_NOT_VERIFIED,
            YVEX_DEEPSEEK_V4_IR_COMPONENT_SOURCE, "strict-verification",
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, 1u,
            verification->verified ? 1u : 0u, err);
    }
    if (strcmp(verification->manifest_target_id, identity->target_id) != 0 ||
        strcmp(verification->repository_id, identity->upstream_repo_id) != 0 ||
        strcmp(verification->revision, identity->upstream_revision) != 0 ||
        strcmp(verification->model_type, identity->config_model_type) != 0 ||
        strcmp(verification->architecture, identity->config_architecture) != 0 ||
        strcmp(verification->source_kind, "huggingface") != 0) {
        return deepseek_v4_reject(
            failure, YVEX_DEEPSEEK_V4_IR_FAILURE_IDENTITY_MISMATCH,
            YVEX_DEEPSEEK_V4_IR_COMPONENT_IDENTITY, "canonical-target",
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, 1u, 0u, err);
    }
    if ((strcmp(verification->verification_stage,
                "exact-source-metadata-header-verified") != 0 &&
         (strcmp(verification->verification_stage,
                 "exact-source-payload-verified") != 0 ||
          !verification->manifest_payload_trusted)) ||
        strcmp(verification->inventory_authority, "upstream-index") != 0 ||
        !verification->shard_index_present ||
        !verification->shard_index_valid ||
        !verification->upstream_index_identity_verified ||
        strcmp(verification->upstream_index_oid,
               identity->upstream_index_oid) != 0 ||
        strcmp(verification->local_index_oid,
               identity->upstream_index_oid) != 0) {
        return deepseek_v4_reject(
            failure, YVEX_DEEPSEEK_V4_IR_FAILURE_SOURCE_FACT_MISSING,
            YVEX_DEEPSEEK_V4_IR_COMPONENT_SOURCE,
            "verification-stage-or-pinned-index-authority",
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, 1u, 0u, err);
    }
    return YVEX_OK;
}

/* Validates exact cross-field geometry and derives checked reusable widths. */
static int deepseek_v4_validate_geometry(
    const yvex_source_verification *source,
    deepseek_v4_derived_geometry *geometry,
    yvex_deepseek_v4_ir_failure *failure,
    yvex_error *err)
{
    unsigned long long schedule_count;
    unsigned long long intermediate;

    memset(geometry, 0, sizeof(*geometry));
    if (source->num_hidden_layers != DEEPSEEK_V4_FLASH_MAIN_LAYERS ||
        source->num_nextn_predict_layers != DEEPSEEK_V4_FLASH_AUX_LAYERS ||
        source->hidden_size == 0u || source->vocab_size == 0u ||
        source->max_position_embeddings == 0u ||
        source->num_attention_heads == 0u ||
        source->num_key_value_heads != 1u || source->head_dim == 0u ||
        source->qk_rope_head_dim == 0u ||
        source->qk_rope_head_dim >= source->head_dim ||
        source->q_lora_rank == 0u || source->o_lora_rank == 0u) {
        return deepseek_v4_reject(
            failure, YVEX_DEEPSEEK_V4_IR_FAILURE_INVALID_DIMENSION,
            YVEX_DEEPSEEK_V4_IR_COMPONENT_MODEL, "global-geometry",
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, 1u, 0u, err);
    }
    if (!deepseek_v4_checked_add(source->num_hidden_layers,
                                 source->num_nextn_predict_layers,
                                 &schedule_count)) {
        return deepseek_v4_reject(
            failure, YVEX_DEEPSEEK_V4_IR_FAILURE_ARITHMETIC_OVERFLOW,
            YVEX_DEEPSEEK_V4_IR_COMPONENT_ATTENTION, "schedule-count",
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, 0u, 0u, err);
    }
    if (source->compress_ratio_count != schedule_count) {
        return deepseek_v4_reject(
            failure, YVEX_DEEPSEEK_V4_IR_FAILURE_SCHEDULE_LENGTH,
            YVEX_DEEPSEEK_V4_IR_COMPONENT_ATTENTION, "compress-ratios",
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, schedule_count,
            source->compress_ratio_count, err);
    }
    if (source->o_groups == 0u ||
        source->num_attention_heads % source->o_groups != 0u) {
        return deepseek_v4_reject(
            failure, YVEX_DEEPSEEK_V4_IR_FAILURE_INVALID_GROUP_GEOMETRY,
            YVEX_DEEPSEEK_V4_IR_COMPONENT_ATTENTION, "output-groups",
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, source->o_groups,
            source->num_attention_heads, err);
    }
    geometry->output_heads_per_group =
        source->num_attention_heads / source->o_groups;
    if (!deepseek_v4_checked_mul(geometry->output_heads_per_group,
                                 source->head_dim,
                                 &geometry->output_group_input_width)) {
        return deepseek_v4_reject(
            failure, YVEX_DEEPSEEK_V4_IR_FAILURE_ARITHMETIC_OVERFLOW,
            YVEX_DEEPSEEK_V4_IR_COMPONENT_ATTENTION,
            "output-group-input-width", YVEX_DEEPSEEK_V4_IR_NO_LAYER,
            0u, 0u, err);
    }
    if (!deepseek_v4_checked_mul(source->num_attention_heads,
                                 source->head_dim,
                                 &geometry->query_width) ||
        !deepseek_v4_checked_mul(source->o_lora_rank, source->o_groups,
                                 &geometry->grouped_output_width) ||
        !deepseek_v4_checked_mul(4u, source->index_n_heads,
                                 &geometry->csa_indexer_rows) ||
        !deepseek_v4_checked_mul(source->index_n_heads,
                                 source->index_head_dim,
                                 &geometry->indexer_query_width)) {
        return deepseek_v4_reject(
            failure, YVEX_DEEPSEEK_V4_IR_FAILURE_ARITHMETIC_OVERFLOW,
            YVEX_DEEPSEEK_V4_IR_COMPONENT_ATTENTION,
            "attention-derived-width", YVEX_DEEPSEEK_V4_IR_NO_LAYER,
            0u, 0u, err);
    }
    if (source->sliding_window == 0u ||
        source->sliding_window > source->max_position_embeddings ||
        !source->use_cache ||
        strcmp(source->rope_scaling_type, "yarn") != 0 ||
        source->rope_scaling_factor == 0u ||
        source->rope_original_context == 0u ||
        source->rope_original_context > source->max_position_embeddings ||
        source->rope_beta_fast <= source->rope_beta_slow ||
        source->rope_theta == 0u || source->compress_rope_theta == 0u) {
        return deepseek_v4_reject(
            failure, YVEX_DEEPSEEK_V4_IR_FAILURE_INVALID_POSITION,
            YVEX_DEEPSEEK_V4_IR_COMPONENT_POSITION,
            "rope-context-and-cache-state",
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, 1u, 0u, err);
    }
    if (!deepseek_v4_parse_double(source->attention_dropout,
                                  &geometry->attention_dropout) ||
        geometry->attention_dropout < 0.0 ||
        !deepseek_v4_parse_double(source->hc_eps, &geometry->hc_epsilon) ||
        geometry->hc_epsilon <= 0.0 ||
        !deepseek_v4_parse_double(source->rms_norm_eps,
                                  &geometry->rms_norm_epsilon) ||
        geometry->rms_norm_epsilon <= 0.0 ||
        !deepseek_v4_parse_double(source->routed_scaling_factor,
                                  &geometry->routed_scaling_factor) ||
        geometry->routed_scaling_factor <= 0.0 ||
        !deepseek_v4_parse_double(source->swiglu_limit,
                                  &geometry->activation_limit) ||
        geometry->activation_limit <= 0.0) {
        return deepseek_v4_reject(
            failure, YVEX_DEEPSEEK_V4_IR_FAILURE_NUMERIC_VALUE,
            YVEX_DEEPSEEK_V4_IR_COMPONENT_MODEL, "numeric-config",
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, 1u, 0u, err);
    }
    if (source->hc_mult == 0u || source->hc_sinkhorn_iters == 0u) {
        return deepseek_v4_reject(
            failure, YVEX_DEEPSEEK_V4_IR_FAILURE_INVALID_MHC,
            YVEX_DEEPSEEK_V4_IR_COMPONENT_MHC, "mhc-geometry",
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, 1u, 0u, err);
    }
    if (!deepseek_v4_checked_mul(source->hc_mult, source->hidden_size,
                                 &geometry->expanded_width) ||
        !deepseek_v4_checked_add(2u, source->hc_mult, &intermediate) ||
        !deepseek_v4_checked_mul(intermediate, source->hc_mult,
                                 &geometry->mixing_rows)) {
        return deepseek_v4_reject(
            failure, YVEX_DEEPSEEK_V4_IR_FAILURE_ARITHMETIC_OVERFLOW,
            YVEX_DEEPSEEK_V4_IR_COMPONENT_MHC, "mhc-geometry",
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, 0u, source->hc_mult, err);
    }
    if (source->n_routed_experts == 0u ||
        source->n_shared_experts == 0u ||
        source->moe_intermediate_size == 0u ||
        source->num_hash_layers > source->num_hidden_layers) {
        return deepseek_v4_reject(
            failure, YVEX_DEEPSEEK_V4_IR_FAILURE_INVALID_ROUTING,
            YVEX_DEEPSEEK_V4_IR_COMPONENT_MOE, "expert-or-hash-geometry",
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, source->num_hidden_layers,
            source->num_hash_layers, err);
    }
    if (source->num_experts_per_tok == 0u ||
        source->num_experts_per_tok > source->n_routed_experts) {
        return deepseek_v4_reject(
            failure, YVEX_DEEPSEEK_V4_IR_FAILURE_INVALID_EXPERT_TOPK,
            YVEX_DEEPSEEK_V4_IR_COMPONENT_MOE, "experts-per-token",
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, source->n_routed_experts,
            source->num_experts_per_tok, err);
    }
    if (!deepseek_v4_checked_mul(source->n_shared_experts,
                                 source->moe_intermediate_size,
                                 &geometry->shared_intermediate_size)) {
        return deepseek_v4_reject(
            failure, YVEX_DEEPSEEK_V4_IR_FAILURE_ARITHMETIC_OVERFLOW,
            YVEX_DEEPSEEK_V4_IR_COMPONENT_MOE,
            "shared-expert-intermediate", YVEX_DEEPSEEK_V4_IR_NO_LAYER,
            0u, 0u, err);
    }
    if (source->index_head_dim == 0u || source->index_n_heads == 0u ||
        source->index_topk == 0u ||
        source->index_topk > source->max_position_embeddings) {
        return deepseek_v4_reject(
            failure, YVEX_DEEPSEEK_V4_IR_FAILURE_INVALID_DIMENSION,
            YVEX_DEEPSEEK_V4_IR_COMPONENT_ATTENTION, "indexer-geometry",
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, 1u, 0u, err);
    }
    if (source->tokenizer_effective_vocab_size != source->vocab_size ||
        source->tokenizer_model_max_length !=
            source->max_position_embeddings ||
        source->bos_token_id != source->generation_bos_token_id ||
        source->eos_token_id != source->generation_eos_token_id ||
        source->bos_token_id >= source->vocab_size ||
        source->eos_token_id >= source->vocab_size ||
        source->bos_token_id == source->eos_token_id) {
        return deepseek_v4_reject(
            failure,
            YVEX_DEEPSEEK_V4_IR_FAILURE_TOKENIZER_OUTPUT_MISMATCH,
            YVEX_DEEPSEEK_V4_IR_COMPONENT_TOKENIZER,
            "vocabulary-context-or-special-token", YVEX_DEEPSEEK_V4_IR_NO_LAYER,
            source->vocab_size, source->tokenizer_effective_vocab_size, err);
    }
    if (strcmp(source->torch_dtype, "bfloat16") != 0 ||
        strcmp(source->expert_dtype, "fp4") != 0 ||
        strcmp(source->quant_method, "fp8") != 0 ||
        strcmp(source->quant_format, "e4m3") != 0 ||
        strcmp(source->quant_scale_format, "ue8m0") != 0 ||
        strcmp(source->quant_activation_scheme, "dynamic") != 0 ||
        source->quant_block_rows != 128u ||
        source->quant_block_columns != 128u) {
        return deepseek_v4_reject(
            failure,
            YVEX_DEEPSEEK_V4_IR_FAILURE_UNSUPPORTED_SOURCE_CONSTRAINT,
            YVEX_DEEPSEEK_V4_IR_COMPONENT_SOURCE_CONSTRAINT,
            "source-dtype-or-quantization", YVEX_DEEPSEEK_V4_IR_NO_LAYER,
            1u, 0u, err);
    }
    if (strcmp(source->hidden_act, "silu") != 0 ||
        strcmp(source->scoring_func, "sqrtsoftplus") != 0 ||
        strcmp(source->topk_method, "noaux_tc") != 0) {
        return deepseek_v4_reject(
            failure, YVEX_DEEPSEEK_V4_IR_FAILURE_INVALID_ROUTING,
            YVEX_DEEPSEEK_V4_IR_COMPONENT_MOE,
            "activation-scoring-or-topk-policy",
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, 1u, 0u, err);
    }
    return YVEX_OK;
}

/* Validates the source schedule against the pinned SWA/CSA/HCA topology. */
static int deepseek_v4_validate_schedule(
    const yvex_source_verification *source,
    yvex_deepseek_v4_ir_failure *failure,
    yvex_error *err)
{
    unsigned long long i;

    for (i = 0u; i < source->compress_ratio_count; ++i) {
        unsigned long long ratio = source->compress_ratios[i];
        unsigned long long expected;

        if (ratio != 0u && ratio != 4u && ratio != 128u) {
            return deepseek_v4_reject(
                failure,
                YVEX_DEEPSEEK_V4_IR_FAILURE_UNSUPPORTED_COMPRESSION,
                YVEX_DEEPSEEK_V4_IR_COMPONENT_ATTENTION,
                "compression-ratio", i, 0u, ratio, err);
        }
        if (i < 2u) {
            expected = 0u;
        } else if (i < source->num_hidden_layers) {
            expected = i % 2u == 0u ? 4u : 128u;
        } else {
            expected = 0u;
        }
        if (ratio != expected) {
            return deepseek_v4_reject(
                failure, YVEX_DEEPSEEK_V4_IR_FAILURE_SCHEDULE_PATTERN,
                i < source->num_hidden_layers
                    ? YVEX_DEEPSEEK_V4_IR_COMPONENT_ATTENTION
                    : YVEX_DEEPSEEK_V4_IR_COMPONENT_AUXILIARY,
                "compression-schedule", i, expected, ratio, err);
        }
    }
    return YVEX_OK;
}

static void deepseek_v4_fill_mhc(
    yvex_deepseek_v4_mhc_spec *mhc,
    const yvex_source_verification *source,
    const deepseek_v4_derived_geometry *geometry,
    yvex_deepseek_v4_mhc_entry entry)
{
    memset(mhc, 0, sizeof(*mhc));
    mhc->residual_streams = source->hc_mult;
    mhc->stream_width = source->hidden_size;
    mhc->expanded_width = geometry->expanded_width;
    mhc->mixing_rows = geometry->mixing_rows;
    mhc->mixing_columns = geometry->expanded_width;
    mhc->base_width = geometry->mixing_rows;
    mhc->scale_width = DEEPSEEK_V4_MHC_SCALE_WIDTH;
    mhc->sinkhorn_iterations = source->hc_sinkhorn_iters;
    mhc->epsilon = geometry->hc_epsilon;
    mhc->residual_post_multiplier = DEEPSEEK_V4_MHC_POST_MULTIPLIER;
    mhc->entry = entry;
    mhc->attention_pre_and_post = 1;
    mhc->ffn_pre_and_deferred_post = 1;
}

static void deepseek_v4_fill_moe(
    yvex_deepseek_v4_moe_spec *moe,
    const yvex_source_verification *source,
    const deepseek_v4_derived_geometry *geometry,
    unsigned long long layer_index)
{
    memset(moe, 0, sizeof(*moe));
    moe->router_class = layer_index < source->num_hash_layers
                            ? YVEX_DEEPSEEK_V4_ROUTER_HASH_TOKEN_ID
                            : YVEX_DEEPSEEK_V4_ROUTER_LEARNED_HIDDEN_STATE;
    moe->scoring = YVEX_DEEPSEEK_V4_SCORING_SQRT_SOFTPLUS;
    moe->topk_policy = YVEX_DEEPSEEK_V4_TOPK_NOAUX_TC;
    moe->activation = YVEX_DEEPSEEK_V4_ACTIVATION_SILU;
    moe->routed_experts = source->n_routed_experts;
    moe->shared_experts = source->n_shared_experts;
    moe->experts_per_token = source->num_experts_per_tok;
    moe->expert_intermediate_size = source->moe_intermediate_size;
    moe->shared_intermediate_size = geometry->shared_intermediate_size;
    moe->routed_scaling_factor = geometry->routed_scaling_factor;
    moe->activation_limit = geometry->activation_limit;
    moe->normalize_topk_probabilities = source->norm_topk_prob;
    if (moe->router_class == YVEX_DEEPSEEK_V4_ROUTER_HASH_TOKEN_ID) {
        moe->requires_token_ids = 1;
        moe->hash_table_rows = source->vocab_size;
        moe->hash_table_columns = source->num_experts_per_tok;
    } else {
        moe->requires_hidden_state = 1;
        moe->requires_correction_bias = 1;
        moe->correction_bias_width = source->n_routed_experts;
    }
}

/* Derives one explicit layer descriptor from one validated schedule entry. */
static void deepseek_v4_fill_layer(
    yvex_deepseek_v4_layer_spec *layer,
    const yvex_source_verification *source,
    const deepseek_v4_derived_geometry *geometry,
    unsigned long long layer_index,
    int auxiliary)
{
    unsigned long long ratio = source->compress_ratios[layer_index];
    memset(layer, 0, sizeof(*layer));
    layer->layer_index = layer_index;
    layer->compression_ratio = ratio;
    layer->query_heads = source->num_attention_heads;
    layer->kv_heads = source->num_key_value_heads;
    layer->head_dimension = source->head_dim;
    layer->rope_head_dimension = source->qk_rope_head_dim;
    layer->non_rope_head_dimension =
        source->head_dim - source->qk_rope_head_dim;
    layer->query_lora_rank = source->q_lora_rank;
    layer->output_lora_rank = source->o_lora_rank;
    layer->output_groups = source->o_groups;
    layer->output_heads_per_group = geometry->output_heads_per_group;
    layer->output_group_input_width = geometry->output_group_input_width;
    layer->attention_sink_count = source->num_attention_heads;
    layer->attention_dropout = geometry->attention_dropout;
    layer->causal = 1;
    layer->attention_bias = source->attention_bias;
    layer->query_norm_required = 1;
    layer->kv_norm_required = 1;
    layer->attention_input_norm.required = 1;
    layer->attention_input_norm.width = source->hidden_size;
    layer->post_attention_ffn_norm.required = 1;
    layer->post_attention_ffn_norm.width = source->hidden_size;
    layer->tensors.q_a_rows = source->q_lora_rank;
    layer->tensors.q_a_columns = source->hidden_size;
    layer->tensors.q_b_rows = geometry->query_width;
    layer->tensors.q_b_columns = source->q_lora_rank;
    layer->tensors.kv_rows = source->head_dim;
    layer->tensors.kv_columns = source->hidden_size;
    layer->tensors.o_a_rows = geometry->grouped_output_width;
    layer->tensors.o_a_columns = source->hidden_size;
    layer->tensors.o_b_rows = source->hidden_size;
    layer->tensors.o_b_columns = geometry->grouped_output_width;
    layer->position.rope_dimension = source->qk_rope_head_dim;
    layer->position.theta = ratio == 0u ? source->rope_theta
                                       : source->compress_rope_theta;
    layer->position.scaling_factor = source->rope_scaling_factor;
    layer->position.original_context = source->rope_original_context;
    layer->position.beta_fast = source->rope_beta_fast;
    layer->position.beta_slow = source->rope_beta_slow;
    layer->position.maximum_context = source->max_position_embeddings;
    layer->position.partial_rope = 1;
    layer->position.inverse_output_rotation = 1;
    layer->kv.compression_ratio = ratio;
    layer->kv.sliding_window = source->sliding_window;
    layer->kv.requires_state_cache = 1;
    if (ratio == 0u) {
        layer->attention_class = YVEX_DEEPSEEK_V4_ATTENTION_SWA;
        layer->kv.class_id = YVEX_DEEPSEEK_V4_KV_SWA;
    } else if (ratio == 4u) {
        layer->attention_class = YVEX_DEEPSEEK_V4_ATTENTION_CSA;
        layer->kv.class_id = YVEX_DEEPSEEK_V4_KV_CSA;
        layer->compressor_required = 1;
        layer->indexer_required = 1;
        layer->indexer_heads = source->index_n_heads;
        layer->indexer_head_dimension = source->index_head_dim;
        layer->indexer_topk = source->index_topk;
        layer->kv.requires_uncompressed_tail = 1;
        layer->kv.requires_compressed_core = 1;
        layer->kv.requires_indexer_cache = 1;
        layer->tensors.compressor_ape_rows = ratio;
        layer->tensors.compressor_ape_columns = source->q_lora_rank;
        layer->tensors.compressor_norm_width = source->head_dim;
        layer->tensors.compressor_projection_rows = source->q_lora_rank;
        layer->tensors.compressor_projection_columns = source->hidden_size;
        layer->tensors.indexer_ape_rows = ratio;
        layer->tensors.indexer_ape_columns = geometry->csa_indexer_rows;
        layer->tensors.indexer_norm_width = source->index_head_dim;
        layer->tensors.indexer_projection_rows =
            geometry->csa_indexer_rows;
        layer->tensors.indexer_projection_columns = source->hidden_size;
        layer->tensors.indexer_query_rows =
            geometry->indexer_query_width;
        layer->tensors.indexer_query_columns = source->q_lora_rank;
        layer->tensors.indexer_weight_rows = source->index_n_heads;
        layer->tensors.indexer_weight_columns = source->hidden_size;
    } else {
        layer->attention_class = YVEX_DEEPSEEK_V4_ATTENTION_HCA;
        layer->kv.class_id = YVEX_DEEPSEEK_V4_KV_HCA;
        layer->compressor_required = 1;
        layer->kv.requires_uncompressed_tail = 1;
        layer->kv.requires_compressed_core = 1;
        layer->tensors.compressor_ape_rows = ratio;
        layer->tensors.compressor_ape_columns = source->head_dim;
        layer->tensors.compressor_norm_width = source->head_dim;
        layer->tensors.compressor_projection_rows = source->head_dim;
        layer->tensors.compressor_projection_columns = source->hidden_size;
    }
    deepseek_v4_fill_mhc(
        &layer->mhc, source, geometry,
        auxiliary || layer_index == 0u
            ? YVEX_DEEPSEEK_V4_MHC_STANDALONE_PRE
            : YVEX_DEEPSEEK_V4_MHC_FUSED_PRIOR_POST_PRE);
    deepseek_v4_fill_moe(&layer->moe, source, geometry, layer_index);
    layer->rms_norm_epsilon = geometry->rms_norm_epsilon;
}

static void deepseek_v4_fill_model(
    yvex_deepseek_v4_ir *ir,
    const yvex_source_verification *source,
    const deepseek_v4_derived_geometry *geometry)
{
    const yvex_model_target_identity *identity =
        yvex_model_target_release_identity();
    yvex_deepseek_v4_model_spec *model = &ir->model;

    deepseek_v4_copy(model->target_id, sizeof(model->target_id),
                     identity->target_id);
    deepseek_v4_copy(model->family, sizeof(model->family),
                     identity->family_key);
    deepseek_v4_copy(model->architecture, sizeof(model->architecture),
                     source->architecture);
    deepseek_v4_copy(model->repository, sizeof(model->repository),
                     source->repository_id);
    deepseek_v4_copy(model->revision, sizeof(model->revision),
                     source->revision);
    deepseek_v4_copy(model->verification_stage,
                     sizeof(model->verification_stage),
                     source->verification_stage);
    deepseek_v4_copy(model->paper_revision, sizeof(model->paper_revision),
                     deepseek_v4_paper_revision);
    deepseek_v4_copy(model->sglang_revision, sizeof(model->sglang_revision),
                     deepseek_v4_sglang_revision);
    deepseek_v4_copy(model->vllm_revision, sizeof(model->vllm_revision),
                     deepseek_v4_vllm_revision);
    model->hidden_size = source->hidden_size;
    model->vocabulary_size = source->vocab_size;
    model->maximum_context = source->max_position_embeddings;
    model->main_layer_count = source->num_hidden_layers;
    model->auxiliary_layer_count = source->num_nextn_predict_layers;
    model->source_header_scan_count = source->header_scan_count;
    model->source_header_tensor_count = source->header_tensor_count;
    model->source_payload_bytes_read = 0u;
    model->embedding.required = 1;
    model->embedding.vocabulary_size = source->vocab_size;
    model->embedding.hidden_size = source->hidden_size;
    model->output.required = 1;
    model->output.tied_to_embedding = source->tie_word_embeddings;
    model->output.input_width = source->hidden_size;
    model->output.vocabulary_size = source->vocab_size;
    deepseek_v4_copy(model->tokenizer.tokenizer_class,
                     sizeof(model->tokenizer.tokenizer_class),
                     source->tokenizer_class);
    deepseek_v4_copy(model->tokenizer.model_type,
                     sizeof(model->tokenizer.model_type),
                     source->tokenizer_model_type);
    model->tokenizer.vocabulary_size = source->tokenizer_effective_vocab_size;
    model->tokenizer.base_vocab_entries = source->tokenizer_base_vocab_count;
    model->tokenizer.added_token_entries = source->tokenizer_added_token_count;
    model->tokenizer.maximum_token_id = source->tokenizer_max_token_id;
    model->tokenizer.maximum_context = source->tokenizer_model_max_length;
    model->tokenizer.bos_token_id = source->bos_token_id;
    model->tokenizer.eos_token_id = source->eos_token_id;
    model->tokenizer.bos_required = 1;
    model->tokenizer.eos_required = 1;
    model->source_constraint.weight_dtype =
        YVEX_DEEPSEEK_V4_SOURCE_WEIGHT_BF16;
    model->source_constraint.expert_dtype =
        YVEX_DEEPSEEK_V4_SOURCE_EXPERT_FP4;
    model->source_constraint.quantization =
        YVEX_DEEPSEEK_V4_SOURCE_QUANT_FP8_E4M3_UE8M0_DYNAMIC;
    model->source_constraint.quant_block_rows = source->quant_block_rows;
    model->source_constraint.quant_block_columns =
        source->quant_block_columns;
    model->source_constraint.fp4_packing_factor = 2u;
    model->source_constraint.fp4_scale_group_width = 32u;
    model->source_constraint.fp4_physical_dtype = YVEX_NATIVE_DTYPE_I8;
    model->source_constraint.scale_dtype = YVEX_NATIVE_DTYPE_F8_E8M0;
    deepseek_v4_fill_mhc(&model->final_mhc, source, geometry,
                         YVEX_DEEPSEEK_V4_MHC_FUSED_PRIOR_POST_PRE);
    model->final_norm_epsilon = geometry->rms_norm_epsilon;
    model->use_cache = source->use_cache;
    model->final_mhc_post_required = 1;
    model->final_mhc_head_required = 1;
    model->final_mhc_head.required = 1;
    model->final_mhc_head.function_rows = source->hc_mult;
    model->final_mhc_head.function_columns = geometry->expanded_width;
    model->final_mhc_head.base_width = source->hc_mult;
    model->final_mhc_head.scale_width = 1u;
    model->final_norm_after_mhc_head = 1;
}

/* Allocates and constructs the immutable object after all source checks pass. */
static int deepseek_v4_construct(
    yvex_deepseek_v4_ir **out,
    const yvex_source_verification *source,
    const yvex_deepseek_v4_ir_allocator *allocator,
    const deepseek_v4_derived_geometry *geometry,
    yvex_deepseek_v4_ir_failure *failure,
    yvex_error *err)
{
    yvex_deepseek_v4_ir *ir;
    unsigned long long i;

    ir = (yvex_deepseek_v4_ir *)allocator->allocate(sizeof(*ir),
                                                    allocator->context);
    if (!ir) {
        return deepseek_v4_reject(
            failure, YVEX_DEEPSEEK_V4_IR_FAILURE_ALLOCATION,
            YVEX_DEEPSEEK_V4_IR_COMPONENT_ALLOCATION, "ir",
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, sizeof(*ir), 0u, err);
    }
    memset(ir, 0, sizeof(*ir));
    ir->allocator = *allocator;
    if (source->num_hidden_layers > (unsigned long long)(SIZE_MAX /
            sizeof(*ir->layers))) {
        yvex_deepseek_v4_ir_close(ir);
        return deepseek_v4_reject(
            failure, YVEX_DEEPSEEK_V4_IR_FAILURE_ARITHMETIC_OVERFLOW,
            YVEX_DEEPSEEK_V4_IR_COMPONENT_ALLOCATION, "layers-bytes",
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, 0u,
            source->num_hidden_layers, err);
    }
    ir->layers = (yvex_deepseek_v4_layer_spec *)allocator->allocate(
        (size_t)source->num_hidden_layers * sizeof(*ir->layers),
        allocator->context);
    if (!ir->layers) {
        yvex_deepseek_v4_ir_close(ir);
        return deepseek_v4_reject(
            failure, YVEX_DEEPSEEK_V4_IR_FAILURE_ALLOCATION,
            YVEX_DEEPSEEK_V4_IR_COMPONENT_ALLOCATION, "layers",
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, source->num_hidden_layers, 0u, err);
    }
    memset(ir->layers, 0,
           (size_t)source->num_hidden_layers * sizeof(*ir->layers));
    if (source->num_nextn_predict_layers >
        (unsigned long long)(SIZE_MAX / sizeof(*ir->auxiliary))) {
        yvex_deepseek_v4_ir_close(ir);
        return deepseek_v4_reject(
            failure, YVEX_DEEPSEEK_V4_IR_FAILURE_ARITHMETIC_OVERFLOW,
            YVEX_DEEPSEEK_V4_IR_COMPONENT_ALLOCATION, "auxiliary-bytes",
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, 0u,
            source->num_nextn_predict_layers, err);
    }
    ir->auxiliary = (yvex_deepseek_v4_auxiliary_spec *)allocator->allocate(
        (size_t)source->num_nextn_predict_layers * sizeof(*ir->auxiliary),
        allocator->context);
    if (!ir->auxiliary) {
        yvex_deepseek_v4_ir_close(ir);
        return deepseek_v4_reject(
            failure, YVEX_DEEPSEEK_V4_IR_FAILURE_ALLOCATION,
            YVEX_DEEPSEEK_V4_IR_COMPONENT_ALLOCATION, "auxiliary",
            YVEX_DEEPSEEK_V4_IR_NO_LAYER,
            source->num_nextn_predict_layers, 0u, err);
    }
    memset(ir->auxiliary, 0,
           (size_t)source->num_nextn_predict_layers * sizeof(*ir->auxiliary));
    deepseek_v4_fill_model(ir, source, geometry);
    for (i = 0u; i < source->num_hidden_layers; ++i) {
        deepseek_v4_fill_layer(&ir->layers[i], source, geometry, i, 0);
        if (ir->layers[i].attention_class ==
            YVEX_DEEPSEEK_V4_ATTENTION_SWA) ir->model.swa_layer_count++;
        if (ir->layers[i].attention_class ==
            YVEX_DEEPSEEK_V4_ATTENTION_CSA) ir->model.csa_layer_count++;
        if (ir->layers[i].attention_class ==
            YVEX_DEEPSEEK_V4_ATTENTION_HCA) ir->model.hca_layer_count++;
        if (ir->layers[i].moe.router_class ==
            YVEX_DEEPSEEK_V4_ROUTER_HASH_TOKEN_ID) {
            ir->model.hash_router_layer_count++;
        } else {
            ir->model.learned_router_layer_count++;
        }
    }
    for (i = 0u; i < source->num_nextn_predict_layers; ++i) {
        yvex_deepseek_v4_auxiliary_spec *aux = &ir->auxiliary[i];
        unsigned long long layer_index = source->num_hidden_layers + i;

        deepseek_v4_fill_layer(&aux->layer, source, geometry, layer_index, 1);
        aux->predictor_index = i;
        aux->previous_hidden_width = geometry->expanded_width;
        aux->embedding_projection_input = source->hidden_size;
        aux->embedding_projection_output = source->hidden_size;
        aux->hidden_projection_input = source->hidden_size;
        aux->hidden_projection_output = source->hidden_size;
        aux->requires_token_embedding = 1;
        aux->requires_previous_hidden_state = 1;
        aux->requires_embedding_norm = 1;
        aux->requires_hidden_norm = 1;
        aux->requires_separate_mhc_head = 1;
        aux->mhc_head.required = 1;
        aux->mhc_head.function_rows = source->hc_mult;
        aux->mhc_head.function_columns = geometry->expanded_width;
        aux->mhc_head.base_width = source->hc_mult;
        aux->mhc_head.scale_width = 1u;
        aux->shares_output_head = 1;
        aux->shares_final_norm = 1;
    }
    *out = ir;
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Validates verified facts and publishes an owned IR through caller allocation policy. */
int yvex_deepseek_v4_ir_build_with_allocator(
    yvex_deepseek_v4_ir **out,
    const struct yvex_source_verification *verification,
    const yvex_deepseek_v4_ir_allocator *allocator,
    yvex_deepseek_v4_ir_failure *failure,
    yvex_error *err)
{
    deepseek_v4_derived_geometry geometry;
    int rc;

    if (out) *out = NULL;
    deepseek_v4_failure_clear(failure);
    yvex_error_clear(err);
    if (!out || !allocator || !allocator->allocate || !allocator->release) {
        return deepseek_v4_reject(
            failure, YVEX_DEEPSEEK_V4_IR_FAILURE_INVALID_ARGUMENT,
            YVEX_DEEPSEEK_V4_IR_COMPONENT_ALLOCATION, "allocator-or-output",
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, 1u, 0u, err);
    }
    rc = deepseek_v4_validate_source(verification, failure, err);
    if (rc != YVEX_OK) return rc;
    rc = deepseek_v4_validate_geometry(verification, &geometry, failure, err);
    if (rc != YVEX_OK) return rc;
    rc = deepseek_v4_validate_schedule(verification, failure, err);
    if (rc != YVEX_OK) return rc;
    return deepseek_v4_construct(out, verification, allocator, &geometry,
                                 failure, err);
}

/* Builds the production IR with heap ownership and no source-side effects. */
int yvex_deepseek_v4_ir_build(
    yvex_deepseek_v4_ir **out,
    const struct yvex_source_verification *verification,
    yvex_deepseek_v4_ir_failure *failure,
    yvex_error *err)
{
    yvex_deepseek_v4_ir_allocator allocator =
        deepseek_v4_default_allocator();

    return yvex_deepseek_v4_ir_build_with_allocator(
        out, verification, &allocator, failure, err);
}

/* Releases a fully or partially owned IR; NULL is a safe no-op. */
void yvex_deepseek_v4_ir_close(yvex_deepseek_v4_ir *ir)
{
    yvex_deepseek_v4_ir_allocator allocator;

    if (!ir) return;
    allocator = ir->allocator;
    if (ir->auxiliary) allocator.release(ir->auxiliary, allocator.context);
    if (ir->layers) allocator.release(ir->layers, allocator.context);
    memset(ir, 0, sizeof(*ir));
    allocator.release(ir, allocator.context);
}

const yvex_deepseek_v4_model_spec *yvex_deepseek_v4_ir_model(
    const yvex_deepseek_v4_ir *ir)
{
    return ir ? &ir->model : NULL;
}

unsigned long long yvex_deepseek_v4_ir_layer_count(
    const yvex_deepseek_v4_ir *ir)
{
    return ir ? ir->model.main_layer_count : 0u;
}

const yvex_deepseek_v4_layer_spec *yvex_deepseek_v4_ir_layer_at(
    const yvex_deepseek_v4_ir *ir,
    unsigned long long index)
{
    return ir && index < ir->model.main_layer_count ? &ir->layers[index]
                                                     : NULL;
}

unsigned long long yvex_deepseek_v4_ir_auxiliary_count(
    const yvex_deepseek_v4_ir *ir)
{
    return ir ? ir->model.auxiliary_layer_count : 0u;
}

const yvex_deepseek_v4_auxiliary_spec *yvex_deepseek_v4_ir_auxiliary_at(
    const yvex_deepseek_v4_ir *ir,
    unsigned long long index)
{
    return ir && index < ir->model.auxiliary_layer_count
               ? &ir->auxiliary[index]
               : NULL;
}

const char *yvex_deepseek_v4_ir_failure_name(
    yvex_deepseek_v4_ir_failure_code code)
{
    switch (code) {
    case YVEX_DEEPSEEK_V4_IR_FAILURE_NONE: return "none";
    case YVEX_DEEPSEEK_V4_IR_FAILURE_INVALID_ARGUMENT: return "invalid-argument";
    case YVEX_DEEPSEEK_V4_IR_FAILURE_SOURCE_NOT_VERIFIED: return "source-not-verified";
    case YVEX_DEEPSEEK_V4_IR_FAILURE_IDENTITY_MISMATCH: return "identity-mismatch";
    case YVEX_DEEPSEEK_V4_IR_FAILURE_SOURCE_FACT_MISSING: return "source-fact-missing";
    case YVEX_DEEPSEEK_V4_IR_FAILURE_SCHEDULE_LENGTH: return "schedule-length";
    case YVEX_DEEPSEEK_V4_IR_FAILURE_SCHEDULE_PATTERN: return "schedule-pattern";
    case YVEX_DEEPSEEK_V4_IR_FAILURE_UNSUPPORTED_COMPRESSION: return "unsupported-compression";
    case YVEX_DEEPSEEK_V4_IR_FAILURE_INVALID_DIMENSION: return "invalid-dimension";
    case YVEX_DEEPSEEK_V4_IR_FAILURE_INVALID_GROUP_GEOMETRY: return "invalid-group-geometry";
    case YVEX_DEEPSEEK_V4_IR_FAILURE_INVALID_POSITION: return "invalid-position";
    case YVEX_DEEPSEEK_V4_IR_FAILURE_INVALID_MHC: return "invalid-mhc";
    case YVEX_DEEPSEEK_V4_IR_FAILURE_INVALID_ROUTING: return "invalid-routing";
    case YVEX_DEEPSEEK_V4_IR_FAILURE_INVALID_EXPERT_TOPK: return "invalid-expert-topk";
    case YVEX_DEEPSEEK_V4_IR_FAILURE_TOKENIZER_OUTPUT_MISMATCH: return "tokenizer-output-mismatch";
    case YVEX_DEEPSEEK_V4_IR_FAILURE_UNSUPPORTED_SOURCE_CONSTRAINT: return "unsupported-source-constraint";
    case YVEX_DEEPSEEK_V4_IR_FAILURE_NUMERIC_VALUE: return "invalid-numeric-value";
    case YVEX_DEEPSEEK_V4_IR_FAILURE_ARITHMETIC_OVERFLOW: return "arithmetic-overflow";
    case YVEX_DEEPSEEK_V4_IR_FAILURE_ALLOCATION: return "allocation-failure";
    default: return "unknown";
    }
}

const char *yvex_deepseek_v4_ir_component_name(
    yvex_deepseek_v4_ir_component component)
{
    switch (component) {
    case YVEX_DEEPSEEK_V4_IR_COMPONENT_NONE: return "none";
    case YVEX_DEEPSEEK_V4_IR_COMPONENT_SOURCE: return "source";
    case YVEX_DEEPSEEK_V4_IR_COMPONENT_IDENTITY: return "identity";
    case YVEX_DEEPSEEK_V4_IR_COMPONENT_MODEL: return "model";
    case YVEX_DEEPSEEK_V4_IR_COMPONENT_ATTENTION: return "attention";
    case YVEX_DEEPSEEK_V4_IR_COMPONENT_POSITION: return "position";
    case YVEX_DEEPSEEK_V4_IR_COMPONENT_MHC: return "mhc";
    case YVEX_DEEPSEEK_V4_IR_COMPONENT_MOE: return "moe";
    case YVEX_DEEPSEEK_V4_IR_COMPONENT_OUTPUT: return "output";
    case YVEX_DEEPSEEK_V4_IR_COMPONENT_TOKENIZER: return "tokenizer";
    case YVEX_DEEPSEEK_V4_IR_COMPONENT_AUXILIARY: return "auxiliary";
    case YVEX_DEEPSEEK_V4_IR_COMPONENT_SOURCE_CONSTRAINT: return "source-constraint";
    case YVEX_DEEPSEEK_V4_IR_COMPONENT_ALLOCATION: return "allocation";
    default: return "unknown";
    }
}

const char *yvex_deepseek_v4_attention_name(
    yvex_deepseek_v4_attention_class class_id)
{
    switch (class_id) {
    case YVEX_DEEPSEEK_V4_ATTENTION_SWA: return "swa";
    case YVEX_DEEPSEEK_V4_ATTENTION_CSA: return "csa";
    case YVEX_DEEPSEEK_V4_ATTENTION_HCA: return "hca";
    default: return "unknown";
    }
}

const char *yvex_deepseek_v4_kv_name(yvex_deepseek_v4_kv_class class_id)
{
    switch (class_id) {
    case YVEX_DEEPSEEK_V4_KV_SWA: return "swa-state";
    case YVEX_DEEPSEEK_V4_KV_CSA: return "csa-state-core-indexer";
    case YVEX_DEEPSEEK_V4_KV_HCA: return "hca-state-core";
    default: return "unknown";
    }
}

const char *yvex_deepseek_v4_router_name(
    yvex_deepseek_v4_router_class class_id)
{
    switch (class_id) {
    case YVEX_DEEPSEEK_V4_ROUTER_HASH_TOKEN_ID: return "hash-token-id";
    case YVEX_DEEPSEEK_V4_ROUTER_LEARNED_HIDDEN_STATE:
        return "learned-hidden-noaux-tc";
    default: return "unknown";
    }
}

const char *yvex_deepseek_v4_source_weight_dtype_name(
    yvex_deepseek_v4_source_weight_dtype dtype)
{
    return dtype == YVEX_DEEPSEEK_V4_SOURCE_WEIGHT_BF16
               ? "bfloat16"
               : "unknown";
}

const char *yvex_deepseek_v4_source_expert_dtype_name(
    yvex_deepseek_v4_source_expert_dtype dtype)
{
    return dtype == YVEX_DEEPSEEK_V4_SOURCE_EXPERT_FP4 ? "fp4" : "unknown";
}

const char *yvex_deepseek_v4_source_quantization_name(
    yvex_deepseek_v4_source_quantization quantization)
{
    return quantization ==
                   YVEX_DEEPSEEK_V4_SOURCE_QUANT_FP8_E4M3_UE8M0_DYNAMIC
               ? "fp8-e4m3-ue8m0-dynamic"
               : "unknown";
}
