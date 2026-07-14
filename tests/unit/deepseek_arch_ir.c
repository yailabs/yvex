/*
 * YVEX - canonical DeepSeek-V4-Flash architecture IR tests.
 *
 * Exercises normalization, layer topology, typed refusal, owned lifetime,
 * allocation cleanup, and the model-class report consumer without source IO
 * or tensor payload reads.
 */
#include "test.h"

#include "src/model/architecture/yvex_deepseek_v4_ir.h"
#include "src/model/target/yvex_model_class_profile.h"
#include "src/model/target/yvex_model_target_catalog.h"
#include "src/source/yvex_source_verify.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

static void arch_ir_copy(char *out, size_t cap, const char *value)
{
    size_t length = strlen(value);

    if (length >= cap) length = cap - 1u;
    memcpy(out, value, length);
    out[length] = '\0';
}

/* Constructs exact already-verified facts without simulating source parsing. */
static void arch_ir_verification_fixture(yvex_source_verification *source)
{
    unsigned long long i;

    memset(source, 0, sizeof(*source));
    source->verified = 1;
    source->path_verified = 1;
    source->repository_verified = 1;
    source->revision_verified = 1;
    source->manifest_verified = 1;
    source->manifest_reopened = 1;
    source->upstream_index_identity_verified = 1;
    source->config_valid = 1;
    source->tokenizer_json_valid = 1;
    source->tokenizer_config_valid = 1;
    source->generation_config_valid = 1;
    source->shard_index_present = 1;
    source->shard_index_valid = 1;
    source->shard_index_headers_match = 1;
    source->header_scan_count = 1;
    source->header_shard_count = 46;
    source->header_tensor_count = 69187;
    source->shard_count = 46;
    source->indexed_tensor_count = 69187;
    arch_ir_copy(source->resolved_source_path,
                 sizeof(source->resolved_source_path),
                 "/verified/DeepSeek-V4-Flash");
    arch_ir_copy(source->manifest_target_id,
                 sizeof(source->manifest_target_id),
                 yvex_deepseek_v4_target_id);
    arch_ir_copy(source->verification_stage,
                 sizeof(source->verification_stage),
                 "exact-source-metadata-header-verified");
    arch_ir_copy(source->inventory_authority,
                 sizeof(source->inventory_authority), "upstream-index");
    arch_ir_copy(source->upstream_index_oid,
                 sizeof(source->upstream_index_oid),
                 yvex_deepseek_v4_upstream_index_oid);
    arch_ir_copy(source->local_index_oid,
                 sizeof(source->local_index_oid),
                 yvex_deepseek_v4_upstream_index_oid);
    arch_ir_copy(source->source_kind, sizeof(source->source_kind),
                 "huggingface");
    arch_ir_copy(source->repository_id, sizeof(source->repository_id),
                 yvex_deepseek_v4_upstream_repo_id);
    arch_ir_copy(source->revision, sizeof(source->revision),
                 yvex_deepseek_v4_upstream_revision);
    arch_ir_copy(source->model_type, sizeof(source->model_type),
                 yvex_deepseek_v4_config_model_type);
    arch_ir_copy(source->architecture, sizeof(source->architecture),
                 yvex_deepseek_v4_config_architecture);
    arch_ir_copy(source->torch_dtype, sizeof(source->torch_dtype),
                 "bfloat16");
    arch_ir_copy(source->expert_dtype, sizeof(source->expert_dtype), "fp4");
    arch_ir_copy(source->hidden_act, sizeof(source->hidden_act), "silu");
    arch_ir_copy(source->scoring_func, sizeof(source->scoring_func),
                 "sqrtsoftplus");
    arch_ir_copy(source->topk_method, sizeof(source->topk_method),
                 "noaux_tc");
    arch_ir_copy(source->tokenizer_model_type,
                 sizeof(source->tokenizer_model_type), "BPE");
    arch_ir_copy(source->tokenizer_class, sizeof(source->tokenizer_class),
                 "PreTrainedTokenizerFast");
    arch_ir_copy(source->rope_scaling_type,
                 sizeof(source->rope_scaling_type), "yarn");
    arch_ir_copy(source->quant_method, sizeof(source->quant_method), "fp8");
    arch_ir_copy(source->quant_format, sizeof(source->quant_format), "e4m3");
    arch_ir_copy(source->quant_activation_scheme,
                 sizeof(source->quant_activation_scheme), "dynamic");
    arch_ir_copy(source->quant_scale_format,
                 sizeof(source->quant_scale_format), "ue8m0");
    arch_ir_copy(source->attention_dropout,
                 sizeof(source->attention_dropout), "0.0");
    arch_ir_copy(source->hc_eps, sizeof(source->hc_eps), "0.000001");
    arch_ir_copy(source->rms_norm_eps, sizeof(source->rms_norm_eps),
                 "0.000001");
    arch_ir_copy(source->routed_scaling_factor,
                 sizeof(source->routed_scaling_factor), "1.5");
    arch_ir_copy(source->swiglu_limit, sizeof(source->swiglu_limit), "10.0");
    source->hidden_size = 4096;
    source->num_hidden_layers = 43;
    source->num_attention_heads = 64;
    source->num_key_value_heads = 1;
    source->head_dim = 512;
    source->qk_rope_head_dim = 64;
    source->max_position_embeddings = 1048576;
    source->moe_intermediate_size = 2048;
    source->n_routed_experts = 256;
    source->n_shared_experts = 1;
    source->num_experts_per_tok = 6;
    source->num_hash_layers = 3;
    source->q_lora_rank = 1024;
    source->o_lora_rank = 1024;
    source->vocab_size = 129280;
    source->sliding_window = 128;
    source->bos_token_id = 0;
    source->eos_token_id = 1;
    source->compress_ratio_count = 44;
    for (i = 0u; i < source->compress_ratio_count; ++i) {
        source->compress_ratios[i] =
            i < 2u || i == 43u ? 0u : (i % 2u == 0u ? 4u : 128u);
    }
    source->compress_rope_theta = 160000;
    source->hc_mult = 4;
    source->hc_sinkhorn_iters = 20;
    source->index_head_dim = 128;
    source->index_n_heads = 64;
    source->index_topk = 512;
    source->num_nextn_predict_layers = 1;
    source->o_groups = 8;
    source->rope_theta = 10000;
    source->attention_bias = 0;
    source->norm_topk_prob = 1;
    source->use_cache = 1;
    source->rope_scaling_factor = 16;
    source->rope_original_context = 65536;
    source->rope_beta_fast = 32;
    source->rope_beta_slow = 1;
    source->quant_block_rows = 128;
    source->quant_block_columns = 128;
    source->tokenizer_base_vocab_count = 128000;
    source->tokenizer_added_token_count = 1283;
    source->tokenizer_max_token_id = 129279;
    source->tokenizer_effective_vocab_size = 129280;
    source->tokenizer_model_max_length = 1048576;
    source->generation_bos_token_id = 0;
    source->generation_eos_token_id = 1;
    source->generation_from_model_config = 1;
    source->generation_do_sample = 1;
    source->tie_word_embeddings = 0;
}

