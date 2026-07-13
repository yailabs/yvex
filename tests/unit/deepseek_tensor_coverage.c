/* Exact DeepSeek tensor coverage, mutation, scale, and lifetime tests. */
#include "test.h"

#include "src/model/target/yvex_deepseek_tensor_coverage.h"
#include "src/model/target/yvex_model_target_catalog.h"
#include "src/source/yvex_source_private.h"

#include <stdlib.h>
#include <string.h>

typedef enum {
    FIXTURE_VALID = 0,
    FIXTURE_MISSING,
    FIXTURE_MISSING_SCALE,
    FIXTURE_RANK,
    FIXTURE_SHAPE,
    FIXTURE_DTYPE,
    FIXTURE_SCALE,
    FIXTURE_UNEXPECTED,
    FIXTURE_INVALID_LAYER,
    FIXTURE_INVALID_EXPERT,
    FIXTURE_INDEX_OVERFLOW
} fixture_mutation;

static void fixture_copy(char *out, size_t cap, const char *value)
{
    (void)snprintf(out, cap, "%s", value);
}

static void fixture_verification(yvex_source_verification *source)
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
    source->header_scan_count = 1u;
    source->header_shard_count = 46u;
    source->header_tensor_count = 69187u;
    source->shard_count = 46u;
    source->indexed_tensor_count = 69187u;
    fixture_copy(source->manifest_target_id,
                 sizeof(source->manifest_target_id),
                 yvex_deepseek_v4_target_id);
    fixture_copy(source->verification_stage,
                 sizeof(source->verification_stage),
                 "exact-source-metadata-header-verified");
    fixture_copy(source->inventory_authority,
                 sizeof(source->inventory_authority), "upstream-index");
    fixture_copy(source->upstream_index_oid,
                 sizeof(source->upstream_index_oid),
                 yvex_deepseek_v4_upstream_index_oid);
    fixture_copy(source->local_index_oid, sizeof(source->local_index_oid),
                 yvex_deepseek_v4_upstream_index_oid);
    fixture_copy(source->source_kind, sizeof(source->source_kind),
                 "huggingface");
    fixture_copy(source->repository_id, sizeof(source->repository_id),
                 yvex_deepseek_v4_upstream_repo_id);
    fixture_copy(source->revision, sizeof(source->revision),
                 yvex_deepseek_v4_upstream_revision);
    fixture_copy(source->model_type, sizeof(source->model_type),
                 yvex_deepseek_v4_config_model_type);
    fixture_copy(source->architecture, sizeof(source->architecture),
                 yvex_deepseek_v4_config_architecture);
    fixture_copy(source->torch_dtype, sizeof(source->torch_dtype), "bfloat16");
    fixture_copy(source->expert_dtype, sizeof(source->expert_dtype), "fp4");
    fixture_copy(source->hidden_act, sizeof(source->hidden_act), "silu");
    fixture_copy(source->scoring_func, sizeof(source->scoring_func),
                 "sqrtsoftplus");
    fixture_copy(source->topk_method, sizeof(source->topk_method), "noaux_tc");
    fixture_copy(source->tokenizer_model_type,
                 sizeof(source->tokenizer_model_type), "BPE");
    fixture_copy(source->tokenizer_class, sizeof(source->tokenizer_class),
                 "PreTrainedTokenizerFast");
    fixture_copy(source->rope_scaling_type,
                 sizeof(source->rope_scaling_type), "yarn");
    fixture_copy(source->quant_method, sizeof(source->quant_method), "fp8");
    fixture_copy(source->quant_format, sizeof(source->quant_format), "e4m3");
    fixture_copy(source->quant_activation_scheme,
                 sizeof(source->quant_activation_scheme), "dynamic");
    fixture_copy(source->quant_scale_format,
                 sizeof(source->quant_scale_format), "ue8m0");
    fixture_copy(source->attention_dropout,
                 sizeof(source->attention_dropout), "0.0");
    fixture_copy(source->hc_eps, sizeof(source->hc_eps), "0.000001");
    fixture_copy(source->rms_norm_eps, sizeof(source->rms_norm_eps),
                 "0.000001");
    fixture_copy(source->routed_scaling_factor,
                 sizeof(source->routed_scaling_factor), "1.5");
    fixture_copy(source->swiglu_limit, sizeof(source->swiglu_limit), "10.0");
    source->hidden_size = 4096u;
    source->num_hidden_layers = 43u;
    source->num_attention_heads = 64u;
    source->num_key_value_heads = 1u;
    source->head_dim = 512u;
    source->qk_rope_head_dim = 64u;
    source->max_position_embeddings = 1048576u;
    source->moe_intermediate_size = 2048u;
    source->n_routed_experts = 256u;
    source->n_shared_experts = 1u;
    source->num_experts_per_tok = 6u;
    source->num_hash_layers = 3u;
    source->q_lora_rank = 1024u;
    source->o_lora_rank = 1024u;
    source->vocab_size = 129280u;
    source->sliding_window = 128u;
    source->bos_token_id = 0u;
    source->eos_token_id = 1u;
    source->compress_ratio_count = 44u;
    for (i = 0u; i < 44u; ++i)
        source->compress_ratios[i] =
            i < 2u || i == 43u ? 0u : (i % 2u == 0u ? 4u : 128u);
    source->compress_rope_theta = 160000u;
    source->hc_mult = 4u;
    source->hc_sinkhorn_iters = 20u;
    source->index_head_dim = 128u;
    source->index_n_heads = 64u;
    source->index_topk = 512u;
    source->num_nextn_predict_layers = 1u;
    source->o_groups = 8u;
    source->rope_theta = 10000u;
    source->norm_topk_prob = 1;
    source->use_cache = 1;
    source->rope_scaling_factor = 16u;
    source->rope_original_context = 65536u;
    source->rope_beta_fast = 32u;
    source->rope_beta_slow = 1u;
    source->quant_block_rows = 128u;
    source->quant_block_columns = 128u;
    source->tokenizer_base_vocab_count = 128000u;
    source->tokenizer_added_token_count = 1283u;
    source->tokenizer_max_token_id = 129279u;
    source->tokenizer_effective_vocab_size = 129280u;
    source->tokenizer_model_max_length = 1048576u;
    source->generation_bos_token_id = 0u;
    source->generation_eos_token_id = 1u;
    source->generation_from_model_config = 1;
    source->generation_do_sample = 1;
}

