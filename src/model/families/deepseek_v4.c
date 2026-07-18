/*
 * deepseek_v4.c - canonical DeepSeek-V4 model-family recipe.
 *
 * Owner:
 *   src/model/families
 *
 * Owns:
 *   immutable architecture facts, exact source coverage, artifact-neutral
 *   transform construction, GGUF lowering, and trusted payload handoff for
 *   the admitted DeepSeek-V4-Flash identity.
 *
 * Does not own:
 *   source/config IO, payload reads, numeric conversion, qtype policy,
 *   artifact writing, materialization, graph execution, or generation.
 *
 * Invariants:
 *   every layer, source contribution, transform terminal, and lowering
 *   descriptor derives from one admitted identity chain; rejected builds
 *   publish no partial object and read zero payload bytes.
 *
 * Boundary:
 *   one complete family recipe is not numeric execution or runtime support.
 */
#include "src/model/families.h"

#include "src/source/verify.h"
#include "src/model/target/catalog.h"

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
#define DEEPSEEK_V4_RUNTIME_NUMERIC_SCHEMA_VERSION 1u
#define DEEPSEEK_V4_RUNTIME_FP8_ACT_BLOCK 64ull
#define DEEPSEEK_V4_RUNTIME_FP4_ACT_BLOCK 32ull
#define DEEPSEEK_V4_RUNTIME_TOPK_POLICY_VERSION 1u

static const char deepseek_v4_paper_revision[] = "arXiv:2606.19348v1";
static const char deepseek_v4_sglang_revision[] =
    "96a04cb13f9c3ed86028e090784a9eb059cf5318";
static const char deepseek_v4_vllm_revision[] =
    "8df14cfc8c8a09b4e57f082e59593a3abce4ffb3";
static const char deepseek_v4_hadamard_revision[] =
    "Dao-AILab/fast-hadamard-transform:v1.1.0.post2:"
    "e7706faf8d1c3b9f241e36860640ad1dac644ede";

/* Private lifecycle and diagnostic operations used before their definitions. */
static void family_ir_close(yvex_deepseek_v4_ir *ir);
static const char *family_ir_failure_name(
    yvex_deepseek_v4_ir_failure_code code);
static const char *family_ir_component_name(
    yvex_deepseek_v4_ir_component component);
static void coverage_close(yvex_deepseek_tensor_coverage *coverage);
static const char *coverage_collection_name(
    yvex_deepseek_tensor_collection collection);
static const char *coverage_failure_name(
    yvex_deepseek_tensor_coverage_failure_code code);
static void lowering_close(yvex_deepseek_gguf_map *map);
static const char *lowering_failure_name(
    yvex_deepseek_gguf_map_failure_code code);
static void payload_close(yvex_deepseek_payload_handoff *handoff);

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
                    family_ir_component_name(component),
                    family_ir_failure_name(code),
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

static void deepseek_v4_fill_runtime_activation_none(
    yvex_deepseek_v4_runtime_activation_policy *policy)
{
    memset(policy, 0, sizeof(*policy));
    policy->stage = YVEX_DEEPSEEK_V4_RUNTIME_ACTIVATION_NONE;
    policy->quantization = YVEX_DEEPSEEK_V4_RUNTIME_ACTIVATION_QUANT_NONE;
    policy->block_axis = YVEX_DEEPSEEK_V4_RUNTIME_AXIS_NONE;
    policy->scale_format = YVEX_DEEPSEEK_V4_RUNTIME_SCALE_NONE;
    policy->scale_dtype = YVEX_NATIVE_DTYPE_UNKNOWN;
    policy->pre_transform = YVEX_DEEPSEEK_V4_RUNTIME_TRANSFORM_NONE;
    policy->tail_policy = YVEX_DEEPSEEK_V4_RUNTIME_TAIL_NONE;
    policy->nonfinite_policy = YVEX_DEEPSEEK_V4_RUNTIME_NONFINITE_REFUSE;
}

static void deepseek_v4_fill_runtime_activation_fp8(
    yvex_deepseek_v4_runtime_activation_policy *policy,
    yvex_deepseek_v4_runtime_activation_stage stage)
{
    memset(policy, 0, sizeof(*policy));
    policy->required = 1;
    policy->stage = stage;
    policy->quantization =
        YVEX_DEEPSEEK_V4_RUNTIME_ACTIVATION_QUANT_FP8_E4M3_UE8M0_FAKE_DEQUANT;
    policy->block_axis = YVEX_DEEPSEEK_V4_RUNTIME_AXIS_FINAL_DIMENSION;
    policy->block_width = DEEPSEEK_V4_RUNTIME_FP8_ACT_BLOCK;
    policy->scale_format = YVEX_DEEPSEEK_V4_RUNTIME_SCALE_UE8M0;
    policy->scale_dtype = YVEX_NATIVE_DTYPE_F8_E8M0;
    policy->pre_transform = YVEX_DEEPSEEK_V4_RUNTIME_TRANSFORM_NONE;
    policy->tail_policy =
        YVEX_DEEPSEEK_V4_RUNTIME_TAIL_EXACT_OR_SHORT_FINAL_BLOCK;
    policy->nonfinite_policy = YVEX_DEEPSEEK_V4_RUNTIME_NONFINITE_REFUSE;
    policy->fake_quant_inplace = 1;
}

static void deepseek_v4_fill_runtime_activation_fp4_hadamard(
    yvex_deepseek_v4_runtime_activation_policy *policy,
    yvex_deepseek_v4_runtime_activation_stage stage)
{
    memset(policy, 0, sizeof(*policy));
    policy->required = 1;
    policy->stage = stage;
    policy->quantization =
        YVEX_DEEPSEEK_V4_RUNTIME_ACTIVATION_QUANT_FP4_E2M1_UE8M0_FAKE_DEQUANT;
    policy->block_axis = YVEX_DEEPSEEK_V4_RUNTIME_AXIS_FINAL_DIMENSION;
    policy->block_width = DEEPSEEK_V4_RUNTIME_FP4_ACT_BLOCK;
    policy->scale_format = YVEX_DEEPSEEK_V4_RUNTIME_SCALE_UE8M0;
    policy->scale_dtype = YVEX_NATIVE_DTYPE_F8_E8M0;
    policy->pre_transform =
        YVEX_DEEPSEEK_V4_RUNTIME_TRANSFORM_DAO_FHT_V1_1_0_POST2;
    policy->tail_policy =
        YVEX_DEEPSEEK_V4_RUNTIME_TAIL_EXACT_OR_SHORT_FINAL_BLOCK;
    policy->nonfinite_policy = YVEX_DEEPSEEK_V4_RUNTIME_NONFINITE_REFUSE;
    policy->fake_quant_inplace = 1;
    policy->zero_pad_hadamard_to_power_of_two = 1;
}