static int arch_ir_build(yvex_source_verification *source,
                         yvex_deepseek_v4_ir **ir,
                         yvex_deepseek_v4_ir_failure *failure)
{
    yvex_error err;

    yvex_error_clear(&err);
    return yvex_deepseek_v4_ir_build(ir, source, failure, &err);
}

static int arch_ir_expect_failure(
    yvex_source_verification *source,
    yvex_deepseek_v4_ir_failure_code expected,
    const char *message)
{
    yvex_deepseek_v4_ir *ir = NULL;
    yvex_deepseek_v4_ir_failure failure;

    YVEX_TEST_ASSERT(arch_ir_build(source, &ir, &failure) != YVEX_OK,
                     message);
    YVEX_TEST_ASSERT(ir == NULL, "refused architecture publishes no IR");
    YVEX_TEST_ASSERT(failure.code == expected, message);
    return 0;
}

static int test_arch_ir_golden_topology(void)
{
    yvex_source_verification source;
    yvex_deepseek_v4_ir *ir = NULL;
    yvex_deepseek_v4_ir_failure failure;
    const yvex_deepseek_v4_model_spec *model;
    const yvex_deepseek_v4_layer_spec *swa;
    const yvex_deepseek_v4_layer_spec *csa;
    const yvex_deepseek_v4_layer_spec *hca;
    const yvex_deepseek_v4_auxiliary_spec *mtp;

    arch_ir_verification_fixture(&source);
    YVEX_TEST_ASSERT(arch_ir_build(&source, &ir, &failure) == YVEX_OK && ir,
                     "exact verified target builds architecture IR");
    model = yvex_deepseek_v4_ir_model(ir);
    YVEX_TEST_ASSERT(model && model->main_layer_count == 43 &&
                     model->auxiliary_layer_count == 1 &&
                     model->hidden_size == 4096 &&
                     model->vocabulary_size == 129280 &&
                     model->maximum_context == 1048576,
                     "global DeepSeek-V4-Flash geometry is normalized");
    YVEX_TEST_ASSERT(model->swa_layer_count == 2 &&
                     model->csa_layer_count == 21 &&
                     model->hca_layer_count == 20,
                     "hybrid attention counts match exact schedule");
    YVEX_TEST_ASSERT(model->hash_router_layer_count == 3 &&
                     model->learned_router_layer_count == 40,
                     "main-layer router classes are explicit");
    YVEX_TEST_ASSERT(model->final_mhc.residual_streams == 4 &&
                     model->final_mhc.expanded_width == 16384 &&
                     model->final_mhc.mixing_rows == 24 &&
                     model->final_mhc.mixing_columns == 16384 &&
                     model->final_mhc.sinkhorn_iterations == 20,
                     "mHC dimensions and iteration policy are explicit");
    YVEX_TEST_ASSERT(model->final_mhc_post_required &&
                     model->final_mhc_head_required &&
                     model->final_norm_after_mhc_head &&
                     model->final_mhc_head.function_rows == 4 &&
                     model->final_mhc_head.function_columns == 16384 &&
                     model->final_mhc_head.base_width == 4 &&
                     model->final_mhc_head.scale_width == 1,
                     "final mHC collapse precedes final norm");
    YVEX_TEST_ASSERT(model->embedding.required && model->output.required &&
                     !model->output.tied_to_embedding &&
                     model->tokenizer.vocabulary_size == 129280 &&
                     model->tokenizer.bos_token_id == 0 &&
                     model->tokenizer.eos_token_id == 1,
                     "embedding output and tokenizer requirements agree");
    YVEX_TEST_ASSERT(model->source_constraint.quant_block_rows == 128 &&
                     model->source_constraint.quant_block_columns == 128 &&
                     model->source_constraint.fp4_packing_factor == 2 &&
                     model->source_constraint.fp4_scale_group_width == 32 &&
                     model->source_constraint.fp4_physical_dtype ==
                         YVEX_NATIVE_DTYPE_I8 &&
                     model->source_constraint.scale_dtype ==
                         YVEX_NATIVE_DTYPE_F8_E8M0 &&
                     model->source_payload_bytes_read == 0 &&
                     model->source_header_scan_count == 1,
                     "source constraints remain typed without payload reads");
    YVEX_TEST_ASSERT_STREQ(
        yvex_deepseek_v4_source_quantization_name(
            model->source_constraint.quantization),
        "fp8-e4m3-ue8m0-dynamic",
        "source quantization rendering projects the typed constraint");
    YVEX_TEST_ASSERT_STREQ(
        model->sglang_revision,
        "96a04cb13f9c3ed86028e090784a9eb059cf5318",
        "SGLang interpretation baseline is immutable");
    YVEX_TEST_ASSERT_STREQ(
        model->vllm_revision,
        "8df14cfc8c8a09b4e57f082e59593a3abce4ffb3",
        "vLLM interpretation baseline is immutable");

    swa = yvex_deepseek_v4_ir_layer_at(ir, 0);
    csa = yvex_deepseek_v4_ir_layer_at(ir, 2);
    hca = yvex_deepseek_v4_ir_layer_at(ir, 3);
    YVEX_TEST_ASSERT(swa && csa && hca &&
                     swa->attention_class == YVEX_DEEPSEEK_V4_ATTENTION_SWA &&
                     csa->attention_class == YVEX_DEEPSEEK_V4_ATTENTION_CSA &&
                     hca->attention_class == YVEX_DEEPSEEK_V4_ATTENTION_HCA,
                     "every attention class has an explicit layer descriptor");
    YVEX_TEST_ASSERT(swa->head_dimension == 512 &&
                     swa->rope_head_dimension == 64 &&
                     swa->non_rope_head_dimension == 448 &&
                     swa->position.theta == 10000 &&
                     csa->position.theta == 160000,
                     "head partition and position bases are normalized");
    YVEX_TEST_ASSERT(csa->compressor_required && csa->indexer_required &&
                     csa->indexer_heads == 64 &&
                     csa->indexer_head_dimension == 128 &&
                     csa->indexer_topk == 512 &&
                     csa->kv.requires_indexer_cache,
                     "CSA compressor indexer and KV state are explicit");
    YVEX_TEST_ASSERT(csa->attention_input_norm.required &&
                     csa->attention_input_norm.width == 4096 &&
                     csa->post_attention_ffn_norm.required &&
                     csa->post_attention_ffn_norm.width == 4096 &&
                     csa->tensors.compressor_ape_rows == 4 &&
                     csa->tensors.compressor_ape_columns == 1024 &&
                     csa->tensors.indexer_query_rows == 8192 &&
                     csa->tensors.indexer_weight_rows == 64,
                     "norm and compressed-attention tensor geometry is explicit");
    YVEX_TEST_ASSERT(hca->compressor_required && !hca->indexer_required &&
                     hca->kv.requires_compressed_core &&
                     !hca->kv.requires_indexer_cache,
                     "HCA uses heavy compression without sparse indexer");
    YVEX_TEST_ASSERT(swa->moe.router_class ==
                         YVEX_DEEPSEEK_V4_ROUTER_HASH_TOKEN_ID &&
                     swa->moe.requires_token_ids &&
                     swa->moe.hash_table_rows == 129280 &&
                     swa->moe.hash_table_columns == 6,
                     "hash MoE requires token-id routing table");
    YVEX_TEST_ASSERT(hca->moe.router_class ==
                         YVEX_DEEPSEEK_V4_ROUTER_LEARNED_HIDDEN_STATE &&
                     hca->moe.requires_hidden_state &&
                     hca->moe.requires_correction_bias &&
                     hca->moe.correction_bias_width == 256,
                     "learned MoE requires hidden-state noaux routing");

    mtp = yvex_deepseek_v4_ir_auxiliary_at(ir, 0);
    YVEX_TEST_ASSERT(mtp && mtp->layer.layer_index == 43 &&
                     mtp->layer.attention_class ==
                         YVEX_DEEPSEEK_V4_ATTENTION_SWA &&
                     mtp->layer.moe.router_class ==
                         YVEX_DEEPSEEK_V4_ROUTER_LEARNED_HIDDEN_STATE &&
                     mtp->previous_hidden_width == 16384 &&
                     mtp->requires_token_embedding &&
                     mtp->requires_previous_hidden_state &&
                     mtp->requires_separate_mhc_head &&
                     mtp->mhc_head.function_rows == 4 &&
                     mtp->mhc_head.function_columns == 16384 &&
                     mtp->shares_output_head && mtp->shares_final_norm,
                     "MTP topology is separate from the 43 main layers");
    YVEX_TEST_ASSERT(yvex_deepseek_v4_ir_layer_at(ir, 43) == NULL &&
                     yvex_deepseek_v4_ir_auxiliary_at(ir, 1) == NULL,
                     "borrowed accessors are bounds checked");
    yvex_deepseek_v4_ir_close(ir);
    return 0;
}