typedef struct {
    yvex_native_weight_table *table;
    fixture_mutation mutation;
    yvex_error err;
} fixture_inventory;

/*
 * Assigns a stable synthetic shard from tensor identity so fixture insertion
 * order can change without changing the represented source snapshot.
 */
static unsigned int fixture_shard_for_name(const char *name)
{
    unsigned long long hash = 1469598103934665603ull;
    const unsigned char *cursor = (const unsigned char *)name;
    while (*cursor) {
        hash ^= (unsigned long long)*cursor++;
        hash *= 1099511628211ull;
    }
    return (unsigned int)(hash % 46u) + 1u;
}

static int fixture_add(fixture_inventory *fixture,
                       const char *name,
                       yvex_native_dtype dtype,
                       unsigned int rank,
                       const unsigned long long *input_dims)
{
    char shard[64];
    unsigned long long dims[2] = {0u, 0u};
    unsigned int i;

    if (fixture->mutation == FIXTURE_MISSING &&
        strcmp(name, "embed.weight") == 0) return YVEX_OK;
    if (fixture->mutation == FIXTURE_MISSING_SCALE &&
        strcmp(name, "layers.0.attn.wq_a.scale") == 0) return YVEX_OK;
    for (i = 0u; i < rank; ++i) dims[i] = input_dims[i];
    if (fixture->mutation == FIXTURE_RANK &&
        strcmp(name, "head.weight") == 0) rank = 1u;
    if (fixture->mutation == FIXTURE_SHAPE &&
        strcmp(name, "layers.0.attn.wq_a.weight") == 0) dims[0]--;
    if (fixture->mutation == FIXTURE_SCALE &&
        strcmp(name, "layers.0.attn.wq_a.scale") == 0) dims[1]--;
    if (fixture->mutation == FIXTURE_DTYPE &&
        strcmp(name, "head.weight") == 0) dtype = YVEX_NATIVE_DTYPE_F32;
    (void)snprintf(shard, sizeof(shard),
                   "model-%05u-of-00046.safetensors",
                   fixture_shard_for_name(name));
    return yvex_native_weight_table_add(
        fixture->table, name, shard, yvex_native_dtype_name(dtype), rank,
        dims, 0u, 1u, &fixture->err);
}