static void deepseek_v4_fill_sparse_topk(
    yvex_deepseek_v4_runtime_sparse_topk_policy *policy,
    unsigned long long k)
{
    memset(policy, 0, sizeof(*policy));
    policy->required = k != 0ull;
    policy->version = DEEPSEEK_V4_RUNTIME_TOPK_POLICY_VERSION;
    policy->policy =
        policy->required
            ? YVEX_DEEPSEEK_V4_RUNTIME_TOPK_YVEX_SCORE_DESC_ORDINAL_ASC_V1
            : YVEX_DEEPSEEK_V4_RUNTIME_TOPK_NONE;
    policy->k = k;
    policy->reject_nonfinite = 1;
    policy->score_descending = 1;
    policy->equal_score_ordinal_ascending = 1;
    policy->plus_zero_equals_minus_zero = 1;
    policy->duplicate_ordinal_refused = 1;
    policy->output_ranked_order = 1;
}

static int deepseek_v4_validate_runtime_activation_policy(
    const yvex_deepseek_v4_runtime_activation_policy *policy,
    yvex_deepseek_v4_ir_failure *failure,
    unsigned long long layer_index,
    const char *field,
    yvex_error *err)
{
    if (!policy) {
        return deepseek_v4_reject(
            failure,
            YVEX_DEEPSEEK_V4_IR_FAILURE_UNSUPPORTED_RUNTIME_NUMERIC,
            YVEX_DEEPSEEK_V4_IR_COMPONENT_RUNTIME_NUMERIC,
            field, layer_index, 1u, 0u, err);
    }
    if (!policy->required) {
        if (policy->quantization !=
                YVEX_DEEPSEEK_V4_RUNTIME_ACTIVATION_QUANT_NONE ||
            policy->block_width != 0ull ||
            policy->pre_transform !=
                YVEX_DEEPSEEK_V4_RUNTIME_TRANSFORM_NONE) {
            return deepseek_v4_reject(
                failure,
                YVEX_DEEPSEEK_V4_IR_FAILURE_UNSUPPORTED_RUNTIME_NUMERIC,
                YVEX_DEEPSEEK_V4_IR_COMPONENT_RUNTIME_NUMERIC,
                field, layer_index, 0u, 1u, err);
        }
        return YVEX_OK;
    }
    if (policy->block_axis !=
            YVEX_DEEPSEEK_V4_RUNTIME_AXIS_FINAL_DIMENSION ||
        policy->scale_format != YVEX_DEEPSEEK_V4_RUNTIME_SCALE_UE8M0 ||
        policy->scale_dtype != YVEX_NATIVE_DTYPE_F8_E8M0 ||
        policy->tail_policy !=
            YVEX_DEEPSEEK_V4_RUNTIME_TAIL_EXACT_OR_SHORT_FINAL_BLOCK ||
        policy->nonfinite_policy !=
            YVEX_DEEPSEEK_V4_RUNTIME_NONFINITE_REFUSE ||
        !policy->fake_quant_inplace) {
        return deepseek_v4_reject(
            failure,
            YVEX_DEEPSEEK_V4_IR_FAILURE_UNSUPPORTED_RUNTIME_NUMERIC,
            YVEX_DEEPSEEK_V4_IR_COMPONENT_RUNTIME_NUMERIC,
            field, layer_index, 1u, 0u, err);
    }
    if (policy->quantization ==
            YVEX_DEEPSEEK_V4_RUNTIME_ACTIVATION_QUANT_FP8_E4M3_UE8M0_FAKE_DEQUANT) {
        if (policy->block_width != DEEPSEEK_V4_RUNTIME_FP8_ACT_BLOCK ||
            policy->pre_transform !=
                YVEX_DEEPSEEK_V4_RUNTIME_TRANSFORM_NONE) {
            return deepseek_v4_reject(
                failure,
                YVEX_DEEPSEEK_V4_IR_FAILURE_UNSUPPORTED_RUNTIME_NUMERIC,
                YVEX_DEEPSEEK_V4_IR_COMPONENT_RUNTIME_NUMERIC,
                field, layer_index, DEEPSEEK_V4_RUNTIME_FP8_ACT_BLOCK,
                policy->block_width, err);
        }
    } else if (policy->quantization ==
               YVEX_DEEPSEEK_V4_RUNTIME_ACTIVATION_QUANT_FP4_E2M1_UE8M0_FAKE_DEQUANT) {
        if (policy->block_width != DEEPSEEK_V4_RUNTIME_FP4_ACT_BLOCK ||
            policy->pre_transform !=
                YVEX_DEEPSEEK_V4_RUNTIME_TRANSFORM_DAO_FHT_V1_1_0_POST2 ||
            !policy->zero_pad_hadamard_to_power_of_two) {
            return deepseek_v4_reject(
                failure,
                YVEX_DEEPSEEK_V4_IR_FAILURE_UNSUPPORTED_RUNTIME_NUMERIC,
                YVEX_DEEPSEEK_V4_IR_COMPONENT_RUNTIME_NUMERIC,
                field, layer_index, DEEPSEEK_V4_RUNTIME_FP4_ACT_BLOCK,
                policy->block_width, err);
        }
    } else {
        return deepseek_v4_reject(
            failure,
            YVEX_DEEPSEEK_V4_IR_FAILURE_UNSUPPORTED_RUNTIME_NUMERIC,
            YVEX_DEEPSEEK_V4_IR_COMPONENT_RUNTIME_NUMERIC,
            field, layer_index, 1u, 0u, err);
    }
    return YVEX_OK;
}