static int test_arch_ir_refusal_matrix(void)
{
    yvex_source_verification source;

    arch_ir_verification_fixture(&source);
    source.verified = 0;
    if (arch_ir_expect_failure(&source,
            YVEX_DEEPSEEK_V4_IR_FAILURE_SOURCE_NOT_VERIFIED,
            "unverified source refuses") != 0) return 1;

    arch_ir_verification_fixture(&source);
    arch_ir_copy(source.repository_id, sizeof(source.repository_id),
                 "wrong/repository");
    if (arch_ir_expect_failure(&source,
            YVEX_DEEPSEEK_V4_IR_FAILURE_IDENTITY_MISMATCH,
            "wrong identity refuses") != 0) return 1;

    arch_ir_verification_fixture(&source);
    source.config_valid = 0;
    if (arch_ir_expect_failure(&source,
            YVEX_DEEPSEEK_V4_IR_FAILURE_SOURCE_NOT_VERIFIED,
            "incomplete sidecar evidence refuses") != 0) return 1;

    arch_ir_verification_fixture(&source);
    source.compress_ratio_count = 43;
    if (arch_ir_expect_failure(&source,
            YVEX_DEEPSEEK_V4_IR_FAILURE_SCHEDULE_LENGTH,
            "schedule length mismatch refuses") != 0) return 1;

    arch_ir_verification_fixture(&source);
    source.compress_ratios[7] = 7;
    if (arch_ir_expect_failure(&source,
            YVEX_DEEPSEEK_V4_IR_FAILURE_UNSUPPORTED_COMPRESSION,
            "unknown compression refuses") != 0) return 1;

    arch_ir_verification_fixture(&source);
    source.compress_ratios[4] = 128;
    if (arch_ir_expect_failure(&source,
            YVEX_DEEPSEEK_V4_IR_FAILURE_SCHEDULE_PATTERN,
            "non-alternating hybrid schedule refuses") != 0) return 1;

    arch_ir_verification_fixture(&source);
    source.qk_rope_head_dim = 512;
    if (arch_ir_expect_failure(&source,
            YVEX_DEEPSEEK_V4_IR_FAILURE_INVALID_DIMENSION,
            "invalid head partition refuses") != 0) return 1;

    arch_ir_verification_fixture(&source);
    source.o_groups = 7;
    if (arch_ir_expect_failure(&source,
            YVEX_DEEPSEEK_V4_IR_FAILURE_INVALID_GROUP_GEOMETRY,
            "invalid output grouping refuses") != 0) return 1;

    arch_ir_verification_fixture(&source);
    source.rope_beta_slow = 64;
    if (arch_ir_expect_failure(&source,
            YVEX_DEEPSEEK_V4_IR_FAILURE_INVALID_POSITION,
            "invalid YaRN parameters refuse") != 0) return 1;

    arch_ir_verification_fixture(&source);
    source.use_cache = 0;
    if (arch_ir_expect_failure(&source,
            YVEX_DEEPSEEK_V4_IR_FAILURE_INVALID_POSITION,
            "missing cache-state requirement refuses") != 0) return 1;

    arch_ir_verification_fixture(&source);
    source.hc_mult = 0;
    if (arch_ir_expect_failure(&source,
            YVEX_DEEPSEEK_V4_IR_FAILURE_INVALID_MHC,
            "invalid mHC dimensions refuse") != 0) return 1;

    arch_ir_verification_fixture(&source);
    source.hidden_size = ULLONG_MAX;
    if (arch_ir_expect_failure(&source,
            YVEX_DEEPSEEK_V4_IR_FAILURE_ARITHMETIC_OVERFLOW,
            "mHC checked geometry overflow refuses") != 0) return 1;

    arch_ir_verification_fixture(&source);
    source.num_hash_layers = 44;
    if (arch_ir_expect_failure(&source,
            YVEX_DEEPSEEK_V4_IR_FAILURE_INVALID_ROUTING,
            "excessive hash layer count refuses") != 0) return 1;

    arch_ir_verification_fixture(&source);
    source.num_experts_per_tok = 257;
    if (arch_ir_expect_failure(&source,
            YVEX_DEEPSEEK_V4_IR_FAILURE_INVALID_EXPERT_TOPK,
            "invalid expert top-k refuses") != 0) return 1;

    arch_ir_verification_fixture(&source);
    source.tokenizer_effective_vocab_size = 129279;
    if (arch_ir_expect_failure(&source,
            YVEX_DEEPSEEK_V4_IR_FAILURE_TOKENIZER_OUTPUT_MISMATCH,
            "tokenizer and output vocabulary mismatch refuses") != 0) return 1;

    arch_ir_verification_fixture(&source);
    arch_ir_copy(source.quant_scale_format,
                 sizeof(source.quant_scale_format), "unknown");
    if (arch_ir_expect_failure(&source,
            YVEX_DEEPSEEK_V4_IR_FAILURE_UNSUPPORTED_SOURCE_CONSTRAINT,
            "unknown source quantization refuses") != 0) return 1;

    arch_ir_verification_fixture(&source);
    arch_ir_copy(source.inventory_authority,
                 sizeof(source.inventory_authority), "header-derived");
    if (arch_ir_expect_failure(&source,
            YVEX_DEEPSEEK_V4_IR_FAILURE_SOURCE_FACT_MISSING,
            "header-derived inventory cannot admit release IR") != 0) return 1;

    arch_ir_verification_fixture(&source);
    source.quant_block_rows = 64;
    if (arch_ir_expect_failure(&source,
            YVEX_DEEPSEEK_V4_IR_FAILURE_UNSUPPORTED_SOURCE_CONSTRAINT,
            "non-pinned quant block geometry refuses") != 0) return 1;

    arch_ir_verification_fixture(&source);
    arch_ir_copy(source.rms_norm_eps, sizeof(source.rms_norm_eps), "nan");
    if (arch_ir_expect_failure(&source,
            YVEX_DEEPSEEK_V4_IR_FAILURE_NUMERIC_VALUE,
            "non-finite numeric source fact refuses") != 0) return 1;
    return 0;
}