static int fixture_v(fixture_inventory *f, const char *name,
                     yvex_native_dtype dtype, unsigned long long width)
{
    unsigned long long dims[1] = {width};
    return fixture_add(f, name, dtype, 1u, dims);
}

static int fixture_m(fixture_inventory *f, const char *name,
                     yvex_native_dtype dtype, unsigned long long rows,
                     unsigned long long columns)
{
    unsigned long long dims[2] = {rows, columns};
    return fixture_add(f, name, dtype, 2u, dims);
}

static int fixture_fp8(fixture_inventory *f, const char *base,
                       unsigned long long rows, unsigned long long columns)
{
    char name[256];
    int rc;
    (void)snprintf(name, sizeof(name), "%s.weight", base);
    rc = fixture_m(f, name, YVEX_NATIVE_DTYPE_F8_E4M3, rows, columns);
    if (rc != YVEX_OK) return rc;
    (void)snprintf(name, sizeof(name), "%s.scale", base);
    return fixture_m(f, name, YVEX_NATIVE_DTYPE_F8_E8M0,
                     rows / 128u, columns / 128u);
}

static int fixture_fp4(fixture_inventory *f, const char *base,
                       unsigned long long rows, unsigned long long columns)
{
    char name[256];
    int rc;
    (void)snprintf(name, sizeof(name), "%s.weight", base);
    rc = fixture_m(f, name, YVEX_NATIVE_DTYPE_I8, rows, columns / 2u);
    if (rc != YVEX_OK) return rc;
    (void)snprintf(name, sizeof(name), "%s.scale", base);
    return fixture_m(f, name, YVEX_NATIVE_DTYPE_F8_E8M0,
                     rows, columns / 32u);
}

static int fixture_mhc(fixture_inventory *f, const char *prefix,
                       const char *kind)
{
    char name[256];
    int rc;
    (void)snprintf(name, sizeof(name), "%s.hc_%s_fn", prefix, kind);
    rc = fixture_m(f, name, YVEX_NATIVE_DTYPE_F32, 24u, 16384u);
    if (rc != YVEX_OK) return rc;
    (void)snprintf(name, sizeof(name), "%s.hc_%s_base", prefix, kind);
    rc = fixture_v(f, name, YVEX_NATIVE_DTYPE_F32, 24u);
    if (rc != YVEX_OK) return rc;
    (void)snprintf(name, sizeof(name), "%s.hc_%s_scale", prefix, kind);
    return fixture_v(f, name, YVEX_NATIVE_DTYPE_F32, 3u);
}