static int deepseek_v4_validate_runtime_numeric_layer(
    const yvex_deepseek_v4_layer_spec *layer,
    yvex_deepseek_v4_ir_failure *failure,
    yvex_error *err)
{
    int rc;

    rc = deepseek_v4_validate_runtime_activation_policy(
        &layer->attention_kv_activation, failure, layer->layer_index,
        "attention-kv-activation", err);
    if (rc != YVEX_OK) return rc;
    rc = deepseek_v4_validate_runtime_activation_policy(
        &layer->compressor_activation, failure, layer->layer_index,
        "compressor-activation", err);
    if (rc != YVEX_OK) return rc;
    rc = deepseek_v4_validate_runtime_activation_policy(
        &layer->compressor_rotated_activation, failure, layer->layer_index,
        "compressor-rotated-activation", err);
    if (rc != YVEX_OK) return rc;
    rc = deepseek_v4_validate_runtime_activation_policy(
        &layer->indexer_query_activation, failure, layer->layer_index,
        "indexer-query-activation", err);
    if (rc != YVEX_OK) return rc;
    if (layer->sparse_topk.required) {
        if (layer->sparse_topk.version !=
                DEEPSEEK_V4_RUNTIME_TOPK_POLICY_VERSION ||
            layer->sparse_topk.policy !=
                YVEX_DEEPSEEK_V4_RUNTIME_TOPK_YVEX_SCORE_DESC_ORDINAL_ASC_V1 ||
            layer->sparse_topk.k == 0ull ||
            !layer->sparse_topk.reject_nonfinite ||
            !layer->sparse_topk.score_descending ||
            !layer->sparse_topk.equal_score_ordinal_ascending ||
            !layer->sparse_topk.plus_zero_equals_minus_zero ||
            !layer->sparse_topk.duplicate_ordinal_refused ||
            !layer->sparse_topk.output_ranked_order) {
            return deepseek_v4_reject(
                failure,
                YVEX_DEEPSEEK_V4_IR_FAILURE_UNSUPPORTED_RUNTIME_NUMERIC,
                YVEX_DEEPSEEK_V4_IR_COMPONENT_RUNTIME_NUMERIC,
                "sparse-topk-policy", layer->layer_index, 1u, 0u, err);
        }
    }
    return YVEX_OK;
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
    deepseek_v4_fill_runtime_activation_fp8(
        &layer->attention_kv_activation,
        YVEX_DEEPSEEK_V4_RUNTIME_ACTIVATION_ATTENTION_KV_NON_ROPE);
    deepseek_v4_fill_runtime_activation_none(&layer->compressor_activation);
    deepseek_v4_fill_runtime_activation_none(
        &layer->compressor_rotated_activation);
    deepseek_v4_fill_runtime_activation_none(&layer->indexer_query_activation);
    deepseek_v4_fill_sparse_topk(&layer->sparse_topk, 0ull);
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
        deepseek_v4_fill_runtime_activation_fp8(
            &layer->compressor_activation,
            YVEX_DEEPSEEK_V4_RUNTIME_ACTIVATION_COMPRESSOR_NON_ROTATED);
        deepseek_v4_fill_runtime_activation_fp4_hadamard(
            &layer->compressor_rotated_activation,
            YVEX_DEEPSEEK_V4_RUNTIME_ACTIVATION_COMPRESSOR_ROTATED);
        deepseek_v4_fill_runtime_activation_fp4_hadamard(
            &layer->indexer_query_activation,
            YVEX_DEEPSEEK_V4_RUNTIME_ACTIVATION_INDEXER_QUERY_ROTATED);
        deepseek_v4_fill_sparse_topk(&layer->sparse_topk,
                                     source->index_topk);
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
        deepseek_v4_fill_runtime_activation_fp8(
            &layer->compressor_activation,
            YVEX_DEEPSEEK_V4_RUNTIME_ACTIVATION_COMPRESSOR_NON_ROTATED);
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
    deepseek_v4_copy(model->hadamard_revision,
                     sizeof(model->hadamard_revision),
                     deepseek_v4_hadamard_revision);
    model->runtime_numeric_schema_version =
        DEEPSEEK_V4_RUNTIME_NUMERIC_SCHEMA_VERSION;
    model->runtime_activation_policy_count = 3u;
    model->runtime_sparse_topk_policy_count = 1u;
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
        family_ir_close(ir);
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
        family_ir_close(ir);
        return deepseek_v4_reject(
            failure, YVEX_DEEPSEEK_V4_IR_FAILURE_ALLOCATION,
            YVEX_DEEPSEEK_V4_IR_COMPONENT_ALLOCATION, "layers",
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, source->num_hidden_layers, 0u, err);
    }
    memset(ir->layers, 0,
           (size_t)source->num_hidden_layers * sizeof(*ir->layers));
    if (source->num_nextn_predict_layers >
        (unsigned long long)(SIZE_MAX / sizeof(*ir->auxiliary))) {
        family_ir_close(ir);
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
        family_ir_close(ir);
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
        if (deepseek_v4_validate_runtime_numeric_layer(
                &ir->layers[i], failure, err) != YVEX_OK) {
            family_ir_close(ir);
            return yvex_error_code(err);
        }
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
        if (deepseek_v4_validate_runtime_numeric_layer(
                &aux->layer, failure, err) != YVEX_OK) {
            family_ir_close(ir);
            return yvex_error_code(err);
        }
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
static int family_ir_build_with_allocator(
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
static int family_ir_build(
    yvex_deepseek_v4_ir **out,
    const struct yvex_source_verification *verification,
    yvex_deepseek_v4_ir_failure *failure,
    yvex_error *err)
{
    yvex_deepseek_v4_ir_allocator allocator =
        deepseek_v4_default_allocator();

    return family_ir_build_with_allocator(
        out, verification, &allocator, failure, err);
}

/* Releases a fully or partially owned IR; NULL is a safe no-op. */
static void family_ir_close(yvex_deepseek_v4_ir *ir)
{
    yvex_deepseek_v4_ir_allocator allocator;

    if (!ir) return;
    allocator = ir->allocator;
    if (ir->auxiliary) allocator.release(ir->auxiliary, allocator.context);
    if (ir->layers) allocator.release(ir->layers, allocator.context);
    memset(ir, 0, sizeof(*ir));
    allocator.release(ir, allocator.context);
}

static const yvex_deepseek_v4_model_spec *family_ir_model(
    const yvex_deepseek_v4_ir *ir)
{
    return ir ? &ir->model : NULL;
}

static unsigned long long family_ir_layer_count(
    const yvex_deepseek_v4_ir *ir)
{
    return ir ? ir->model.main_layer_count : 0u;
}

static const yvex_deepseek_v4_layer_spec *family_ir_layer_at(
    const yvex_deepseek_v4_ir *ir,
    unsigned long long index)
{
    return ir && index < ir->model.main_layer_count ? &ir->layers[index]
                                                     : NULL;
}

static unsigned long long family_ir_auxiliary_count(
    const yvex_deepseek_v4_ir *ir)
{
    return ir ? ir->model.auxiliary_layer_count : 0u;
}

static const yvex_deepseek_v4_auxiliary_spec *family_ir_auxiliary_at(
    const yvex_deepseek_v4_ir *ir,
    unsigned long long index)
{
    return ir && index < ir->model.auxiliary_layer_count
               ? &ir->auxiliary[index]
               : NULL;
}

static const char *family_ir_failure_name(
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
    case YVEX_DEEPSEEK_V4_IR_FAILURE_UNSUPPORTED_RUNTIME_NUMERIC: return "unsupported-runtime-numeric";
    case YVEX_DEEPSEEK_V4_IR_FAILURE_NUMERIC_VALUE: return "invalid-numeric-value";
    case YVEX_DEEPSEEK_V4_IR_FAILURE_ARITHMETIC_OVERFLOW: return "arithmetic-overflow";
    case YVEX_DEEPSEEK_V4_IR_FAILURE_ALLOCATION: return "allocation-failure";
    default: return "unknown";
    }
}

static const char *family_ir_component_name(
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
    case YVEX_DEEPSEEK_V4_IR_COMPONENT_RUNTIME_NUMERIC: return "runtime-numeric";
    case YVEX_DEEPSEEK_V4_IR_COMPONENT_ALLOCATION: return "allocation";
    default: return "unknown";
    }
}

static const char *family_attention_name(
    yvex_deepseek_v4_attention_class class_id)
{
    switch (class_id) {
    case YVEX_DEEPSEEK_V4_ATTENTION_SWA: return "swa";
    case YVEX_DEEPSEEK_V4_ATTENTION_CSA: return "csa";
    case YVEX_DEEPSEEK_V4_ATTENTION_HCA: return "hca";
    default: return "unknown";
    }
}

static const char *family_kv_name(yvex_deepseek_v4_kv_class class_id)
{
    switch (class_id) {
    case YVEX_DEEPSEEK_V4_KV_SWA: return "swa-state";
    case YVEX_DEEPSEEK_V4_KV_CSA: return "csa-state-core-indexer";
    case YVEX_DEEPSEEK_V4_KV_HCA: return "hca-state-core";
    default: return "unknown";
    }
}