/* Admits the stronger v3 payload stage only when its parsed trust fact is set. */
static int test_arch_ir_payload_manifest_stage(void)
{
    yvex_source_verification source;
    yvex_deepseek_v4_ir *ir = NULL;
    yvex_deepseek_v4_ir_failure failure;

    arch_ir_verification_fixture(&source);
    arch_ir_copy(source.verification_stage,
                 sizeof(source.verification_stage),
                 "exact-source-payload-verified");
    source.manifest_payload_trusted = 1;
    YVEX_TEST_ASSERT(arch_ir_build(&source, &ir, &failure) == YVEX_OK && ir,
                     "trusted v3 payload manifest remains strict IR input");
    yvex_deepseek_v4_ir_close(ir);

    source.manifest_payload_trusted = 0;
    if (arch_ir_expect_failure(
            &source, YVEX_DEEPSEEK_V4_IR_FAILURE_SOURCE_FACT_MISSING,
            "untrusted payload-stage label cannot bypass source admission") !=
        0) return 1;
    return 0;
}

typedef struct {
    unsigned int call_count;
    unsigned int fail_call;
    unsigned int live_allocations;
} arch_ir_allocator_state;

static void *arch_ir_test_allocate(size_t size, void *context)
{
    arch_ir_allocator_state *state = (arch_ir_allocator_state *)context;
    void *allocation;

    if (state->call_count++ == state->fail_call) return NULL;
    allocation = malloc(size);
    if (allocation) state->live_allocations++;
    return allocation;
}