static int fixture_attention(fixture_inventory *f, const char *prefix,
                             unsigned long long ratio)
{
    char name[256];
    char base[256];
    int rc;
#define FV(suffix, dtype, width)                                                \
    do {                                                                        \
        (void)snprintf(name, sizeof(name), "%s.%s", prefix, suffix);          \
        rc = fixture_v(f, name, dtype, width);                                  \
        if (rc != YVEX_OK) return rc;                                           \
    } while (0)
#define FP8(suffix, rows, columns)                                              \
    do {                                                                        \
        (void)snprintf(base, sizeof(base), "%s.%s", prefix, suffix);          \
        rc = fixture_fp8(f, base, rows, columns);                               \
        if (rc != YVEX_OK) return rc;                                           \
    } while (0)
    FV("attn.attn_sink", YVEX_NATIVE_DTYPE_F32, 64u);
    FV("attn.q_norm.weight", YVEX_NATIVE_DTYPE_BF16, 1024u);
    FV("attn.kv_norm.weight", YVEX_NATIVE_DTYPE_BF16, 512u);
    FP8("attn.wkv", 512u, 4096u);
    FP8("attn.wq_a", 1024u, 4096u);
    FP8("attn.wq_b", 32768u, 1024u);
    FP8("attn.wo_a", 8192u, 4096u);
    FP8("attn.wo_b", 4096u, 8192u);
    if (ratio) {
        unsigned long long width = ratio == 4u ? 1024u : 512u;
        (void)snprintf(name, sizeof(name), "%s.attn.compressor.ape", prefix);
        rc = fixture_m(f, name, YVEX_NATIVE_DTYPE_F32, ratio, width);
        if (rc != YVEX_OK) return rc;
        FV("attn.compressor.norm.weight", YVEX_NATIVE_DTYPE_BF16, 512u);
        (void)snprintf(name, sizeof(name),
                       "%s.attn.compressor.wgate.weight", prefix);
        rc = fixture_m(f, name, YVEX_NATIVE_DTYPE_BF16, width, 4096u);
        if (rc != YVEX_OK) return rc;
        (void)snprintf(name, sizeof(name),
                       "%s.attn.compressor.wkv.weight", prefix);
        rc = fixture_m(f, name, YVEX_NATIVE_DTYPE_BF16, width, 4096u);
        if (rc != YVEX_OK) return rc;
    }
    if (ratio == 4u) {
        (void)snprintf(name, sizeof(name),
                       "%s.attn.indexer.compressor.ape", prefix);
        rc = fixture_m(f, name, YVEX_NATIVE_DTYPE_F32, 4u, 256u);
        if (rc != YVEX_OK) return rc;
        FV("attn.indexer.compressor.norm.weight",
           YVEX_NATIVE_DTYPE_BF16, 128u);
        (void)snprintf(name, sizeof(name),
                       "%s.attn.indexer.compressor.wgate.weight", prefix);
        rc = fixture_m(f, name, YVEX_NATIVE_DTYPE_BF16, 256u, 4096u);
        if (rc != YVEX_OK) return rc;
        (void)snprintf(name, sizeof(name),
                       "%s.attn.indexer.compressor.wkv.weight", prefix);
        rc = fixture_m(f, name, YVEX_NATIVE_DTYPE_BF16, 256u, 4096u);
        if (rc != YVEX_OK) return rc;
        FP8("attn.indexer.wq_b", 8192u, 1024u);
        (void)snprintf(name, sizeof(name),
                       "%s.attn.indexer.weights_proj.weight", prefix);
        rc = fixture_m(f, name, YVEX_NATIVE_DTYPE_BF16, 64u, 4096u);
        if (rc != YVEX_OK) return rc;
    }
#undef FP8
#undef FV
    return YVEX_OK;
}

static int fixture_layer(fixture_inventory *f, const char *prefix,
                         unsigned long long layer, unsigned long long ratio,
                         int hash_router)
{
    char name[256];
    char base[256];
    unsigned long long expert;
    int rc = fixture_attention(f, prefix, ratio);
    if (rc != YVEX_OK) return rc;
    (void)snprintf(name, sizeof(name), "%s.attn_norm.weight", prefix);
    rc = fixture_v(f, name, YVEX_NATIVE_DTYPE_BF16, 4096u);
    if (rc != YVEX_OK) return rc;
    (void)snprintf(name, sizeof(name), "%s.ffn_norm.weight", prefix);
    rc = fixture_v(f, name, YVEX_NATIVE_DTYPE_BF16, 4096u);
    if (rc != YVEX_OK) return rc;
    rc = fixture_mhc(f, prefix, "attn");
    if (rc != YVEX_OK) return rc;
    rc = fixture_mhc(f, prefix, "ffn");
    if (rc != YVEX_OK) return rc;
    for (expert = 0u; expert < 256u; ++expert) {
        (void)snprintf(base, sizeof(base), "%s.ffn.experts.%llu.w1",
                       prefix, expert);
        rc = fixture_fp4(f, base, 2048u, 4096u);
        if (rc != YVEX_OK) return rc;
        (void)snprintf(base, sizeof(base), "%s.ffn.experts.%llu.w2",
                       prefix, expert);
        rc = fixture_fp4(f, base, 4096u, 2048u);
        if (rc != YVEX_OK) return rc;
        (void)snprintf(base, sizeof(base), "%s.ffn.experts.%llu.w3",
                       prefix, expert);
        rc = fixture_fp4(f, base, 2048u, 4096u);
        if (rc != YVEX_OK) return rc;
    }
    (void)snprintf(base, sizeof(base), "%s.ffn.shared_experts.w1", prefix);
    rc = fixture_fp8(f, base, 2048u, 4096u);
    if (rc != YVEX_OK) return rc;
    (void)snprintf(base, sizeof(base), "%s.ffn.shared_experts.w2", prefix);
    rc = fixture_fp8(f, base, 4096u, 2048u);
    if (rc != YVEX_OK) return rc;
    (void)snprintf(base, sizeof(base), "%s.ffn.shared_experts.w3", prefix);
    rc = fixture_fp8(f, base, 2048u, 4096u);
    if (rc != YVEX_OK) return rc;
    (void)snprintf(name, sizeof(name), "%s.ffn.gate.weight", prefix);
    rc = fixture_m(f, name, YVEX_NATIVE_DTYPE_BF16, 256u, 4096u);
    if (rc != YVEX_OK) return rc;
    if (hash_router) {
        (void)snprintf(name, sizeof(name), "%s.ffn.gate.tid2eid", prefix);
        return fixture_m(f, name, YVEX_NATIVE_DTYPE_I64, 129280u, 6u);
    }
    (void)snprintf(name, sizeof(name), "%s.ffn.gate.bias", prefix);
    (void)layer;
    return fixture_v(f, name, YVEX_NATIVE_DTYPE_F32, 256u);
}

