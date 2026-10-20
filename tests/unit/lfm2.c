/* Scaled source topology controls; not complete-model inference evidence. */
#include "tests/test.h"
#include <yvex/internal/families/lfm2.h>
#include <stdlib.h>
#include <string.h>
#define YVEX_CHECK(condition) YVEX_TEST_ASSERT(condition, #condition)

int yvex_test_lfm2_acquired_source(void)
{
    const yvex_lfm2_api *api = yvex_model_register_lfm2();
    yvex_source_verify_options options = {
        .identity = yvex_source_target_identity_find(YVEX_LFM2_TARGET),
        .source_path = getenv("YVEX_LFM2_SOURCE"),
        .models_root = getenv("YVEX_LFM2_MODELS_ROOT"),
        .manifest_path = getenv("YVEX_LFM2_MANIFEST"),
        .promote_manifest = getenv("YVEX_LFM2_PROMOTE_SOURCE") != NULL};
    yvex_source_verification verification;
    yvex_source_tensor_snapshot *snapshot = NULL;
    yvex_lfm2_architecture architecture;
    yvex_lfm2_inventory inventory;
    yvex_error err;
    int rc;
    YVEX_CHECK(options.source_path && options.models_root && options.manifest_path);
    rc = yvex_source_verify_with_snapshot(&options, &verification, &snapshot, &err);
    if (rc == YVEX_OK && !verification.verified)
        for (unsigned int i = 0u; i < verification.blocker_count; ++i)
            fprintf(stderr, "LFM2 source blocker: %s\n", verification.blockers[i]);
    if (rc == YVEX_OK) rc = api->open(&verification, &architecture, &err);
    if (rc == YVEX_OK) rc = api->snapshot_audit(&architecture, snapshot, &inventory, &err);
    yvex_source_tensor_snapshot_release(snapshot);
    if (rc != YVEX_OK) fprintf(stderr, "LFM2 source: %s: %s\n",
        yvex_error_where(&err), yvex_error_message(&err));
    YVEX_CHECK(rc == YVEX_OK && verification.verified && architecture.complete && inventory.complete);
    YVEX_CHECK(architecture.layers == 40u && architecture.convolution_layers == 30u &&
        architecture.attention_layers == 10u && architecture.experts == 64u &&
        architecture.selected_experts == 4u && inventory.tensors == 7610u &&
        inventory.bf16_tensors == 7572u && inventory.f32_tensors == 38u &&
        inventory.bytes == 47943180288ull);
    printf("lfm2-source: revision=%s architecture=%s roles=%s layers=%llu conv=%llu attention=%llu "
           "tensors=%llu bytes=%llu manifest=%s payload_trusted=%d executable=false\n",
           YVEX_LFM2_REVISION, architecture.identity, inventory.identity, architecture.layers,
           architecture.convolution_layers, architecture.attention_layers, inventory.tensors,
           inventory.bytes, verification.manifest_schema, verification.manifest_payload_trusted);
    return 0;
}

static const char lfm_config[] =
    "{\"architectures\":[\"Lfm2MoeForCausalLM\"],\"model_type\":\"lfm2_moe\","
    "\"dtype\":\"bfloat16\",\"hidden_size\":4,\"num_hidden_layers\":3,\"vocab_size\":8,"
    "\"intermediate_size\":8,\"moe_intermediate_size\":6,\"num_attention_heads\":2,"
    "\"num_key_value_heads\":1,\"num_experts\":2,\"num_experts_per_tok\":1,"
    "\"num_dense_layers\":1,\"max_position_embeddings\":128,\"conv_L_cache\":3,"
    "\"bos_token_id\":1,\"eos_token_id\":2,\"pad_token_id\":0,\"norm_eps\":0.00001,"
    "\"routed_scaling_factor\":1.0,\"layer_types\":[\"conv\",\"full_attention\",\"conv\"],"
    "\"rope_parameters\":{\"rope_type\":\"default\",\"rope_theta\":10000000.0},"
    "\"conv_bias\":false,\"norm_topk_prob\":true,\"use_expert_bias\":true,"
    "\"tie_word_embeddings\":true,\"use_cache\":true}";