static const char *family_router_name(
    yvex_deepseek_v4_router_class class_id)
{
    switch (class_id) {
    case YVEX_DEEPSEEK_V4_ROUTER_HASH_TOKEN_ID: return "hash-token-id";
    case YVEX_DEEPSEEK_V4_ROUTER_LEARNED_HIDDEN_STATE:
        return "learned-hidden-noaux-tc";
    default: return "unknown";
    }
}

static const char *family_source_weight_dtype_name(
    yvex_deepseek_v4_source_weight_dtype dtype)
{
    return dtype == YVEX_DEEPSEEK_V4_SOURCE_WEIGHT_BF16
               ? "bfloat16"
               : "unknown";
}

static const char *family_source_expert_dtype_name(
    yvex_deepseek_v4_source_expert_dtype dtype)
{
    return dtype == YVEX_DEEPSEEK_V4_SOURCE_EXPERT_FP4 ? "fp4" : "unknown";
}

static const char *family_source_quantization_name(
    yvex_deepseek_v4_source_quantization quantization)
{
    return quantization ==
                   YVEX_DEEPSEEK_V4_SOURCE_QUANT_FP8_E4M3_UE8M0_DYNAMIC
               ? "fp8-e4m3-ue8m0-dynamic"
               : "unknown";
}

static const char *family_activation_stage_name(
    yvex_deepseek_v4_runtime_activation_stage stage)
{
    switch (stage) {
    case YVEX_DEEPSEEK_V4_RUNTIME_ACTIVATION_NONE: return "none";
    case YVEX_DEEPSEEK_V4_RUNTIME_ACTIVATION_ATTENTION_KV_NON_ROPE:
        return "attention-kv-non-rope";
    case YVEX_DEEPSEEK_V4_RUNTIME_ACTIVATION_COMPRESSOR_NON_ROTATED:
        return "compressor-non-rotated";
    case YVEX_DEEPSEEK_V4_RUNTIME_ACTIVATION_COMPRESSOR_ROTATED:
        return "compressor-rotated";
    case YVEX_DEEPSEEK_V4_RUNTIME_ACTIVATION_INDEXER_QUERY_ROTATED:
        return "indexer-query-rotated";
    }
    return "unknown";
}

static const char *family_activation_quantization_name(
    yvex_deepseek_v4_runtime_activation_quantization quantization)
{
    switch (quantization) {
    case YVEX_DEEPSEEK_V4_RUNTIME_ACTIVATION_QUANT_NONE:
        return "none";
    case YVEX_DEEPSEEK_V4_RUNTIME_ACTIVATION_QUANT_FP8_E4M3_UE8M0_FAKE_DEQUANT:
        return "fp8-e4m3-ue8m0-fake-dequant";
    case YVEX_DEEPSEEK_V4_RUNTIME_ACTIVATION_QUANT_FP4_E2M1_UE8M0_FAKE_DEQUANT:
        return "fp4-e2m1-ue8m0-fake-dequant";
    }
    return "unknown";
}

static const char *family_runtime_transform_name(
    yvex_deepseek_v4_runtime_transform transform)
{
    switch (transform) {
    case YVEX_DEEPSEEK_V4_RUNTIME_TRANSFORM_NONE: return "none";
    case YVEX_DEEPSEEK_V4_RUNTIME_TRANSFORM_DAO_FHT_V1_1_0_POST2:
        return "dao-fast-hadamard-transform-v1.1.0.post2";
    }
    return "unknown";
}

static const char *family_sparse_topk_policy_name(
    yvex_deepseek_v4_runtime_sparse_topk_policy_id policy)
{
    switch (policy) {
    case YVEX_DEEPSEEK_V4_RUNTIME_TOPK_NONE: return "none";
    case YVEX_DEEPSEEK_V4_RUNTIME_TOPK_YVEX_SCORE_DESC_ORDINAL_ASC_V1:
        return "yvex-score-desc-ordinal-asc-v1";
    }
    return "unknown";
}

/* Exact coverage binds every typed requirement to one retained source row. */