static int fixture_globals(fixture_inventory *f)
{
    int rc = fixture_m(f, "embed.weight", YVEX_NATIVE_DTYPE_BF16,
                       129280u, 4096u);
    if (rc != YVEX_OK) return rc;
    rc = fixture_v(f, "norm.weight", YVEX_NATIVE_DTYPE_BF16, 4096u);
    if (rc != YVEX_OK) return rc;
    rc = fixture_m(f, "head.weight", YVEX_NATIVE_DTYPE_BF16,
                   129280u, 4096u);
    if (rc != YVEX_OK) return rc;
    rc = fixture_m(f, "hc_head_fn", YVEX_NATIVE_DTYPE_F32, 4u, 16384u);
    if (rc != YVEX_OK) return rc;
    rc = fixture_v(f, "hc_head_base", YVEX_NATIVE_DTYPE_F32, 4u);
    if (rc != YVEX_OK) return rc;
    return fixture_v(f, "hc_head_scale", YVEX_NATIVE_DTYPE_F32, 1u);
}

static int fixture_mtp(fixture_inventory *f)
{
    char name[256];
    int rc = fixture_layer(f, "mtp.0", 43u, 0u, 0);
    if (rc != YVEX_OK) return rc;
    rc = fixture_fp8(f, "mtp.0.e_proj", 4096u, 4096u);
    if (rc != YVEX_OK) return rc;
    rc = fixture_fp8(f, "mtp.0.h_proj", 4096u, 4096u);
    if (rc != YVEX_OK) return rc;
    rc = fixture_v(f, "mtp.0.enorm.weight", YVEX_NATIVE_DTYPE_BF16, 4096u);
    if (rc != YVEX_OK) return rc;
    rc = fixture_v(f, "mtp.0.hnorm.weight", YVEX_NATIVE_DTYPE_BF16, 4096u);
    if (rc != YVEX_OK) return rc;
    rc = fixture_v(f, "mtp.0.norm.weight", YVEX_NATIVE_DTYPE_BF16, 4096u);
    if (rc != YVEX_OK) return rc;
    (void)snprintf(name, sizeof(name), "%s", "mtp.0.hc_head_fn");
    rc = fixture_m(f, name, YVEX_NATIVE_DTYPE_F32, 4u, 16384u);
    if (rc != YVEX_OK) return rc;
    rc = fixture_v(f, "mtp.0.hc_head_base", YVEX_NATIVE_DTYPE_F32, 4u);
    if (rc != YVEX_OK) return rc;
    return fixture_v(f, "mtp.0.hc_head_scale", YVEX_NATIVE_DTYPE_F32, 1u);
}