static void arch_ir_test_release(void *allocation, void *context)
{
    arch_ir_allocator_state *state = (arch_ir_allocator_state *)context;

    if (!allocation) return;
    free(allocation);
    state->live_allocations--;
}

static int test_arch_ir_lifetime_and_allocation(void)
{
    yvex_source_verification *source;
    yvex_deepseek_v4_ir *ir = NULL;
    yvex_deepseek_v4_ir_failure failure;
    yvex_error err;
    unsigned int fail_call;

    source = (yvex_source_verification *)malloc(sizeof(*source));
    YVEX_TEST_ASSERT(source != NULL, "allocate source evidence fixture");
    arch_ir_verification_fixture(source);
    YVEX_TEST_ASSERT(arch_ir_build(source, &ir, &failure) == YVEX_OK && ir,
                     "build owned lifetime IR");
    memset(source, 0, sizeof(*source));
    free(source);
    YVEX_TEST_ASSERT_STREQ(yvex_deepseek_v4_ir_model(ir)->repository,
                           yvex_deepseek_v4_upstream_repo_id,
                           "IR remains valid after source evidence release");
    YVEX_TEST_ASSERT(yvex_deepseek_v4_ir_layer_at(ir, 42)->layer_index == 42,
                     "owned layer collection survives source release");
    yvex_deepseek_v4_ir_close(ir);

    for (fail_call = 0u; fail_call < 3u; ++fail_call) {
        yvex_source_verification fixture;
        arch_ir_allocator_state state;
        yvex_deepseek_v4_ir_allocator allocator;
        int rc;

        arch_ir_verification_fixture(&fixture);
        memset(&state, 0, sizeof(state));
        state.fail_call = fail_call;
        allocator.allocate = arch_ir_test_allocate;
        allocator.release = arch_ir_test_release;
        allocator.context = &state;
        ir = NULL;
        yvex_error_clear(&err);
        rc = yvex_deepseek_v4_ir_build_with_allocator(
            &ir, &fixture, &allocator, &failure, &err);
        YVEX_TEST_ASSERT(rc == YVEX_ERR_NOMEM && ir == NULL &&
                         failure.code ==
                             YVEX_DEEPSEEK_V4_IR_FAILURE_ALLOCATION,
                         "allocation failure refuses partial IR");
        YVEX_TEST_ASSERT(state.live_allocations == 0,
                         "partial construction releases every allocation");
    }
    return 0;
}