#include "catalog.h"


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
                    coverage_failure_name(code),
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
    const yvex_deepseek_v4_model_spec *model = family_ir_model(ir);
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
            family_ir_layer_at(ir, layer_index);
        (void)snprintf(prefix, sizeof(prefix), "layers.%llu", layer_index);
        rc = coverage_layer(builder, prefix, layer,
                            YVEX_DEEPSEEK_TENSOR_SCOPE_MAIN_LAYER, model);
        if (rc != YVEX_OK) return rc;
    }
    for (layer_index = 0u; layer_index < model->auxiliary_layer_count;
         ++layer_index) {
        const yvex_deepseek_v4_auxiliary_spec *aux =
            family_ir_auxiliary_at(ir, layer_index);
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
        family_ir_model(ir);
    const yvex_deepseek_v4_layer_spec *layer_spec = NULL;
    const yvex_deepseek_v4_auxiliary_spec *aux =
        family_ir_auxiliary_at(ir, 0u);
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
            layer_spec = family_ir_layer_at(ir, layer);
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
    const yvex_deepseek_v4_model_spec *model = family_ir_model(ir);

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
static int coverage_build(
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
        coverage_close(coverage);
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
        coverage_close(coverage);
        return rc;
    }
    if (coverage->summary.required_tensor_count != source_facts.tensor_count ||
        coverage->summary.matched_tensor_count != source_facts.tensor_count) {
        unsigned long long matched = coverage->summary.matched_tensor_count;
        coverage_close(coverage);
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
        const char *collection = coverage_collection_name(
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
        coverage_close(coverage);
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
static int coverage_open_verified_source(
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
    rc = family_ir_build(&ir, verification, &ir_failure, err);
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
    rc = coverage_build(
        out, verification, ir, snapshot, NULL, failure, err);
cleanup:
    family_ir_close(ir);
    yvex_source_tensor_snapshot_release(snapshot);
    return rc;
}

/* Releases owned rows and the retained snapshot; null input is a no-op. */
static void coverage_close(
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

static const yvex_deepseek_tensor_coverage_summary *
coverage_summary(
    const yvex_deepseek_tensor_coverage *coverage)
{
    return coverage ? &coverage->summary : NULL;
}

static const yvex_deepseek_tensor_coverage_row *
coverage_at(
    const yvex_deepseek_tensor_coverage *coverage,
    unsigned long long index)
{
    if (!coverage || index >= coverage->summary.required_tensor_count)
        return NULL;
    return &coverage->rows[index];
}

static const yvex_deepseek_tensor_coverage_row *
coverage_find(
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
static int coverage_find_index(
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

/* Resolves one covered name to its retained snapshot tensor index. */
static int coverage_find_source_index(
    const yvex_deepseek_tensor_coverage *coverage,
    const char *source_name,
    unsigned long long *source_index)
{
    if (!coverage || !source_name || !source_index ||
        !yvex_source_tensor_snapshot_find_index(coverage->snapshot,
                                                source_name, source_index))
        return 0;
    return coverage->row_by_source[*source_index] != NULL;
}

static const char *coverage_collection_name(
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

static const char *coverage_failure_name(
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

/* The family transform recipe registers semantics in the generic sealed IR. */
#include "src/model/compilation/private.h"

#include "src/core/sha256.h"


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
static int transform_architecture_identity(
    const yvex_deepseek_v4_ir *architecture,
    char output[YVEX_TRANSFORM_IR_IDENTITY_CAP])
{
    static const char domain[] = "yvex.logical-model.deepseek-v4-flash.v1";
    const yvex_deepseek_v4_model_spec *model =
        family_ir_model(architecture);
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
#define ID_RUNTIME_ACTIVATION(policy) do {                                   \
        ID_U64((policy).required);                                           \
        ID_U64((policy).stage);                                              \
        ID_U64((policy).quantization);                                       \
        ID_U64((policy).block_axis);                                         \
        ID_U64((policy).block_width);                                        \
        ID_U64((policy).scale_format);                                       \
        ID_U64((policy).scale_dtype);                                        \
        ID_U64((policy).pre_transform);                                      \
        ID_U64((policy).tail_policy);                                        \
        ID_U64((policy).nonfinite_policy);                                   \
        ID_U64((policy).fake_quant_inplace);                                 \
        ID_U64((policy).zero_pad_hadamard_to_power_of_two);                  \
    } while (0)
#define ID_RUNTIME_TOPK(topk_obj) do {                                       \
        ID_U64((topk_obj).required);                                         \
        ID_U64((topk_obj).version);                                          \
        ID_U64((topk_obj).policy);                                           \
        ID_U64((topk_obj).k);                                                \
        ID_U64((topk_obj).reject_nonfinite);                                 \
        ID_U64((topk_obj).score_descending);                                 \
        ID_U64((topk_obj).equal_score_ordinal_ascending);                    \
        ID_U64((topk_obj).plus_zero_equals_minus_zero);                      \
        ID_U64((topk_obj).duplicate_ordinal_refused);                        \
        ID_U64((topk_obj).output_ranked_order);                              \
    } while (0)
    ID_TEXT(domain);
    ID_TEXT(model->target_id);
    ID_TEXT(model->family);
    ID_TEXT(model->architecture);
    ID_TEXT(model->repository);
    ID_TEXT(model->revision);
    ID_TEXT(model->hadamard_revision);
    ID_U64(model->runtime_numeric_schema_version);
    ID_U64(model->runtime_activation_policy_count);
    ID_U64(model->runtime_sparse_topk_policy_count);
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
            family_ir_layer_at(architecture, index);
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
        ID_RUNTIME_ACTIVATION(layer->attention_kv_activation);
        ID_RUNTIME_ACTIVATION(layer->compressor_activation);
        ID_RUNTIME_ACTIVATION(layer->compressor_rotated_activation);
        ID_RUNTIME_ACTIVATION(layer->indexer_query_activation);
        ID_RUNTIME_TOPK(layer->sparse_topk);
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
            family_ir_auxiliary_at(architecture, index);
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
        ID_RUNTIME_ACTIVATION(aux->layer.attention_kv_activation);
        ID_RUNTIME_ACTIVATION(aux->layer.compressor_activation);
        ID_RUNTIME_ACTIVATION(aux->layer.compressor_rotated_activation);
        ID_RUNTIME_ACTIVATION(aux->layer.indexer_query_activation);
        ID_RUNTIME_TOPK(aux->layer.sparse_topk);
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
#undef ID_RUNTIME_TOPK
#undef ID_RUNTIME_ACTIVATION
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

    row = coverage_find(builder->coverage, name);
    if (!row || !row->source ||
        !coverage_find_index(
            builder->coverage, name, &requirement_index) ||
        !coverage_find_source_index(
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
        coverage_find(builder->coverage, source_name);
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
    row = coverage_find(builder->coverage, weight);
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
    first = coverage_find(builder->coverage, weight);
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
            family_ir_layer_at(builder->architecture, layer),
            YVEX_DEEPSEEK_TENSOR_SCOPE_MAIN_LAYER,
            YVEX_DEEPSEEK_TENSOR_NO_INDEX);
        if (rc != YVEX_OK) return rc;
    }
    for (layer = 0u; layer < builder->model->auxiliary_layer_count; ++layer) {
        const yvex_deepseek_v4_auxiliary_spec *aux =
            family_ir_auxiliary_at(builder->architecture, layer);
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
        family_ir_model(architecture);
    const yvex_deepseek_tensor_coverage_summary *summary =
        coverage_summary(coverage);

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

static int transform_ir_build(
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
    if (!transform_architecture_identity(
            architecture, logical_identity))
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

/* GGUF lowering projects the sealed IR without becoming semantic identity. */


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
                    lowering_failure_name(code),
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
        family_ir_model(builder->architecture);
    const yvex_deepseek_v4_layer_spec *first =
        family_ir_layer_at(builder->architecture, 0u);
    const yvex_deepseek_v4_layer_spec *first_csa =
        family_ir_layer_at(builder->architecture, 2u);
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
        ratios[index] = family_ir_layer_at(
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
        clamp[index] = family_ir_layer_at(
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

static int lowering_build_with_allocator(
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
    model = family_ir_model(architecture);
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
        lowering_close(map);
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
        lowering_close(map);
        return rc;
    }
    *out = map;
    yvex_error_clear(err);
    return YVEX_OK;
}

static int lowering_build(
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
    return lowering_build_with_allocator(
        out, architecture, transform_ir, &allocator, failure, err);
}

static void lowering_close(yvex_deepseek_gguf_map *map)
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

static const yvex_deepseek_gguf_map_summary *lowering_summary(
    const yvex_deepseek_gguf_map *map)
{
    return map ? &map->summary : NULL;
}

static const yvex_deepseek_gguf_descriptor *lowering_at(
    const yvex_deepseek_gguf_map *map,
    unsigned long long index)
{
    return map && index < map->summary.descriptor_count
        ? &map->descriptors[index] : NULL;
}

static const yvex_deepseek_gguf_contribution *
lowering_contribution_at(
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

static const yvex_deepseek_gguf_descriptor *lowering_find_source(
    const yvex_deepseek_gguf_map *map,
    const char *source_name)
{
    return map_find_name(map, source_name, 0);
}

static const yvex_deepseek_gguf_descriptor *lowering_find_emitted(
    const yvex_deepseek_gguf_map *map,
    const char *emitted_name)
{
    return map_find_name(map, emitted_name, 1);
}

static const yvex_deepseek_gguf_descriptor *lowering_find_role(
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

static const yvex_deepseek_gguf_metadata *lowering_metadata_at(
    const yvex_deepseek_gguf_map *map,
    unsigned long long index)
{
    return map && index < map->summary.metadata_count
        ? &map->metadata[index] : NULL;
}

static const yvex_deepseek_gguf_metadata *lowering_metadata_find(
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

static const char *lowering_transform_name(
    yvex_deepseek_gguf_transform transform)
{
    static const char *names[] = {
        "direct", "fp8-e4m3-e8m0-pair", "expert-mxfp4-repack",
        "i64-to-i32"
    };
    return transform <= YVEX_DEEPSEEK_GGUF_TRANSFORM_I64_TO_I32
        ? names[transform] : "unknown";
}

static const char *lowering_failure_name(
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

/* Payload handoff resolves typed family inputs through the common source ABI. */


struct yvex_deepseek_payload_handoff {
    char *source_path;
    char *models_root;
    char *manifest_path;
    yvex_source_verify_options source_options;
    yvex_source_verification verification;
    yvex_deepseek_tensor_coverage *coverage;
    yvex_transform_ir *transform_ir;
    yvex_deepseek_gguf_map *map;
    yvex_source_payload_session *session;
    yvex_transform_binding *binding;
    yvex_source_payload_plan *plan;
    yvex_deepseek_payload_handoff_summary summary;
};

static char *handoff_strdup(const char *text)
{
    size_t length;
    char *copy;

    if (!text) return NULL;
    length = strlen(text);
    copy = (char *)malloc(length + 1u);
    if (copy) memcpy(copy, text, length + 1u);
    return copy;
}

static int handoff_reject(yvex_deepseek_payload_failure *failure,
                          yvex_deepseek_payload_failure_code code,
                          unsigned long long descriptor,
                          unsigned long long contribution,
                          int status,
                          yvex_error *err,
                          const char *message)
{
    if (failure) {
        memset(failure, 0, sizeof(*failure));
        failure->code = code;
        failure->descriptor_index = descriptor;
        failure->contribution_index = contribution;
    }
    yvex_error_set(err, (yvex_status)status, "deepseek_payload_handoff", message);
    return status;
}

/* Resolves every canonical contribution and builds one physical-order source plan. */
static int handoff_resolve(yvex_deepseek_payload_handoff *handoff,
                           const yvex_deepseek_payload_handoff_options *options,
                           yvex_deepseek_payload_failure *failure,
                           yvex_error *err)
{
    const yvex_deepseek_gguf_map_summary *map_summary =
        lowering_summary(handoff->map);
    unsigned long long *tensor_indices;
    unsigned long long contribution_index;
    unsigned long long descriptor_index;
    int rc;

    if (!map_summary || !map_summary->complete ||
        map_summary->mapping_identity !=
            YVEX_DEEPSEEK_PAYLOAD_MAPPING_IDENTITY ||
        map_summary->source_identity !=
            handoff->verification.source_snapshot_identity) {
        return handoff_reject(
            failure, YVEX_DEEPSEEK_PAYLOAD_FAILURE_MAPPING_IDENTITY,
            ULLONG_MAX, ULLONG_MAX, YVEX_ERR_FORMAT, err,
            "canonical DeepSeek mapping identity mismatch");
    }
    if (map_summary->source_contribution_count >
        (unsigned long long)(SIZE_MAX / sizeof(tensor_indices[0]))) {
        return handoff_reject(
            failure, YVEX_DEEPSEEK_PAYLOAD_FAILURE_ALLOCATION,
            ULLONG_MAX, ULLONG_MAX, YVEX_ERR_BOUNDS, err,
            "mapping contribution index allocation overflow");
    }
    tensor_indices = (unsigned long long *)calloc(
        (size_t)map_summary->source_contribution_count,
        sizeof(tensor_indices[0]));
    if (!tensor_indices) {
        return handoff_reject(
            failure, YVEX_DEEPSEEK_PAYLOAD_FAILURE_ALLOCATION,
            ULLONG_MAX, ULLONG_MAX, YVEX_ERR_NOMEM, err,
            "mapping contribution index allocation failed");
    }
    handoff->summary.mapping_identity = map_summary->mapping_identity;
    (void)snprintf(handoff->summary.transform_identity,
                   sizeof(handoff->summary.transform_identity), "%s",
                   yvex_transform_ir_summary_get(
                       handoff->transform_ir)->transform_identity);
    handoff->summary.source_snapshot_identity = map_summary->source_identity;
    handoff->summary.descriptor_count = map_summary->descriptor_count;
    handoff->summary.contribution_count = map_summary->source_contribution_count;
    for (contribution_index = 0u;
         contribution_index < map_summary->source_contribution_count;
         ++contribution_index) {
        const yvex_deepseek_gguf_contribution *contribution =
            lowering_contribution_at(
                handoff->map, contribution_index);
        const yvex_source_payload_range *range;
        const yvex_deepseek_tensor_coverage_row *coverage_row;
        const yvex_deepseek_gguf_descriptor *descriptor;

        if (!contribution ||
            contribution->descriptor_index >= map_summary->descriptor_count) {
            free(tensor_indices);
            return handoff_reject(
                failure, YVEX_DEEPSEEK_PAYLOAD_FAILURE_CONTRIBUTION,
                ULLONG_MAX, contribution_index, YVEX_ERR_FORMAT, err,
                "mapping contribution is incomplete");
        }
        descriptor = lowering_at(
            handoff->map, contribution->descriptor_index);
        coverage_row = coverage_at(
            handoff->coverage, contribution->source_row_index);
        range = yvex_source_payload_range_find(
            handoff->session, contribution->source_name);
        handoff->summary.range_lookup_count++;
        if (!descriptor || !coverage_row || !coverage_row->source || !range ||
            strcmp(coverage_row->source->name, contribution->source_name) != 0 ||
            range->source_snapshot_identity != map_summary->source_identity ||
            range->dtype != contribution->source_dtype ||
            range->rank != contribution->source_rank) {
            free(tensor_indices);
            return handoff_reject(
                failure, YVEX_DEEPSEEK_PAYLOAD_FAILURE_RANGE,
                contribution->descriptor_index, contribution_index,
                YVEX_ERR_FORMAT, err,
                "mapping contribution does not resolve to its exact source range");
        }
        tensor_indices[contribution_index] = range->source_tensor_index;
        handoff->summary.contributions_resolved++;
        if (descriptor->transform == YVEX_DEEPSEEK_GGUF_TRANSFORM_DIRECT)
            handoff->summary.direct_contributions++;
        if (contribution->kind == YVEX_DEEPSEEK_GGUF_CONTRIBUTION_PRIMARY &&
            contribution->source_dtype == YVEX_NATIVE_DTYPE_F8_E4M3)
            handoff->summary.fp8_weight_contributions++;
        if (contribution->kind == YVEX_DEEPSEEK_GGUF_CONTRIBUTION_SCALE &&
            contribution->source_dtype == YVEX_NATIVE_DTYPE_F8_E8M0)
            handoff->summary.e8m0_scale_contributions++;
        if (contribution->kind ==
                YVEX_DEEPSEEK_GGUF_CONTRIBUTION_EXPERT_WEIGHT ||
            contribution->kind ==
                YVEX_DEEPSEEK_GGUF_CONTRIBUTION_EXPERT_SCALE) {
            if (ULLONG_MAX - handoff->summary.routed_expert_logical_bytes <
                range->byte_length) {
                free(tensor_indices);
                return handoff_reject(
                    failure, YVEX_DEEPSEEK_PAYLOAD_FAILURE_RANGE,
                    contribution->descriptor_index, contribution_index,
                    YVEX_ERR_BOUNDS, err,
                    "routed expert payload accounting overflow");
            }
            handoff->summary.expert_contributions++;
            handoff->summary.routed_expert_logical_bytes += range->byte_length;
        }
        if (descriptor->transform == YVEX_DEEPSEEK_GGUF_TRANSFORM_I64_TO_I32 &&
            contribution->source_dtype == YVEX_NATIVE_DTYPE_I64)
            handoff->summary.i64_router_contributions++;
        if (descriptor->collection == YVEX_DEEPSEEK_TENSOR_COLLECTION_GLOBAL)
            handoff->summary.global_contributions++;
        if (descriptor->collection == YVEX_DEEPSEEK_TENSOR_COLLECTION_NORM)
            handoff->summary.norm_contributions++;
        if (descriptor->collection ==
            YVEX_DEEPSEEK_TENSOR_COLLECTION_SHARED_EXPERT)
            handoff->summary.shared_expert_contributions++;
        if (descriptor->role == YVEX_TENSOR_ROLE_OUTPUT_HEAD) {
            if (ULLONG_MAX - handoff->summary.output_head_logical_bytes <
                range->byte_length) {
                free(tensor_indices);
                return handoff_reject(
                    failure, YVEX_DEEPSEEK_PAYLOAD_FAILURE_RANGE,
                    contribution->descriptor_index, contribution_index,
                    YVEX_ERR_BOUNDS, err,
                    "output head payload accounting overflow");
            }
            handoff->summary.output_head_contributions++;
            handoff->summary.output_head_logical_bytes += range->byte_length;
        }
        if (descriptor->scope == YVEX_DEEPSEEK_TENSOR_SCOPE_MTP)
            handoff->summary.mtp_contributions++;
    }
    for (descriptor_index = 0u;
         descriptor_index < map_summary->descriptor_count; ++descriptor_index) {
        const yvex_deepseek_gguf_descriptor *descriptor =
            lowering_at(handoff->map, descriptor_index);
        unsigned long long end;

        if (!descriptor || descriptor->contribution_count == 0u ||
            ULLONG_MAX - descriptor->contribution_offset <
                descriptor->contribution_count) {
            free(tensor_indices);
            return handoff_reject(
                failure, YVEX_DEEPSEEK_PAYLOAD_FAILURE_CONTRIBUTION,
                descriptor_index, ULLONG_MAX, YVEX_ERR_FORMAT, err,
                "logical descriptor has no bounded source contribution set");
        }
        end = descriptor->contribution_offset + descriptor->contribution_count;
        if (end > handoff->summary.contributions_resolved) {
            free(tensor_indices);
            return handoff_reject(
                failure, YVEX_DEEPSEEK_PAYLOAD_FAILURE_CONTRIBUTION,
                descriptor_index, end, YVEX_ERR_FORMAT, err,
                "logical descriptor contribution span exceeds resolved mapping");
        }
        handoff->summary.descriptors_covered++;
    }
    rc = yvex_source_payload_plan_build(
        &handoff->plan, handoff->session, tensor_indices,
        map_summary->source_contribution_count, options->chunk_bytes,
        options->page_bytes, failure ? &failure->payload_failure : NULL, err);
    free(tensor_indices);
    if (rc != YVEX_OK) {
        if (failure) failure->code = YVEX_DEEPSEEK_PAYLOAD_FAILURE_PLAN;
        return rc;
    }
    handoff->summary.complete =
        handoff->summary.descriptors_covered ==
            YVEX_DEEPSEEK_GGUF_DESCRIPTOR_COUNT &&
        handoff->summary.contributions_resolved ==
            YVEX_DEEPSEEK_GGUF_SOURCE_COUNT &&
        handoff->summary.fp8_weight_contributions != 0u &&
        handoff->summary.e8m0_scale_contributions != 0u &&
        handoff->summary.expert_contributions != 0u &&
        handoff->summary.i64_router_contributions != 0u &&
        handoff->summary.global_contributions != 0u &&
        handoff->summary.norm_contributions != 0u &&
        handoff->summary.shared_expert_contributions != 0u &&
        handoff->summary.output_head_contributions != 0u &&
        handoff->summary.mtp_contributions != 0u;
    if (!handoff->summary.complete)
        return handoff_reject(
            failure, YVEX_DEEPSEEK_PAYLOAD_FAILURE_CONTRIBUTION,
            ULLONG_MAX, ULLONG_MAX, YVEX_ERR_FORMAT, err,
            "mapping payload handoff lacks one required contribution class");
    return YVEX_OK;
}

/* Runs one canonical source pass and retains only typed downstream owners. */
static int payload_open(
    yvex_deepseek_payload_handoff **out,
    const yvex_deepseek_payload_handoff_options *options,
    yvex_deepseek_payload_failure *failure,
    yvex_error *err)
{
    yvex_deepseek_payload_handoff *handoff;
    yvex_source_tensor_snapshot *snapshot = NULL;
    yvex_deepseek_v4_ir *ir = NULL;
    yvex_deepseek_v4_ir_failure ir_failure;
    yvex_deepseek_tensor_coverage_failure coverage_failure;
    yvex_deepseek_gguf_map_failure map_failure;
    yvex_transform_failure transform_failure;
    yvex_source_payload_open_options payload_options;
    int rc;

    if (out) *out = NULL;
    if (!out || !options || !options->source_path ||
        !options->source_path[0] || !options->models_root ||
        !options->models_root[0]) {
        return handoff_reject(
            failure, YVEX_DEEPSEEK_PAYLOAD_FAILURE_INVALID_ARGUMENT,
            ULLONG_MAX, ULLONG_MAX, YVEX_ERR_INVALID_ARG, err,
            "source path, models root, and output are required");
    }
    handoff = (yvex_deepseek_payload_handoff *)calloc(1u, sizeof(*handoff));
    if (!handoff)
        return handoff_reject(
            failure, YVEX_DEEPSEEK_PAYLOAD_FAILURE_ALLOCATION,
            ULLONG_MAX, ULLONG_MAX, YVEX_ERR_NOMEM, err,
            "payload handoff allocation failed");
    handoff->source_path = handoff_strdup(options->source_path);
    handoff->models_root = handoff_strdup(options->models_root);
    handoff->manifest_path = options->manifest_path
        ? handoff_strdup(options->manifest_path) : NULL;
    if (!handoff->source_path || !handoff->models_root ||
        (options->manifest_path && !handoff->manifest_path)) {
        payload_close(handoff);
        return handoff_reject(
            failure, YVEX_DEEPSEEK_PAYLOAD_FAILURE_ALLOCATION,
            ULLONG_MAX, ULLONG_MAX, YVEX_ERR_NOMEM, err,
            "payload handoff path allocation failed");
    }
    handoff->source_options.identity = yvex_model_target_release_identity();
    handoff->source_options.source_path = handoff->source_path;
    handoff->source_options.models_root = handoff->models_root;
    handoff->source_options.manifest_path = handoff->manifest_path;
    handoff->source_options.promote_manifest = 1;
    rc = yvex_source_verify_with_snapshot(
        &handoff->source_options, &handoff->verification, &snapshot, err);
    if (rc != YVEX_OK || !handoff->verification.verified || !snapshot) {
        yvex_source_tensor_snapshot_release(snapshot);
        payload_close(handoff);
        return handoff_reject(
            failure, YVEX_DEEPSEEK_PAYLOAD_FAILURE_SOURCE,
            ULLONG_MAX, ULLONG_MAX, rc == YVEX_OK ? YVEX_ERR_STATE : rc, err,
            "exact source verification did not produce a retained snapshot");
    }
    rc = family_ir_build(
        &ir, &handoff->verification, &ir_failure, err);
    if (rc != YVEX_OK) {
        yvex_source_tensor_snapshot_release(snapshot);
        payload_close(handoff);
        if (failure) failure->code = YVEX_DEEPSEEK_PAYLOAD_FAILURE_ARCHITECTURE;
        return rc;
    }
    rc = coverage_build(
        &handoff->coverage, &handoff->verification, ir, snapshot, NULL,
        &coverage_failure, err);
    if (rc == YVEX_OK)
        rc = transform_ir_build(
            &handoff->transform_ir, &handoff->verification, ir,
            handoff->coverage, NULL, &transform_failure, err);
    if (rc == YVEX_OK)
        rc = lowering_build(
            &handoff->map, ir, handoff->transform_ir, &map_failure, err);
    family_ir_close(ir);
    if (rc != YVEX_OK) {
        yvex_deepseek_payload_failure_code code = !handoff->coverage
            ? YVEX_DEEPSEEK_PAYLOAD_FAILURE_COVERAGE
            : (!handoff->transform_ir
                ? YVEX_DEEPSEEK_PAYLOAD_FAILURE_TRANSFORM_IR
                : YVEX_DEEPSEEK_PAYLOAD_FAILURE_MAPPING);
        yvex_source_tensor_snapshot_release(snapshot);
        payload_close(handoff);
        if (failure) failure->code = code;
        return rc;
    }
    memset(&payload_options, 0, sizeof(payload_options));
    payload_options.verification_options = &handoff->source_options;
    payload_options.verification = &handoff->verification;
    payload_options.snapshot = snapshot;
    payload_options.budget = options->budget;
    payload_options.manifest_path = handoff->verification.manifest_path;
    rc = yvex_source_payload_session_open(
        &handoff->session, &payload_options,
        failure ? &failure->payload_failure : NULL, err);
    yvex_source_tensor_snapshot_release(snapshot);
    if (rc != YVEX_OK) {
        if (failure) failure->code = YVEX_DEEPSEEK_PAYLOAD_FAILURE_SOURCE;
        payload_close(handoff);
        return rc;
    }
    rc = yvex_transform_binding_create(
        &handoff->binding, handoff->transform_ir, handoff->session, NULL,
        &transform_failure, err);
    if (rc != YVEX_OK) {
        if (failure) failure->code = YVEX_DEEPSEEK_PAYLOAD_FAILURE_BINDING;
        payload_close(handoff);
        return rc;
    }
    rc = handoff_resolve(handoff, options, failure, err);
    if (rc != YVEX_OK) {
        payload_close(handoff);
        return rc;
    }
    *out = handoff;
    if (failure) memset(failure, 0, sizeof(*failure));
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Releases plan, session, map, coverage, and copied paths in dependency order. */
static void payload_close(
    yvex_deepseek_payload_handoff *handoff)
{
    if (!handoff) return;
    yvex_source_payload_plan_close(handoff->plan);
    yvex_transform_binding_release(&handoff->binding);
    (void)yvex_source_payload_session_release(&handoff->session, NULL, NULL);
    lowering_close(handoff->map);
    yvex_transform_ir_release(&handoff->transform_ir);
    coverage_close(handoff->coverage);
    free(handoff->manifest_path);
    free(handoff->models_root);
    free(handoff->source_path);
    free(handoff);
}

static const yvex_deepseek_payload_handoff_summary *
payload_summary(
    const yvex_deepseek_payload_handoff *handoff)
{
    return handoff ? &handoff->summary : NULL;
}

static const yvex_source_verification *payload_verification(
    const yvex_deepseek_payload_handoff *handoff)
{
    return handoff ? &handoff->verification : NULL;
}

static const yvex_deepseek_gguf_map *payload_map(
    const yvex_deepseek_payload_handoff *handoff)
{
    return handoff ? handoff->map : NULL;
}

static const yvex_transform_ir *payload_transform_ir(
    const yvex_deepseek_payload_handoff *handoff)
{
    return handoff ? handoff->transform_ir : NULL;
}

static const yvex_transform_binding *payload_binding(
    const yvex_deepseek_payload_handoff *handoff)
{
    return handoff ? handoff->binding : NULL;
}

static yvex_source_payload_session *payload_session(
    yvex_deepseek_payload_handoff *handoff)
{
    return handoff ? handoff->session : NULL;
}

static const yvex_source_payload_plan *payload_plan(
    const yvex_deepseek_payload_handoff *handoff)
{
    return handoff ? handoff->plan : NULL;
}

static const char *payload_failure_name(
    yvex_deepseek_payload_failure_code code)
{
    static const char *const names[] = {
        "none", "invalid-argument", "source-verification",
        "architecture-ir", "tensor-coverage", "transform-ir", "gguf-mapping",
        "mapping-identity-mismatch", "mapping-contribution",
        "payload-range", "transform-binding", "payload-plan",
        "allocation-failure"
    };
    size_t count = sizeof(names) / sizeof(names[0]);

    return code >= 0 && (size_t)code < count ? names[code]
                                                : "unknown-handoff-failure";
}

/* Publishes the single immutable family ABI.  The table contains functions,
 * not mutable state, and its address and contents remain stable for process
 * lifetime. */
const yvex_model_family_api *yvex_model_register_deepseek_v4(void)
{
    static const yvex_model_family_api api = {
        1u,
        "deepseek-v4-flash",
        {
            family_ir_build,
            family_ir_build_with_allocator,
            family_ir_close,
            family_ir_model,
            family_ir_layer_count,
            family_ir_layer_at,
            family_ir_auxiliary_count,
            family_ir_auxiliary_at,
            family_ir_failure_name,
            family_ir_component_name,
            family_attention_name,
            family_kv_name,
            family_router_name,
            family_source_weight_dtype_name,
            family_source_expert_dtype_name,
            family_source_quantization_name,
            family_activation_stage_name,
            family_activation_quantization_name,
            family_runtime_transform_name,
            family_sparse_topk_policy_name
        },
        {
            coverage_build,
            coverage_open_verified_source,
            coverage_close,
            coverage_summary,
            coverage_at,
            coverage_find,
            coverage_find_index,
            coverage_find_source_index,
            coverage_collection_name,
            coverage_failure_name
        },
        {
            transform_architecture_identity,
            transform_ir_build
        },
        {
            lowering_build,
            lowering_build_with_allocator,
            lowering_close,
            lowering_summary,
            lowering_at,
            lowering_contribution_at,
            lowering_find_source,
            lowering_find_emitted,
            lowering_find_role,
            lowering_metadata_at,
            lowering_metadata_find,
            lowering_transform_name,
            lowering_failure_name
        },
        {
            payload_open,
            payload_close,
            payload_summary,
            payload_verification,
            payload_map,
            payload_transform_ir,
            payload_binding,
            payload_session,
            payload_plan,
            payload_failure_name
        }
    };

    return &api;
}