static int fixture_snapshot(yvex_source_tensor_snapshot **out,
                            yvex_source_verification *verification,
                            fixture_mutation mutation,
                            int globals_last)
{
    fixture_inventory fixture;
    yvex_source_tensor_snapshot_facts facts;
    unsigned long long layer;
    char prefix[64];
    int rc;

    memset(&fixture, 0, sizeof(fixture));
    fixture.mutation = mutation;
    fixture.table = (yvex_native_weight_table *)calloc(1u,
                                                       sizeof(*fixture.table));
    if (!fixture.table) return YVEX_ERR_NOMEM;
    if (!globals_last && fixture_globals(&fixture) != YVEX_OK) goto fail;
    for (layer = 0u; layer < 43u; ++layer) {
        unsigned long long ratio = layer < 2u ? 0u
                                  : (layer % 2u == 0u ? 4u : 128u);
        (void)snprintf(prefix, sizeof(prefix), "layers.%llu", layer);
        if (fixture_layer(&fixture, prefix, layer, ratio, layer < 3u) !=
            YVEX_OK) goto fail;
    }
    if (fixture_mtp(&fixture) != YVEX_OK) goto fail;
    if (globals_last && fixture_globals(&fixture) != YVEX_OK) goto fail;
    if (mutation == FIXTURE_UNEXPECTED &&
        fixture_v(&fixture, "unexpected.weight", YVEX_NATIVE_DTYPE_F32, 1u) !=
            YVEX_OK) goto fail;
    if (mutation == FIXTURE_INVALID_LAYER &&
        fixture_v(&fixture, "layers.43.attn_norm.weight",
                  YVEX_NATIVE_DTYPE_BF16, 4096u) != YVEX_OK) goto fail;
    if (mutation == FIXTURE_INVALID_EXPERT &&
        fixture_m(&fixture, "layers.0.ffn.experts.256.w1.weight",
                  YVEX_NATIVE_DTYPE_I8, 2048u, 2048u) != YVEX_OK) goto fail;
    if (mutation == FIXTURE_INDEX_OVERFLOW &&
        fixture_v(&fixture,
                  "layers.184467440737095516160.attn_norm.weight",
                  YVEX_NATIVE_DTYPE_BF16, 4096u) != YVEX_OK) goto fail;
    rc = yvex_source_tensor_snapshot_take_table(
        out, &fixture.table, 46u, 1u, &fixture.err);
    if (rc != YVEX_OK) goto fail;
    YVEX_TEST_ASSERT(yvex_source_tensor_snapshot_facts_get(
                         *out, &facts, &fixture.err) == YVEX_OK,
                     "snapshot facts available");
    verification->header_tensor_count = facts.tensor_count;
    verification->indexed_tensor_count = facts.tensor_count;
    return YVEX_OK;
fail:
    yvex_native_weight_table_close(fixture.table);
    return yvex_error_code(&fixture.err) == YVEX_OK
               ? YVEX_ERR_FORMAT
               : yvex_error_code(&fixture.err);
}

static int fixture_build_case(
    fixture_mutation mutation,
    int globals_last,
    yvex_deepseek_tensor_coverage **coverage,
    yvex_deepseek_tensor_coverage_failure *failure)
{
    yvex_source_verification verification;
    yvex_source_tensor_snapshot *snapshot = NULL;
    yvex_deepseek_v4_ir *ir = NULL;
    yvex_deepseek_v4_ir_failure ir_failure;
    yvex_error err;
    int rc;

    fixture_verification(&verification);
    if (fixture_snapshot(&snapshot, &verification, mutation, globals_last) !=
        YVEX_OK) return YVEX_ERR_FORMAT;
    yvex_error_clear(&err);
    rc = yvex_deepseek_v4_ir_build(&ir, &verification, &ir_failure, &err);
    if (rc == YVEX_OK) {
        rc = yvex_deepseek_tensor_coverage_build(
            coverage, &verification, ir, snapshot, NULL, failure, &err);
    }
    yvex_deepseek_v4_ir_close(ir);
    yvex_source_tensor_snapshot_release(snapshot);
    return rc;
}

typedef struct {
    unsigned int calls;
    unsigned int fail_at;
    unsigned int live;
} fixture_allocator;

static void *fixture_allocate(size_t size, void *context)
{
    fixture_allocator *allocator = (fixture_allocator *)context;
    void *result;
    if (allocator->calls++ == allocator->fail_at) return NULL;
    result = malloc(size);
    if (result) allocator->live++;
    return result;
}

static void fixture_release(void *allocation, void *context)
{
    fixture_allocator *allocator = (fixture_allocator *)context;
    if (!allocation) return;
    free(allocation);
    allocator->live--;
}

