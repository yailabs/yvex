/*
 * YVEX - exact source verification tests.
 *
 * Exercises structured DeepSeek source verification with tiny metadata and
 * safetensors headers. No model payload fixture is used.
 */
#include "test.h"

#include "yvex_model_target_catalog.h"
#include "yvex_source_verify.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char source_verify_revision[] =
    "0123456789abcdef0123456789abcdef01234567";

static int source_verify_make_dir(const char *path)
{
    return mkdir(path, 0777) == 0 || errno == EEXIST;
}

static int source_verify_write_text(const char *path, const char *text)
{
    FILE *fp = fopen(path, "wb");

    if (!fp) return 0;
    if (fputs(text, fp) < 0) {
        fclose(fp);
        return 0;
    }
    return fclose(fp) == 0;
}

static int source_verify_write_safetensors(const char *path)
{
    static const char header[] =
        "{\"model.embed_tokens.weight\":{\"dtype\":\"BF16\","
        "\"shape\":[2,2],\"data_offsets\":[0,8]},"
        "\"model.scale\":{\"dtype\":\"F8_E8M0\","
        "\"shape\":[1],\"data_offsets\":[8,9]},"
        "\"model.values\":{\"dtype\":\"I8\","
        "\"shape\":[2],\"data_offsets\":[9,11]}}";
    unsigned long long length = (unsigned long long)strlen(header);
    unsigned char bytes[8];
    FILE *fp;
    unsigned int i;

    fp = fopen(path, "wb");
    if (!fp) return 0;
    for (i = 0; i < 8u; ++i) {
        bytes[i] = (unsigned char)((length >> (8u * i)) & 0xffu);
    }
    if (fwrite(bytes, 1u, sizeof(bytes), fp) != sizeof(bytes) ||
        fwrite(header, 1u, (size_t)length, fp) != (size_t)length ||
        fwrite("12345678901", 1u, 11u, fp) != 11u) {
        fclose(fp);
        return 0;
    }
    return fclose(fp) == 0;
}

static int source_verify_write_config(const char *root,
                                      const char *model_type,
                                      const char *architecture)
{
    char path[512];
    char json[4096];
    int n;

    n = snprintf(path, sizeof(path), "%s/config.json", root);
    if (n < 0 || (size_t)n >= sizeof(path)) return 0;
    n = snprintf(
        json, sizeof(json),
        "{\"architectures\":[\"%s\"],\"model_type\":\"%s\","
        "\"hidden_size\":4096,\"num_hidden_layers\":43,"
        "\"num_attention_heads\":64,\"num_key_value_heads\":1,"
        "\"head_dim\":512,\"qk_rope_head_dim\":64,"
        "\"max_position_embeddings\":1048576,"
        "\"moe_intermediate_size\":2048,\"n_routed_experts\":256,"
        "\"n_shared_experts\":1,\"num_experts_per_tok\":6,"
        "\"num_hash_layers\":3,\"q_lora_rank\":1024,"
        "\"o_lora_rank\":1024,\"vocab_size\":129280,"
        "\"sliding_window\":128,\"tie_word_embeddings\":false,"
        "\"torch_dtype\":\"bfloat16\",\"expert_dtype\":\"fp4\","
        "\"hidden_act\":\"silu\",\"attention_bias\":false,"
        "\"attention_dropout\":0.0,\"bos_token_id\":0,\"eos_token_id\":1,"
        "\"compress_ratios\":[0,0,4,128,4,128,4,128,4,128,4,128,4,128,4,128,4,128,4,128,4,128,4,128,4,128,4,128,4,128,4,128,4,128,4,128,4,128,4,128,4,128,4,0],"
        "\"compress_rope_theta\":160000,\"hc_eps\":0.000001,"
        "\"hc_mult\":4,\"hc_sinkhorn_iters\":20,"
        "\"index_head_dim\":128,\"index_n_heads\":64,\"index_topk\":512,"
        "\"num_nextn_predict_layers\":1,\"o_groups\":8,"
        "\"rms_norm_eps\":0.000001,\"rope_theta\":10000,"
        "\"routed_scaling_factor\":1.5,\"scoring_func\":\"sqrtsoftplus\","
        "\"topk_method\":\"noaux_tc\",\"norm_topk_prob\":true,"
        "\"swiglu_limit\":10.0,\"use_cache\":true,\"rope_scaling\":{"
        "\"type\":\"yarn\",\"factor\":16,"
        "\"original_max_position_embeddings\":65536},"
        "\"quantization_config\":{\"quant_method\":\"fp8\","
        "\"fmt\":\"e4m3\",\"weight_block_size\":[128,128]}}",
        architecture, model_type);
    return n >= 0 && (size_t)n < sizeof(json) &&
           source_verify_write_text(path, json);
}