static int lfm_test_add(yvex_native_weight_table *table, const char *name,
                        const char *dtype, unsigned long long x, unsigned long long y,
                        unsigned long long z, unsigned long long *offset)
{
    unsigned long long shape[3] = {x, y, z}, bytes = x * (y ? y : 1u) * (z ? z : 1u) *
        (!strcmp(dtype, "F32") ? 4u : 2u);
    yvex_error err;
    int rc = yvex_native_weight_table_add(table, name, "fixture.safetensors", dtype,
        z ? 3u : y ? 2u : 1u, shape, *offset, *offset + bytes, &err);
    *offset += bytes;
    return rc == YVEX_OK;
}

static int lfm_test_inventory(yvex_native_weight_table **out)
{
    static const char *const conv[] = {"conv.conv.weight", "conv.in_proj.weight", "conv.out_proj.weight"};
    static const char *const attention[] = {"self_attn.q_proj.weight", "self_attn.k_proj.weight",
        "self_attn.v_proj.weight", "self_attn.out_proj.weight", "self_attn.q_layernorm.weight",
        "self_attn.k_layernorm.weight"};
    static const char *const dense[] = {"w1.weight", "w2.weight", "w3.weight"};
    char name[128];
    unsigned long long offset = 0u;
    *out = calloc(1u, sizeof(**out));
    if (!*out || !lfm_test_add(*out, "model.embed_tokens.weight", "BF16", 8u, 4u, 0u, &offset) ||
        !lfm_test_add(*out, "model.embedding_norm.weight", "BF16", 4u, 0u, 0u, &offset)) return 0;
    for (unsigned int l = 0u; l < 3u; ++l) {
        const unsigned long long h = 4u;
        snprintf(name, sizeof(name), "model.layers.%u.operator_norm.weight", l);
        if (!lfm_test_add(*out, name, "BF16", h, 0u, 0u, &offset)) return 0;
        snprintf(name, sizeof(name), "model.layers.%u.ffn_norm.weight", l);
        if (!lfm_test_add(*out, name, "BF16", h, 0u, 0u, &offset)) return 0;
        for (unsigned int i = 0u; i < (l == 1u ? 6u : 3u); ++i) {
            unsigned long long x, y, z;
            if (l == 1u) {
                x = i == 1u || i == 2u || i >= 4u ? 2u : 4u;
                y = i < 4u ? 4u : 0u; z = 0u;
            } else {
                x = i == 1u ? 12u : 4u; y = i == 0u ? 1u : 4u; z = i == 0u ? 3u : 0u;
            }
            snprintf(name, sizeof(name), "model.layers.%u.%s", l, l == 1u ? attention[i] : conv[i]);
            if (!lfm_test_add(*out, name, "BF16", x, y, z, &offset)) return 0;
        }
        if (l) {
            snprintf(name, sizeof(name), "model.layers.%u.feed_forward.gate.weight", l);
            if (!lfm_test_add(*out, name, "BF16", 2u, 4u, 0u, &offset)) return 0;
            snprintf(name, sizeof(name), "model.layers.%u.feed_forward.expert_bias", l);
            if (!lfm_test_add(*out, name, "F32", 2u, 0u, 0u, &offset)) return 0;
        }
        for (unsigned int e = 0u; e < (l ? 2u : 1u); ++e)
            for (unsigned int i = 0u; i < 3u; ++i) {
                unsigned long long mid = l ? 6u : 8u;
                if (l) snprintf(name, sizeof(name), "model.layers.%u.feed_forward.experts.%u.%s", l, e, dense[i]);
                else snprintf(name, sizeof(name), "model.layers.%u.feed_forward.%s", l, dense[i]);
                if (!lfm_test_add(*out, name, "BF16", i == 1u ? h : mid, i == 1u ? mid : h, 0u, &offset))
                    return 0;
            }
    }
    {
        yvex_error err;
        return yvex_native_weight_table_finalize(*out, &err) == YVEX_OK;
    }
}

static int lfm_mutated_config(const char *from, const char *to)
{
    char changed[4096];
    const char *at = strstr(lfm_config, from);
    yvex_lfm2_architecture a;
    yvex_error err;
    size_t prefix;
    if (!at) return 0;
    prefix = (size_t)(at - lfm_config);
    memcpy(changed, lfm_config, prefix);
    snprintf(changed + prefix, sizeof(changed) - prefix, "%s%s", to, at + strlen(from));
    return yvex_model_register_lfm2()->config_parse(changed, strlen(changed), &a, &err) != YVEX_OK &&
        !a.complete && !a.identity[0];
}