static int test_valid_target_scale(void)
{
    yvex_deepseek_tensor_coverage *first = NULL;
    yvex_deepseek_tensor_coverage *shuffled = NULL;
    yvex_deepseek_tensor_coverage_failure failure;
    const yvex_deepseek_tensor_coverage_summary *summary;
    unsigned long long identity;

    YVEX_TEST_ASSERT(fixture_build_case(FIXTURE_VALID, 0, &first, &failure) ==
                         YVEX_OK && first,
                     "69,187-entry coverage fixture closes");
    summary = yvex_deepseek_tensor_coverage_summary_get(first);
    YVEX_TEST_ASSERT(summary->complete && summary->source_tensor_count == 69187u &&
                     summary->required_tensor_count == 69187u &&
                     summary->matched_tensor_count == 69187u &&
                     summary->missing_count == 0u &&
                     summary->unexpected_count == 0u,
                     "target-scale one-to-one summary is exact");
    YVEX_TEST_ASSERT(summary->collection_counts[
                         YVEX_DEEPSEEK_TENSOR_COLLECTION_ROUTED_EXPERT] ==
                         67584u &&
                     summary->collection_counts[
                         YVEX_DEEPSEEK_TENSOR_COLLECTION_ATTENTION] == 572u &&
                     summary->collection_counts[
                         YVEX_DEEPSEEK_TENSOR_COLLECTION_AUXILIARY] == 10u,
                     "major collection counts include main and MTP tensors");
    YVEX_TEST_ASSERT(summary->header_scan_count == 1u &&
                     summary->payload_bytes_read == 0u &&
                     summary->source_lookup_count < 3u * 69187u &&
                     summary->source_collision_count > 0u &&
                     summary->source_maximum_probe > 0u &&
                     summary->source_maximum_probe < 64u,
                     "indexed reconciliation is linear and payload-free");
    YVEX_TEST_ASSERT(yvex_deepseek_tensor_coverage_find(
                         first, "layers.2.attn.indexer.wq_b.scale") != NULL &&
                     yvex_deepseek_tensor_coverage_find(
                         first, "mtp.0.hc_head_fn") != NULL,
                     "CSA scale and MTP head are typed requirements");
    identity = summary->coverage_identity;
    YVEX_TEST_ASSERT(fixture_build_case(FIXTURE_VALID, 1, &shuffled, &failure) ==
                         YVEX_OK && shuffled,
                     "shuffled insertion order closes");
    YVEX_TEST_ASSERT(yvex_deepseek_tensor_coverage_summary_get(shuffled)->
                             coverage_identity == identity,
                     "coverage identity is insertion-order invariant");
    yvex_deepseek_tensor_coverage_close(shuffled);
    yvex_deepseek_tensor_coverage_close(first);
    return 0;
}

static int test_mutations(void)
{
    const struct {
        fixture_mutation mutation;
        yvex_deepseek_tensor_coverage_failure_code expected;
    } cases[] = {
        {FIXTURE_MISSING, YVEX_DEEPSEEK_COVERAGE_FAILURE_MISSING_REQUIREMENT},
        {FIXTURE_MISSING_SCALE,
         YVEX_DEEPSEEK_COVERAGE_FAILURE_SCALE_COMPANION},
        {FIXTURE_RANK, YVEX_DEEPSEEK_COVERAGE_FAILURE_RANK_MISMATCH},
        {FIXTURE_SHAPE, YVEX_DEEPSEEK_COVERAGE_FAILURE_SHAPE_MISMATCH},
        {FIXTURE_DTYPE, YVEX_DEEPSEEK_COVERAGE_FAILURE_DTYPE_MISMATCH},
        {FIXTURE_SCALE, YVEX_DEEPSEEK_COVERAGE_FAILURE_SHAPE_MISMATCH},
        {FIXTURE_UNEXPECTED,
         YVEX_DEEPSEEK_COVERAGE_FAILURE_UNEXPECTED_SOURCE},
        {FIXTURE_INVALID_LAYER,
         YVEX_DEEPSEEK_COVERAGE_FAILURE_INVALID_INDEX},
        {FIXTURE_INVALID_EXPERT,
         YVEX_DEEPSEEK_COVERAGE_FAILURE_INVALID_INDEX},
        {FIXTURE_INDEX_OVERFLOW,
         YVEX_DEEPSEEK_COVERAGE_FAILURE_ARITHMETIC_OVERFLOW}
    };
    size_t i;

    for (i = 0u; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        yvex_deepseek_tensor_coverage *coverage = NULL;
        yvex_deepseek_tensor_coverage_failure failure;
        YVEX_TEST_ASSERT(fixture_build_case(cases[i].mutation, 0, &coverage,
                                            &failure) != YVEX_OK && !coverage,
                         "mutated inventory refuses coverage");
        YVEX_TEST_ASSERT(failure.code == cases[i].expected,
                         "mutation returns stable typed reason");
    }
    return 0;
}