static int source_verify_write_manifest(const char *root, const char *kind,
                                        const char *repo,
                                        const char *status,
                                        const char *revision)
{
    char path[512];
    char json[2048];
    char revision_json[256];
    int n;

    n = snprintf(path, sizeof(path), "%s/source-manifest.json", root);
    if (n < 0 || (size_t)n >= sizeof(path)) return 0;
    n = revision
            ? snprintf(revision_json, sizeof(revision_json),
                       ",\"revision\":\"%s\"", revision)
            : snprintf(revision_json, sizeof(revision_json), "%s", "");
    if (n < 0 || (size_t)n >= sizeof(revision_json)) return 0;
    n = snprintf(
        json, sizeof(json),
        "{\"schema\":\"yvex.source_manifest.v1\",\"status\":\"%s\","
        "\"source\":{\"kind\":\"%s\",\"repo\":\"%s\"%s},"
        "\"local\":{\"path\":\"%s\"}}",
        status,
        kind,
        repo,
        revision_json,
        root);
    return n >= 0 && (size_t)n < sizeof(json) &&
           source_verify_write_text(path, json);
}

static int source_verify_write_metadata(const char *root, const char *name)
{
    char path[768];
    char text[256];
    int n;

    n = snprintf(path, sizeof(path),
                 "%s/.cache/huggingface/download/%s.metadata", root, name);
    if (n < 0 || (size_t)n >= sizeof(path)) return 0;
    n = snprintf(text, sizeof(text), "%s\nblob-id\n0\n",
                 source_verify_revision);
    return n >= 0 && (size_t)n < sizeof(text) &&
           source_verify_write_text(path, text);
}

static int source_verify_make_valid(const char *root)
{
    char path[512];

    if (!source_verify_make_dir("build") ||
        !source_verify_make_dir("build/tests") ||
        !source_verify_make_dir(root)) return 0;
    snprintf(path, sizeof(path), "%s/.cache", root);
    if (!source_verify_make_dir(path)) return 0;
    snprintf(path, sizeof(path), "%s/.cache/huggingface", root);
    if (!source_verify_make_dir(path)) return 0;
    snprintf(path, sizeof(path), "%s/.cache/huggingface/download", root);
    if (!source_verify_make_dir(path)) return 0;
    if (!source_verify_write_manifest(root, "huggingface",
                                      yvex_deepseek_v4_upstream_repo_id,
                                      "complete", source_verify_revision) ||
        !source_verify_write_config(root,
                                    yvex_deepseek_v4_config_model_type,
                                    yvex_deepseek_v4_config_architecture)) return 0;
    snprintf(path, sizeof(path), "%s/tokenizer.json", root);
    if (!source_verify_write_text(path,
                                  "{\"version\":\"1.0\",\"added_tokens\":[],"
                                  "\"normalizer\":null,\"pre_tokenizer\":{},"
                                  "\"post_processor\":{},\"decoder\":{},"
                                  "\"model\":{\"type\":\"BPE\",\"vocab\":{}}}")) return 0;
    snprintf(path, sizeof(path), "%s/tokenizer_config.json", root);
    if (!source_verify_write_text(
            path,
            "{\"tokenizer_class\":\"PreTrainedTokenizerFast\","
            "\"model_max_length\":1048576,\"bos_token\":{},\"eos_token\":{}}")) return 0;
    snprintf(path, sizeof(path), "%s/generation_config.json", root);
    if (!source_verify_write_text(
            path,
            "{\"_from_model_config\":true,\"bos_token_id\":0,"
            "\"eos_token_id\":1,\"do_sample\":true,"
            "\"temperature\":1.0,\"top_p\":1.0,"
            "\"transformers_version\":\"4.46.3\"}")) return 0;
    snprintf(path, sizeof(path), "%s/model.safetensors.index.json", root);
    if (!source_verify_write_text(
            path,
            "{\"metadata\":{\"total_size\":11},\"weight_map\":{"
            "\"model.embed_tokens.weight\":\"model-00001-of-00001.safetensors\","
            "\"model.scale\":\"model-00001-of-00001.safetensors\","
            "\"model.values\":\"model-00001-of-00001.safetensors\"}}")) return 0;
    snprintf(path, sizeof(path), "%s/model-00001-of-00001.safetensors", root);
    if (!source_verify_write_safetensors(path)) return 0;
    return source_verify_write_metadata(root, "config.json") &&
           source_verify_write_metadata(root, "tokenizer.json") &&
           source_verify_write_metadata(root, "tokenizer_config.json") &&
           source_verify_write_metadata(root, "generation_config.json") &&
           source_verify_write_metadata(root, "model.safetensors.index.json") &&
           source_verify_write_metadata(root,
                                        "model-00001-of-00001.safetensors");
}