int yvex_test_lfm2_source(void)
{
    const yvex_lfm2_api *api = yvex_model_register_lfm2();
    yvex_lfm2_architecture a;
    yvex_lfm2_inventory inventory;
    yvex_native_weight_table *table = NULL;
    yvex_lfm2_tensor_binding b;
    yvex_error err;
    YVEX_CHECK(!strcmp(yvex_source_release_identity()->target_id, YVEX_SOURCE_RELEASE_TARGET_ID) &&
        yvex_source_is_release_target(YVEX_SOURCE_RELEASE_TARGET_ID) &&
        !yvex_source_is_release_target(YVEX_LFM2_TARGET));
    YVEX_CHECK(api->config_parse(lfm_config, strlen(lfm_config), &a, &err) == YVEX_OK);
    YVEX_CHECK(a.complete && a.head_width == 2u && a.required_tensors == 39u);
    YVEX_CHECK(a.convolution_layers == 2u && a.attention_layers == 1u);
    YVEX_CHECK(lfm_mutated_config("\"conv_bias\":false", "\"conv_bias\":true"));
    YVEX_CHECK(lfm_mutated_config("\"use_expert_bias\":true", "\"use_expert_bias\":false"));
    YVEX_CHECK(lfm_mutated_config("\"norm_topk_prob\":true", "\"norm_topk_prob\":false"));
    YVEX_CHECK(lfm_mutated_config("\"tie_word_embeddings\":true", "\"tie_word_embeddings\":false"));
    YVEX_CHECK(lfm_mutated_config("\"num_experts_per_tok\":1", "\"num_experts_per_tok\":3"));
    YVEX_CHECK(lfm_mutated_config("\"num_key_value_heads\":1", "\"num_key_value_heads\":3"));
    YVEX_CHECK(lfm_mutated_config("\"conv_L_cache\":3", "\"conv_L_cache\":0"));
    YVEX_CHECK(lfm_mutated_config("\"hidden_size\":4", "\"hidden_size\":4,\"hidden_size\":4"));
    YVEX_CHECK(lfm_mutated_config("\"rope_type\":\"default\"", "\"rope_type\":\"linear\""));
    YVEX_CHECK(lfm_mutated_config("\"conv\",\"full_attention\",\"conv\"", "\"conv\",\"full_attention\""));
    YVEX_CHECK(lfm_mutated_config("\"conv\",\"full_attention\",\"conv\"", "\"conv\",\"gated_delta\",\"conv\""));
    YVEX_CHECK(lfm_mutated_config("\"use_cache\":true", "\"use_cache\":true,\"attention_bias\":true"));
    YVEX_CHECK(lfm_mutated_config("\"norm_eps\":0.00001", "\"norm_eps\":-1.0"));
    YVEX_CHECK(lfm_test_inventory(&table));
    YVEX_CHECK(api->tensor_audit(&a, table, &inventory, &err) == YVEX_OK);
    YVEX_CHECK(inventory.complete && inventory.tensors == 39u && inventory.f32_tensors == 2u);
    for (unsigned long long i = 0u; i < yvex_native_weight_table_count(table); ++i) {
        const yvex_native_weight_info *original = yvex_native_weight_table_at(table, i);
        yvex_native_weight_info changed = *original;
        YVEX_CHECK(api->tensor_classify(&a, original, &b, &err) == YVEX_OK);
        changed.data_bytes++;
        YVEX_CHECK(api->tensor_classify(&a, &changed, &b, &err) == YVEX_ERR_FORMAT);
        changed = *original;
        changed.dtype = original->dtype == YVEX_NATIVE_DTYPE_F32 ? YVEX_NATIVE_DTYPE_BF16 : YVEX_NATIVE_DTYPE_F32;
        YVEX_CHECK(api->tensor_classify(&a, &changed, &b, &err) == YVEX_ERR_FORMAT);
    }
    {
        yvex_native_weight_info changed = *yvex_native_weight_table_at(table, 0u);
        changed.name = "lm_head.weight";
        YVEX_CHECK(api->tensor_classify(&a, &changed, &b, &err) == YVEX_ERR_FORMAT);
        changed.name = "model.layers.01.operator_norm.weight";
        YVEX_CHECK(api->tensor_classify(&a, &changed, &b, &err) == YVEX_ERR_FORMAT);
        changed.name = "model.layers.18446744073709551616.operator_norm.weight";
        YVEX_CHECK(api->tensor_classify(&a, &changed, &b, &err) == YVEX_ERR_FORMAT);
    }
    yvex_native_weight_table_close(table);
    return 0;
}