static int test_limits_and_allocation(void)
{
    yvex_source_verification verification;
    yvex_source_tensor_snapshot *snapshot = NULL;
    yvex_deepseek_v4_ir *ir = NULL;
    yvex_deepseek_v4_ir_failure ir_failure;
    yvex_deepseek_tensor_coverage_failure failure;
    yvex_deepseek_tensor_coverage *coverage = NULL;
    yvex_deepseek_tensor_coverage_options options;
    yvex_error err;
    unsigned int fail_at;

    fixture_verification(&verification);
    YVEX_TEST_ASSERT(fixture_snapshot(&snapshot, &verification,
                                      FIXTURE_VALID, 0) == YVEX_OK,
                     "allocation test snapshot builds");
    yvex_error_clear(&err);
    YVEX_TEST_ASSERT(yvex_deepseek_v4_ir_build(
                         &ir, &verification, &ir_failure, &err) == YVEX_OK,
                     "allocation test IR builds");
    fixture_copy(verification.inventory_authority,
                 sizeof(verification.inventory_authority), "header-derived");
    YVEX_TEST_ASSERT(yvex_deepseek_tensor_coverage_build(
                         &coverage, &verification, ir, snapshot, NULL,
                         &failure, &err) != YVEX_OK && !coverage &&
                     failure.code ==
                         YVEX_DEEPSEEK_COVERAGE_FAILURE_INVENTORY_AUTHORITY,
                     "non-pinned inventory authority refuses coverage");
    fixture_copy(verification.inventory_authority,
                 sizeof(verification.inventory_authority), "upstream-index");
    verification.header_tensor_count--;
    YVEX_TEST_ASSERT(yvex_deepseek_tensor_coverage_build(
                         &coverage, &verification, ir, snapshot, NULL,
                         &failure, &err) != YVEX_OK && !coverage &&
                     failure.code ==
                         YVEX_DEEPSEEK_COVERAGE_FAILURE_INVENTORY_DRIFT,
                     "snapshot and verification count drift refuses coverage");
    verification.header_tensor_count++;
    memset(&options, 0, sizeof(options));
    options.maximum_tensors = 69186u;
    YVEX_TEST_ASSERT(yvex_deepseek_tensor_coverage_build(
                         &coverage, &verification, ir, snapshot, &options,
                         &failure, &err) != YVEX_OK && !coverage &&
                     failure.code ==
                         YVEX_DEEPSEEK_COVERAGE_FAILURE_RESOURCE_LIMIT,
                     "resource limit fails before construction");
    for (fail_at = 0u; fail_at < 4u; ++fail_at) {
        fixture_allocator allocator;
        memset(&allocator, 0, sizeof(allocator));
        allocator.fail_at = fail_at;
        memset(&options, 0, sizeof(options));
        options.allocate = fixture_allocate;
        options.release = fixture_release;
        options.context = &allocator;
        YVEX_TEST_ASSERT(yvex_deepseek_tensor_coverage_build(
                             &coverage, &verification, ir, snapshot, &options,
                             &failure, &err) != YVEX_OK && !coverage,
                         "injected allocation failure refuses");
        YVEX_TEST_ASSERT(allocator.live == 0u,
                         "partial coverage allocation rolls back");
    }
    yvex_deepseek_v4_ir_close(ir);
    yvex_source_tensor_snapshot_release(snapshot);
    return 0;
}

int yvex_test_deepseek_tensor_coverage(void)
{
    if (test_valid_target_scale() != 0) return 1;
    if (test_mutations() != 0) return 1;
    if (test_limits_and_allocation() != 0) return 1;
    return 0;
}