static int source_verify_has_blocker(const yvex_source_verification *result,
                                     const char *blocker)
{
    unsigned int i;

    for (i = 0; result && i < result->blocker_count; ++i) {
        if (strcmp(result->blockers[i], blocker) == 0) return 1;
    }
    return 0;
}

static int source_verify_run(const char *root,
                             yvex_source_verification *result,
                             yvex_error *err)
{
    yvex_source_verify_options options;

    memset(&options, 0, sizeof(options));
    options.identity = yvex_model_target_release_identity();
    options.source_path = root;
    options.models_root = "build/tests";
    return yvex_source_verify(&options, result, err);
}

int yvex_test_source_verify(void)
{
    const char *root = "build/tests/source-verify";
    yvex_source_verification result;
    yvex_error err;
    unsigned long long total;
    char path[512];
    int rc;

    system("rm -rf build/tests/source-verify");
    YVEX_TEST_ASSERT(source_verify_make_valid(root), "create valid source fixture");
    rc = source_verify_run(root, &result, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK && result.verified,
                     "valid exact source verifies");
    YVEX_TEST_ASSERT_STREQ(result.repository_id,
                           yvex_deepseek_v4_upstream_repo_id,
                           "repository identity matches");
    YVEX_TEST_ASSERT_STREQ(result.revision, source_verify_revision,
                           "exact revision matches");
    YVEX_TEST_ASSERT(result.shard_count == 1 &&
                     result.header_tensor_count == 3 &&
                     result.shard_index_headers_match,
                     "index and header inventory agree");
    YVEX_TEST_ASSERT(result.dtype_bf16_count == 1 &&
                     result.dtype_f8_e8m0_count == 1 &&
                     result.dtype_i8_count == 1 &&
                     result.dtype_other_count == 0,
                     "raw source dtype facts remain distinct");
    YVEX_TEST_ASSERT(result.generation_config_valid &&
                     result.generation_bos_token_id == 0 &&
                     result.generation_eos_token_id == 1,
                     "generation sidecar facts are verified");
    YVEX_TEST_ASSERT(result.compress_ratio_count == 44 &&
                     result.index_topk == 512 &&
                     strcmp(result.scoring_func, "sqrtsoftplus") == 0 &&
                     strcmp(result.tokenizer_model_type, "BPE") == 0,
                     "execution-affecting config and tokenizer facts are preserved");

    system("rm -rf build/tests/source-verify");
    YVEX_TEST_ASSERT(source_verify_make_valid(root), "recreate wrong repo fixture");
    YVEX_TEST_ASSERT(source_verify_write_manifest(root, "huggingface",
                                                  "wrong/repository",
                                                  "complete",
                                                  source_verify_revision),
                     "write wrong repo manifest");
    YVEX_TEST_ASSERT(source_verify_run(root, &result, &err) == YVEX_OK &&
                     !result.verified &&
                     source_verify_has_blocker(&result, "wrong-source-repository"),
                     "wrong repository is refused");

    YVEX_TEST_ASSERT(source_verify_write_manifest(
                         root, "local", yvex_deepseek_v4_upstream_repo_id,
                         "complete", source_verify_revision),
                     "write unsupported source kind manifest");
    YVEX_TEST_ASSERT(source_verify_run(root, &result, &err) == YVEX_OK &&
                     !result.verified &&
                     source_verify_has_blocker(&result,
                                               "unsupported-source-kind"),
                     "unsupported source kind is refused");

    YVEX_TEST_ASSERT(source_verify_write_manifest(
                         root, "huggingface",
                         yvex_deepseek_v4_upstream_repo_id,
                         "complete", NULL),
                     "write absent revision manifest");
    YVEX_TEST_ASSERT(source_verify_run(root, &result, &err) == YVEX_OK &&
                     !result.verified &&
                     source_verify_has_blocker(&result, "missing-source-revision"),
                     "absent revision is refused");
    YVEX_TEST_ASSERT(source_verify_write_manifest(
                         root, "huggingface",
                         yvex_deepseek_v4_upstream_repo_id,
                         "complete", "unknown"),
                     "write unverifiable revision manifest");
    YVEX_TEST_ASSERT(source_verify_run(root, &result, &err) == YVEX_OK &&
                     !result.verified &&
                     source_verify_has_blocker(
                         &result, "unverifiable-source-revision"),
                     "unknown revision is not promoted to verified provenance");
    YVEX_TEST_ASSERT(source_verify_write_manifest(
                         root, "huggingface",
                         yvex_deepseek_v4_upstream_repo_id,
                         "in-progress", source_verify_revision),
                     "write incomplete manifest status");
    YVEX_TEST_ASSERT(source_verify_run(root, &result, &err) == YVEX_OK &&
                     !result.verified &&
                     source_verify_has_blocker(
                         &result, "source-manifest-incomplete"),
                     "in-progress manifest is refused");

    system("rm -rf build/tests/source-verify");
    YVEX_TEST_ASSERT(source_verify_make_valid(root), "recreate wrong config fixture");
    YVEX_TEST_ASSERT(source_verify_write_config(root, "not_deepseek_v4",
                                                yvex_deepseek_v4_config_architecture),
                     "write wrong config identity");
    YVEX_TEST_ASSERT(source_verify_run(root, &result, &err) == YVEX_OK &&
                     source_verify_has_blocker(&result, "wrong-source-model-type"),
                     "wrong structured config identity is refused");
    snprintf(path, sizeof(path), "%s/config.json", root);
    YVEX_TEST_ASSERT(source_verify_write_text(path, "{"),
                     "write malformed config");
    YVEX_TEST_ASSERT(source_verify_run(root, &result, &err) == YVEX_OK &&
                     source_verify_has_blocker(&result, "malformed-source-config"),
                     "malformed config is refused");

    system("rm -rf build/tests/source-verify");
    YVEX_TEST_ASSERT(source_verify_make_valid(root), "recreate tokenizer fixture");
    snprintf(path, sizeof(path), "%s/tokenizer.json", root);
    YVEX_TEST_ASSERT(unlink(path) == 0, "remove tokenizer");
    YVEX_TEST_ASSERT(source_verify_run(root, &result, &err) == YVEX_OK &&
                     source_verify_has_blocker(&result, "missing-tokenizer-json"),
                     "missing tokenizer is refused");
    YVEX_TEST_ASSERT(source_verify_write_text(path, "{}"),
                     "write structurally incomplete tokenizer");
    YVEX_TEST_ASSERT(source_verify_run(root, &result, &err) == YVEX_OK &&
                     source_verify_has_blocker(
                         &result, "malformed-tokenizer-json"),
                     "tokenizer structure is validated, not only JSON syntax");

    system("rm -rf build/tests/source-verify");
    YVEX_TEST_ASSERT(source_verify_make_valid(root),
                     "recreate generation config fixture");
    snprintf(path, sizeof(path), "%s/generation_config.json", root);
    YVEX_TEST_ASSERT(unlink(path) == 0, "remove generation config");
    YVEX_TEST_ASSERT(source_verify_run(root, &result, &err) == YVEX_OK &&
                     source_verify_has_blocker(&result,
                                               "missing-generation-config"),
                     "missing generation config is refused");
    YVEX_TEST_ASSERT(source_verify_write_text(
                         path,
                         "{\"_from_model_config\":true,\"bos_token_id\":9,"
                         "\"eos_token_id\":1,\"do_sample\":true,"
                         "\"temperature\":1.0,\"top_p\":1.0,"
                         "\"transformers_version\":\"4.46.3\"}"),
                     "write inconsistent generation config");
    YVEX_TEST_ASSERT(source_verify_run(root, &result, &err) == YVEX_OK &&
                     source_verify_has_blocker(
                         &result, "generation-config-token-mismatch"),
                     "generation token identity must match model config");

    system("rm -rf build/tests/source-verify");
    YVEX_TEST_ASSERT(source_verify_make_valid(root), "recreate index fixture");
    snprintf(path, sizeof(path), "%s/model.safetensors.index.json", root);
    YVEX_TEST_ASSERT(source_verify_write_text(path, "{"),
                     "write malformed index");
    YVEX_TEST_ASSERT(source_verify_run(root, &result, &err) == YVEX_OK &&
                     source_verify_has_blocker(&result, "malformed-shard-index"),
                     "malformed shard index is refused");
    YVEX_TEST_ASSERT(source_verify_write_text(
                         path,
                         "{\"weight_map\":{\"model.embed_tokens.weight\":"
                         "\"model-00001-of-00001.safetensors\","
                         "\"model.embed_tokens.weight\":"
                         "\"model-00001-of-00001.safetensors\"}}"),
                     "write duplicate tensor index");
    YVEX_TEST_ASSERT(source_verify_run(root, &result, &err) == YVEX_OK &&
                     source_verify_has_blocker(&result,
                                               "duplicate-index-tensor"),
                     "duplicate index tensor is refused explicitly");
    YVEX_TEST_ASSERT(source_verify_write_text(
                         path,
                         "{\"metadata\":{\"total_size\":9},\"weight_map\":{"
                         "\"model.embed_tokens.weight\":"
                         "\"model-00001-of-00001.safetensors\"}}"),
                     "write inconsistent index size");
    YVEX_TEST_ASSERT(source_verify_run(root, &result, &err) == YVEX_OK &&
                     source_verify_has_blocker(&result,
                                               "shard-index-size-mismatch"),
                     "index total size must match header tensor spans");
    YVEX_TEST_ASSERT(source_verify_write_text(
                         path,
                         "{\"weight_map\":{\"model.embed_tokens.weight\":"
                         "\"model-00002-of-00002.safetensors\"}}"),
                     "write missing referenced shard index");
    YVEX_TEST_ASSERT(source_verify_run(root, &result, &err) == YVEX_OK &&
                     source_verify_has_blocker(&result, "missing-referenced-shard"),
                     "missing referenced shard is refused");

    system("rm -rf build/tests/source-verify");
    YVEX_TEST_ASSERT(source_verify_make_valid(root), "recreate unexpected shard fixture");
    snprintf(path, sizeof(path), "%s/weights.safetensors", root);
    YVEX_TEST_ASSERT(source_verify_write_safetensors(path),
                     "write unexpected shard");
    YVEX_TEST_ASSERT(source_verify_run(root, &result, &err) == YVEX_OK &&
                     source_verify_has_blocker(&result, "unexpected-shard"),
                     "unexpected shard is refused");
    snprintf(path, sizeof(path), "%s/model-00001-of-00002.safetensors", root);
    YVEX_TEST_ASSERT(source_verify_write_safetensors(path),
                     "write inconsistent shard series");
    YVEX_TEST_ASSERT(source_verify_run(root, &result, &err) == YVEX_OK &&
                     source_verify_has_blocker(&result,
                                               "inconsistent-shard-set") &&
                     source_verify_has_blocker(&result,
                                               "duplicate-source-shard"),
                     "inconsistent and duplicate shard numbering is refused");

    system("rm -rf build/tests/source-verify");
    YVEX_TEST_ASSERT(source_verify_make_valid(root), "recreate invalid header fixture");
    snprintf(path, sizeof(path), "%s/model-00001-of-00001.safetensors", root);
    YVEX_TEST_ASSERT(source_verify_write_text(path, "bad"),
                     "write invalid safetensors header");
    YVEX_TEST_ASSERT(source_verify_run(root, &result, &err) == YVEX_OK &&
                     source_verify_has_blocker(&result, "invalid-safetensors-header"),
                     "invalid safetensors header is refused");

    total = ULLONG_MAX - 1u;
    YVEX_TEST_ASSERT(!yvex_source_checked_add_u64(&total, 2u) &&
                     total == ULLONG_MAX - 1u,
                     "footprint overflow fails without mutation");
    YVEX_TEST_ASSERT(yvex_source_checked_add_u64(&total, 1u) &&
                     total == ULLONG_MAX,
                     "checked footprint addition accepts exact limit");
    return 0;
}