static int report_has(const yvex_model_target_report *report,
                      const char *text)
{
    unsigned long i;

    for (i = 0u; report && i < report->row_count; ++i) {
        if (strstr(report->rows[i].value, text)) return 1;
    }
    return 0;
}

static int test_arch_ir_report_consumer_and_family_preservation(void)
{
    yvex_source_verification source;
    yvex_model_target_request request;
    yvex_model_target_report report;
    yvex_error err;

    arch_ir_verification_fixture(&source);
    memset(&request, 0, sizeof(request));
    memset(&report, 0, sizeof(report));
    request.kind = YVEX_MODEL_TARGET_COMMAND_CLASS_PROFILE;
    request.mode = YVEX_MODEL_TARGET_OUTPUT_AUDIT;
    arch_ir_copy(request.target_id, sizeof(request.target_id),
                 yvex_deepseek_v4_target_id);
    yvex_error_clear(&err);
    YVEX_TEST_ASSERT(
        yvex_model_class_profile_deepseek_from_verification(
            &request, &source, &report, &err) == YVEX_OK &&
            report.deepseek_architecture_ir != NULL &&
            strcmp(report.status, "typed-architecture-specified") == 0 &&
            strcmp(report.next_row,
                   "V010.SOURCE.PAYLOAD.STREAM.0") == 0,
        "release class profile consumes canonical IR");
    YVEX_TEST_ASSERT(source.header_scan_count == 1,
                     "IR consumer does not rescan source headers");
    yvex_model_target_report_close(&report);

    memset(&request, 0, sizeof(request));
    memset(&report, 0, sizeof(report));
    request.kind = YVEX_MODEL_TARGET_COMMAND_CLASS_PROFILE;
    request.mode = YVEX_MODEL_TARGET_OUTPUT_NORMAL;
    arch_ir_copy(request.target_id, sizeof(request.target_id), "qwen3-8b");
    arch_ir_copy(request.models_root, sizeof(request.models_root),
                 "build/tests/missing-arch-ir-qwen");
    YVEX_TEST_ASSERT(yvex_model_class_profile_report_build(
                         &request, &report, &err) == YVEX_OK &&
                     report_has(&report, "qwen-source-model-class-profile"),
                     "Qwen evidence path remains intact");
    yvex_model_target_report_close(&report);

    memset(&request, 0, sizeof(request));
    memset(&report, 0, sizeof(report));
    request.kind = YVEX_MODEL_TARGET_COMMAND_CLASS_PROFILE;
    request.mode = YVEX_MODEL_TARGET_OUTPUT_NORMAL;
    arch_ir_copy(request.target_id, sizeof(request.target_id),
                 "gemma-4-12b-it");
    arch_ir_copy(request.models_root, sizeof(request.models_root),
                 "build/tests/missing-arch-ir-gemma");
    YVEX_TEST_ASSERT(yvex_model_class_profile_report_build(
                         &request, &report, &err) == YVEX_OK &&
                     report_has(&report, "gemma-source-model-class-profile"),
                     "Gemma evidence path remains intact");
    yvex_model_target_report_close(&report);
    return 0;
}

int yvex_test_deepseek_arch_ir(void)
{
    if (test_arch_ir_golden_topology() != 0) return 1;
    if (test_arch_ir_refusal_matrix() != 0) return 1;
    if (test_arch_ir_payload_manifest_stage() != 0) return 1;
    if (test_arch_ir_lifetime_and_allocation() != 0) return 1;
    if (test_arch_ir_report_consumer_and_family_preservation() != 0) return 1;
    return 0;
}
