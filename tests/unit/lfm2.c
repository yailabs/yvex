/* Scaled source topology controls; not complete-model inference evidence. */
#include "tests/test.h"
#include <yvex/internal/families/lfm2.h>
#include <yvex/internal/program_stage.h>
#include <yvex/internal/quant_numeric.h>
#include <yvex/internal/neural_operations.h>
#include <yvex/internal/artifact_lowering.h>
#include <yvex/internal/compilation.h>
#include <yvex/internal/compiler_source.h>
#include <yvex/internal/family_catalog.h>
#include <yvex/internal/runtime.h>
#include <yvex/internal/tokenizer.h>
#include <yvex/internal/model.h>
#include <yvex/qtype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define YVEX_CHECK(condition) YVEX_TEST_ASSERT(condition, #condition)

static int lfm_tokenizer_vector(FILE *file, const yvex_tokenizer *tokenizer)
{
    size_t bytes, count, decoded_count;
    unsigned char text[8192], decoded[8192];
    unsigned int ids[8192], value;
    yvex_error err;
    YVEX_CHECK(fscanf(file, "%zu %zu %zu", &bytes, &count, &decoded_count) == 3 &&
        bytes <= sizeof(text) && count <= sizeof(ids) / sizeof(*ids) && decoded_count <= sizeof(decoded));
    for (size_t i = 0u; i < bytes; ++i) {
        YVEX_CHECK(fscanf(file, "%2x", &value) == 1); text[i] = (unsigned char)value;
    }
    for (size_t i = 0u; i < count; ++i) YVEX_CHECK(fscanf(file, "%u", ids + i) == 1 && ids[i] < 125017u);
    for (size_t i = 0u; i < decoded_count; ++i) {
        YVEX_CHECK(fscanf(file, "%2x", &value) == 1); decoded[i] = (unsigned char)value;
    }
    yvex_tokenizer_encode_options options = {0, 0, 1, 8192u};
    yvex_tokenizer_encode_result encoded = {0};
    int rc = yvex_tokenizer_encode(tokenizer, text, bytes, &options, &encoded, &err);
    if (rc != YVEX_OK) fprintf(stderr, "Liquid tokenizer encode: %s\n", err.message);
    if (rc == YVEX_OK && (encoded.tokens.len != count ||
            (count && memcmp(encoded.tokens.ids, ids, count * sizeof(*ids))))) {
        fprintf(stderr, "Liquid public tokenizer vector: expected=%zu actual=%llu; IDs", count, encoded.tokens.len);
        for (size_t i = 0u; i < encoded.tokens.len; ++i) fprintf(stderr, " %u", encoded.tokens.ids[i]);
        fputc('\n', stderr);
    }
    YVEX_CHECK(rc == YVEX_OK && encoded.completed && encoded.tokens.len == count &&
        (!count || !memcmp(encoded.tokens.ids, ids, count * sizeof(*ids))));
    yvex_tokenizer_decode_options decode_options = {0};
    yvex_tokenizer_decode_result result = {0};
    rc = yvex_tokenizer_decode(tokenizer, ids, count, &decode_options, &result, &err);
    if (rc != YVEX_OK) fprintf(stderr, "Liquid tokenizer decode: %s\n", err.message);
    YVEX_CHECK(rc == YVEX_OK && result.completed && result.byte_count == decoded_count &&
        (!decoded_count || !memcmp(result.bytes, decoded, decoded_count)));
    yvex_tokenizer_encode_result_clear(&encoded); yvex_tokenizer_decode_result_clear(&result);
    return 0;
}

static int lfm_tokenizer_literal_controls(const yvex_tokenizer *tokenizer)
{
    const unsigned char text[] = "<|im_end|><|pad|><|startoftext|>";
    yvex_tokenizer_encode_options options = {0, 0, 0, 128u};
    yvex_tokenizer_encode_result encoded = {0};
    yvex_tokenizer_decode_options decode_options = {0};
    yvex_tokenizer_decode_result decoded = {0};
    yvex_error err;
    YVEX_CHECK(yvex_tokenizer_encode(tokenizer, text, sizeof(text) - 1u,
        &options, &encoded, &err) == YVEX_OK && encoded.completed);
    for (size_t i = 0u; i < encoded.tokens.len; ++i)
        YVEX_CHECK(encoded.tokens.ids[i] < 124893u);
    YVEX_CHECK(yvex_tokenizer_decode(tokenizer, encoded.tokens.ids, encoded.tokens.len,
        &decode_options, &decoded, &err) == YVEX_OK && decoded.completed &&
        decoded.byte_count == sizeof(text) - 1u && !memcmp(decoded.bytes, text, sizeof(text) - 1u));
    yvex_tokenizer_encode_result_clear(&encoded); yvex_tokenizer_decode_result_clear(&decoded);
    return 0;
}

int yvex_test_lfm2_tokenizer_reference(void)
{
    const char *fixture = getenv("YVEX_LFM2_TOKENIZER_ORACLE"), *artifact = getenv("YVEX_LFM2_ARTIFACT");
    const char *binding_path = getenv("YVEX_LFM2_BINDING");
    FILE *file = fixture ? fopen(fixture, "r") : NULL;
    char schema[64], identity[65], trailing;
    size_t count;
    yvex_model_context context = {0};
    yvex_runtime_binding *binding = NULL;
    yvex_runtime_binding_summary binding_summary;
    yvex_complete_artifact_admission admission;
    yvex_runtime_binding_failure failure;
    yvex_error err;
    YVEX_CHECK(file && artifact && binding_path && fscanf(file, "%63s %64s %zu", schema, identity, &count) == 3 &&
        !strcmp(schema, "yvex.lfm2.tokenizer.reference.v1") && count >= 15u && count <= 1024u &&
        !strcmp(identity, "695be7802a0e4b8a81048f0ff5ebb7fc811a0ba5a6be63dbb24deb5a81096f41"));
    YVEX_CHECK(yvex_model_context_open(artifact, &context, &err) == YVEX_OK);
    YVEX_CHECK(yvex_runtime_binding_open(&binding, binding_path, &binding_summary, &admission, &failure, &err) == YVEX_OK);
    YVEX_CHECK(!strcmp(binding_summary.artifact_identity, "ffd820b1614f994c4e146c49f5e29278029623ef86a90f67efee95f7bcd7612c"));
    int rc = yvex_tokenizer_from_compiled_gguf(&context.tokenizer, context.gguf,
        yvex_runtime_binding_tokenizer_policy(binding), &err);
    if (rc != YVEX_OK) fprintf(stderr, "Liquid tokenizer admission: %s\n", err.message);
    YVEX_CHECK(rc == YVEX_OK);
    YVEX_CHECK(yvex_tokenizer_bind_runtime(context.tokenizer, binding_summary.artifact_identity,
        binding_summary.logical_model_identity, binding_summary.runtime_descriptor_identity, &err) == YVEX_OK);
    const yvex_tokenizer_plan_summary *plan = yvex_tokenizer_plan_summary_get(context.tokenizer);
    YVEX_CHECK(plan && plan->runtime_bound && plan->vocabulary_size == 125017u &&
        !strcmp(plan->tokenizer_json_identity, identity));
    for (size_t i = 0u; i < count; ++i) {
        printf("Liquid tokenizer vector %zu\n", i);
        YVEX_CHECK(lfm_tokenizer_vector(file, context.tokenizer) == 0);
    }
    YVEX_CHECK(lfm_tokenizer_literal_controls(context.tokenizer) == 0);
    YVEX_CHECK(fscanf(file, " %c", &trailing) == EOF && fclose(file) == 0);
    yvex_runtime_binding_close(binding); yvex_model_context_close(&context);
    printf("Liquid tokenizer: %zu exact source encode/decode vectors; no chat-template or model inference claim\n", count);
    return 0;
}

static int lfm_cancel(void *context) { return ++*(unsigned int *)context > 1u; }

int yvex_test_lfm2_moe_reference(void)
{
    const char *path = getenv("YVEX_LFM2_MOE_ORACLE");
    unsigned long long n, h, m, e, k;
    yvex_error err;
    FILE *file = path ? fopen(path, "r") : NULL;
    YVEX_CHECK(file && fscanf(file, "%llu %llu %llu %llu %llu", &n, &h, &m, &e, &k) == 5 &&
        n > 0u && n <= 64u && h > 0u && h <= 64u && m > 0u && m <= 64u &&
        e >= 2u && e <= 64u && k > 0u && k < e);
    size_t counts[] = {n * h, n * e, e, e * m * h, e * m * h, e * h * m, n * h};
    float *data[7];
    for (size_t a = 0u; a < 7u; ++a) {
        data[a] = calloc(counts[a], sizeof(float));
        YVEX_CHECK(data[a]);
        for (size_t i = 0u; i < counts[a]; ++i)
            YVEX_CHECK(fscanf(file, "%f", &data[a][i]) == 1 && isfinite(data[a][i]));
    }
    char trailing;
    YVEX_CHECK(fscanf(file, " %c", &trailing) == EOF && fclose(file) == 0);
    unsigned short *encoded[3];
    yvex_component_encoded_weight bank[3];
    for (size_t a = 0u; a < 3u; ++a) {
        encoded[a] = calloc(counts[a + 3u], sizeof(unsigned short));
        YVEX_CHECK(encoded[a]);
        for (size_t i = 0u; i < counts[a + 3u]; ++i) {
            encoded[a][i] = yvex_quant_bf16_encode(data[a + 3u][i]);
            YVEX_CHECK(yvex_quant_bf16_decode(encoded[a][i]) == data[a + 3u][i]);
        }
        bank[a] = (yvex_component_encoded_weight){.encoded = (const unsigned char *)encoded[a],
            .encoded_bytes = counts[a + 3u] * 2u, .row_count = e * (a == 2u ? h : m),
            .row_width = a == 2u ? m : h, .row_bytes = 2u * (a == 2u ? m : h),
            .qtype = YVEX_GGUF_QTYPE_BF16};
    }
    float *output = calloc(n * h, sizeof(float)), *workspace = calloc(2u * e + m, sizeof(float));
    YVEX_CHECK(output && workspace);
    yvex_routed_swiglu_bf16_request request = {.rows = n, .hidden = h, .intermediate = m,
        .experts = e, .selected = k, .normalization_epsilon = 1e-6f, .routed_scale = 1.0f,
        .input = data[0], .logits = data[1], .bias = data[2],
        .gate = &bank[0], .up = &bank[1], .down = &bank[2], .output = output,
        .workspace = workspace, .workspace_values = 2u * e + m};
    int rc = yvex_routed_swiglu_bf16(&request, &err);
    if (rc != YVEX_OK) fprintf(stderr, "Liquid routed oracle: %s\n", err.message);
    YVEX_CHECK(rc == YVEX_OK);
    size_t different = 0u;
    float max_abs = 0.0f;
    for (size_t i = 0u; i < n * h; ++i) {
        different += output[i] != data[6][i];
        max_abs = fmaxf(max_abs, fabsf(output[i] - data[6][i]));
    }
    printf("Liquid routed oracle: outputs=%llu different=%zu max_abs=%g\n", n * h, different, max_abs);
    YVEX_CHECK(different == 0u);
    for (size_t row = 0u; row < n; ++row) {
        yvex_routed_swiglu_bf16_request single = request;
        single.rows = 1u; single.input += row * h; single.logits += row * e; single.output += row * h;
        YVEX_CHECK(yvex_routed_swiglu_bf16(&single, &err) == YVEX_OK);
    }
    YVEX_CHECK(!memcmp(output, data[6], n * h * sizeof(float)));
    float saved = data[0][0];
    data[0][0] = NAN;
    YVEX_CHECK(yvex_routed_swiglu_bf16(&request, &err) == YVEX_ERR_FORMAT);
    data[0][0] = saved;
    request.output = data[0];
    YVEX_CHECK(yvex_routed_swiglu_bf16(&request, &err) == YVEX_ERR_FORMAT);
    request.output = output;
    request.workspace_values--;
    YVEX_CHECK(yvex_routed_swiglu_bf16(&request, &err) == YVEX_ERR_FORMAT);
    request.workspace_values++;
    unsigned int cancelled = 0u;
    request.cancel_requested = lfm_cancel; request.cancel_context = &cancelled;
    YVEX_CHECK(yvex_routed_swiglu_bf16(&request, &err) == YVEX_ERR_CANCELLED);
    request.cancel_requested = NULL;
    YVEX_CHECK(yvex_routed_swiglu_bf16(&request, &err) == YVEX_OK &&
        !memcmp(output, data[6], n * h * sizeof(float)));
    memset(data[1], 0, n * e * sizeof(float)); memset(data[2], 0, e * sizeof(float));
    YVEX_CHECK(yvex_routed_swiglu_bf16(&request, &err) == YVEX_ERR_FORMAT &&
        strstr(err.message, "ambiguous top-k"));
    for (size_t a = 0u; a < 7u; ++a) free(data[a]);
    for (size_t a = 0u; a < 3u; ++a) free(encoded[a]);
    free(output); free(workspace);
    return 0;
}

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
    yvex_transform_ir *transform = NULL;
    yvex_artifact_lowering_map *lowering = NULL;
    yvex_compilation_source_failure failure;
    yvex_ir_module *program = NULL;
    if (rc == YVEX_OK) rc = yvex_lfm2_source_lower(&transform, &lowering,
        &verification, snapshot, &failure, &err);
    if (rc == YVEX_OK) rc = api->program_build(&program, &architecture,
        verification.manifest_payload_identity, &err);
    if (rc == YVEX_OK) {
        const yvex_transform_ir_summary *t = yvex_transform_ir_summary_get(transform);
        const yvex_artifact_lowering_summary *l = yvex_artifact_lowering_operations.summary(lowering);
        YVEX_CHECK(t && l && t->complete && l->complete && l->descriptor_count == 428u &&
            l->source_contribution_count == 7610u);
        printf("Liquid parameter recipe: sources=%llu terminals=%llu mapping=%llu transform=%s; "
            "sealed whole topology; payload not emitted\n", l->source_contribution_count,
            l->descriptor_count, l->mapping_identity, t->transform_identity);
    }
    yvex_ir_module_close(&program);
    yvex_artifact_lowering_operations.close(lowering);
    yvex_transform_ir_release(&transform);
    yvex_source_tensor_snapshot_release(snapshot);
    yvex_family_source_products products = {0};
    yvex_compilation_runtime_binding_request request = {.source_path = options.source_path,
        .models_root = options.models_root, .source_manifest_path = options.manifest_path};
    if (rc == YVEX_OK) rc = yvex_family_source_compile(YVEX_LFM2_TARGET, &request, &products, &err);
    if (rc == YVEX_OK) {
        const yvex_semantic_model_ir_summary *s = yvex_semantic_model_ir_summary_get(products.semantic_model);
        YVEX_CHECK(s && s->execution_descriptor.schema_version == YVEX_MODEL_EXECUTION_DESCRIPTOR_SCHEMA_V3 &&
            s->attention_layer_count == 10u && !s->decoder_layer_count &&
            s->execution_descriptor.sequence_mixer_layers == 30u &&
            s->execution_descriptor.routed_experts == 64u && !s->execution_descriptor.activation_limit);
        printf("Liquid canonical source products: semantic=%s parameters=%llu metadata-only=true\n",
            s->identity, products.source_summary->descriptor_count);
        unsigned char wire[YVEX_MODEL_EXECUTION_WIRE_BYTES];
        yvex_model_execution_descriptor roundtrip;
        YVEX_CHECK(yvex_model_execution_descriptor_encode(&s->execution_descriptor, wire, &err) == YVEX_OK &&
            yvex_model_execution_descriptor_decode(wire, sizeof(wire), &roundtrip, &err) == YVEX_OK &&
            !strcmp(roundtrip.identity, s->execution_descriptor.identity));
        wire[0] = YVEX_MODEL_EXECUTION_DESCRIPTOR_SCHEMA_V2;
        YVEX_CHECK(yvex_model_execution_descriptor_decode(wire, sizeof(wire), &roundtrip, &err) != YVEX_OK);
        wire[0] = 99u;
        YVEX_CHECK(yvex_model_execution_descriptor_decode(wire, sizeof(wire), &roundtrip, &err) != YVEX_OK);
    }
    yvex_family_source_products_release(&products);
    yvex_physical_variant_session *variant = NULL;
    const yvex_graph_execution_binding *family = yvex_graph_execution_find(
        YVEX_LFM2_ADAPTER_ID, YVEX_LFM2_ADAPTER_VERSION, YVEX_LFM2_TARGET);
    YVEX_CHECK(family && family->compiler && family->compiler->physical_variant);
    const yvex_physical_variant_api *variants = family->compiler->physical_variant();
    yvex_physical_variant_request vr = {.target_id = YVEX_LFM2_TARGET,
        .source_path = options.source_path, .models_root = options.models_root,
        .source_manifest_path = options.manifest_path, .worker_count = 1u,
        .quant_preset_name = "lfm2.5-24b-a2b-source-faithful-v1", .backend = "cpu"};
    if (rc == YVEX_OK) rc = variants->open(&variant, &vr, &err);
    if (rc == YVEX_OK) {
        const yvex_physical_variant_view *view = variants->view(variant);
        const yvex_quant_plan_summary *q = view ? yvex_quant_plan_summary_get(view->plan) : NULL;
        YVEX_CHECK(q && q->complete && q->decision_count == 428u && view->writer);
        printf("Liquid canonical physical plan: decisions=%llu; source-faithful writer ready; no emission\n",
            q->decision_count);
    }
    variants->close(&variant);
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

static int lfm_norm_control(void)
{
    const char *identity = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    for (unsigned int grouped = 0u; grouped < 2u; ++grouped) {
        yvex_ir_module *m = NULL;
        yvex_program_execution *execution = NULL;
        yvex_program_physical *physical = NULL, *copy = NULL;
        yvex_program_stage *stage = NULL;
        yvex_backend *backend = NULL;
        yvex_error err;
        yvex_ir_dialect dialects[] = {*yvex_ir_core_dialect(), *yvex_ir_neural_dialect()};
        yvex_ir_dimension dimension = {.name = "rows", .minimum = 1u, .maximum = 2u, .multiple = 1u};
        yvex_ir_type x = {.kind = YVEX_IR_TENSOR, .scalar = YVEX_IR_BF16, .rank = 2u,
            .shape = {{YVEX_IR_NONE, 0u}, {YVEX_IR_NONE, grouped ? 8u : 4u}}};
        yvex_ir_type w = {.kind = YVEX_IR_TENSOR, .scalar = YVEX_IR_BF16, .rank = 1u,
            .shape = {{YVEX_IR_NONE, 4u}}};
        yvex_ir_id xt, wt, function, op, parameter, args[2], output;
        int rc = yvex_ir_module_open(&m, "rounded_norm", identity, dialects, 2u, &err);
        if (rc == YVEX_OK) rc = yvex_ir_dimension_add(m, &dimension, &x.shape[0].symbol, &err);
        if (rc == YVEX_OK) rc = yvex_ir_type_intern(m, &x, &xt, &err);
        if (rc == YVEX_OK) rc = yvex_ir_type_intern(m, &w, &wt, &err);
        if (rc == YVEX_OK) rc = yvex_ir_function_add(m, "forward", &xt, 1u, &xt, 1u, 0u, &function, &err);
        YVEX_CHECK(rc == YVEX_OK);
        yvex_ir_id block = yvex_ir_function_at(m, function)->body;
        yvex_ir_attribute pa[] = {{.name = "source", .kind = YVEX_IR_ATTR_TEXT},
            {.name = "parameter", .kind = YVEX_IR_ATTR_SYMBOL, .value.text = "norm"}};
        snprintf(pa[0].value.text, sizeof(pa[0].value.text), "%s", identity);
        yvex_ir_operation_request request = {.operation = "core.parameter", .result_types = &wt,
            .result_count = 1u, .attributes = pa, .attribute_count = 2u};
        YVEX_CHECK(yvex_ir_operation_add(m, block, &request, &op, &err) == YVEX_OK);
        parameter = yvex_ir_operation_at(m, op)->results[0];
        args[0] = yvex_ir_block_at(m, block)->arguments[0]; args[1] = parameter;
        yvex_ir_attribute attrs[] = {{.name = "epsilon", .kind = YVEX_IR_ATTR_F64, .value.real = 1e-5},
            {.name = "weight_offset", .kind = YVEX_IR_ATTR_F64, .value.real = 0.0}};
        request = (yvex_ir_operation_request){.operation = grouped ? "nn.group_rms_norm" : "nn.rms_norm",
            .version = 2u, .operands = args, .operand_count = 2u, .result_types = &xt, .result_count = 1u,
            .attributes = attrs, .attribute_count = grouped ? 1u : 2u};
        YVEX_CHECK(yvex_ir_operation_add(m, block, &request, &op, &err) == YVEX_OK);
        output = yvex_ir_operation_at(m, op)->results[0];
        request = (yvex_ir_operation_request){.operation = "core.return", .operands = &output, .operand_count = 1u};
        YVEX_CHECK(yvex_ir_operation_add(m, block, &request, &op, &err) == YVEX_OK &&
            yvex_ir_seal(m, &err) == YVEX_OK);
        yvex_program_parameter_binding binding = {parameter, 0u, YVEX_GGUF_QTYPE_BF16};
        yvex_core_bytes bytes = {.maximum = 65536u};
        rc = yvex_program_execution_compile(&execution, m, &err);
        if (rc == YVEX_OK)
            rc = yvex_program_physical_compile(&physical, execution, "forward", &binding, 1u, identity, &err);
        if (rc == YVEX_OK) rc = yvex_program_physical_encode(physical, &bytes, &err);
        if (rc == YVEX_OK) rc = yvex_program_physical_decode(&copy, bytes.data, bytes.count, &err);
        if (rc != YVEX_OK) fprintf(stderr, "norm compile: %s\n", err.message);
        YVEX_CHECK(rc == YVEX_OK);
        const float weights[] = {0.8203125f, 1.296875f, -1.1015625f, 0.70703125f};
        unsigned short encoded[4];
        for (size_t i = 0u; i < 4u; ++i) encoded[i] = yvex_quant_bf16_encode(weights[i]);
        yvex_program_kernel_parameter p = {.tensor_id = 0u, .weight = {
            .encoded = (const unsigned char *)encoded, .encoded_bytes = sizeof(encoded),
            .row_count = 1u, .row_width = 4u, .row_bytes = 8u, .qtype = YVEX_GGUF_QTYPE_BF16}};
        YVEX_CHECK(yvex_backend_open_cpu(&backend, &err) == YVEX_OK);
        rc = yvex_program_stage_open(&stage, copy, &p, 1u, backend, 2u, 1, 1048576u, 1048576u, &err);
        if (rc != YVEX_OK) fprintf(stderr, "norm prepare: %s\n", err.message);
        YVEX_CHECK(rc == YVEX_OK);
        float input[16], result[16], expected[16]; unsigned int distinct = 0u;
        size_t count = grouped ? 16u : 8u;
        for (size_t i = 0u; i < count; ++i) input[i] = (float)((int)(i * 13u % 17u) - 8) / 8.0f;
        for (size_t g = 0u; g < count / 4u; ++g) {
            float sum = 0.0f;
            for (size_t j = 0u; j < 4u; ++j) sum += input[g * 4u + j] * input[g * 4u + j];
            float inverse = 1.0f / sqrtf(sum / 4.0f + 1e-5f);
            for (size_t j = 0u; j < 4u; ++j) {
                float normalized = yvex_quant_bf16_decode(yvex_quant_bf16_encode(input[g * 4u + j] * inverse));
                expected[g * 4u + j] = yvex_quant_bf16_decode(yvex_quant_bf16_encode(normalized * weights[j]));
                distinct += expected[g * 4u + j] !=
                    yvex_quant_bf16_decode(yvex_quant_bf16_encode(input[g * 4u + j] * inverse * weights[j]));
            }
        }
        const float *inputs[] = {input}; float *outputs[] = {result};
        yvex_backend_operation_facts facts;
        rc = yvex_program_stage_host(stage, 2u, inputs, 1u, outputs, 1u, NULL, NULL, &facts, &err);
        if (rc != YVEX_OK) fprintf(stderr, "norm execution: %s\n", err.message);
        YVEX_CHECK(rc == YVEX_OK && distinct && !memcmp(result, expected, count * sizeof(float)));
        YVEX_CHECK(yvex_program_stage_close(&stage, &err) == YVEX_OK &&
            yvex_backend_close_checked(&backend, &err) == YVEX_OK);
        free(bytes.data); yvex_program_physical_close(&copy); yvex_program_physical_close(&physical);
        yvex_program_execution_close(&execution); yvex_ir_module_close(&m);
    }
    return 0;
}

static int lfm_program_control(const yvex_lfm2_architecture *a)
{
    const char *source = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    yvex_ir_module *m = NULL;
    yvex_program_execution *execution = NULL;
    yvex_program_physical *physical = NULL, *copy = NULL;
    yvex_program_parameter_binding parameters[1024];
    yvex_core_bytes bytes = {.maximum = 16u * 1024u * 1024u};
    size_t count = 0u, convolution = 0u, attention = 0u, routed = 0u, banks = 0u;
    yvex_error err;
    int rc = yvex_model_register_lfm2()->program_build(&m, a, source, &err);
    if (rc != YVEX_OK) fprintf(stderr, "Liquid program: %s\n", err.message);
    YVEX_CHECK(rc == YVEX_OK);
    for (size_t i = 0u; i < yvex_ir_operation_count(m); ++i) {
        const yvex_ir_operation *op = yvex_ir_operation_at(m, (yvex_ir_id)i);
        convolution += !strcmp(op->definition->name, "sequence.double_gated_conv");
        attention += !strcmp(op->definition->name, "attention.causal");
        routed += !strcmp(op->definition->name, "nn.routed_swiglu");
        if (strcmp(op->definition->name, "core.parameter")) continue;
        YVEX_CHECK(count < 1024u);
        const yvex_ir_type *type = yvex_ir_type_at(m, yvex_ir_value_at(m, op->results[0])->type);
        parameters[count] = (yvex_program_parameter_binding){op->results[0], count,
            type->scalar == YVEX_IR_F32 ? YVEX_GGUF_QTYPE_F32 : YVEX_GGUF_QTYPE_BF16};
        count++; banks += op->definition->version == 2u;
    }
    YVEX_CHECK(convolution == a->convolution_layers && attention == a->attention_layers &&
        routed == a->layers - a->dense_layers && banks == 3u * routed);
    rc = yvex_program_execution_compile(&execution, m, &err);
    if (rc == YVEX_OK)
        rc = yvex_program_physical_compile(&physical, execution, "forward", parameters, count, source, &err);
    if (rc == YVEX_OK) rc = yvex_program_physical_encode(physical, &bytes, &err);
    if (rc == YVEX_OK) rc = yvex_program_physical_decode(&copy, bytes.data, bytes.count, &err);
    if (rc != YVEX_OK) fprintf(stderr, "Liquid physical: %s\n", err.message);
    YVEX_CHECK(rc == YVEX_OK);
    yvex_program_token_interface tokens;
    YVEX_CHECK(yvex_program_physical_token_interface(copy, &tokens, &err) == YVEX_OK &&
        tokens.attention_operations == a->attention_layers &&
        tokens.recurrent_operations == a->convolution_layers);
    printf("Liquid typed graph: conv=%zu attention=%zu routed=%zu explicit-expert-banks=%zu; "
        "physical import verified; not model execution\n", convolution, attention, routed, banks);
    free(bytes.data); yvex_program_physical_close(&copy); yvex_program_physical_close(&physical);
    yvex_program_execution_close(&execution); yvex_ir_module_close(&m);
    return 0;
}

static int lfm_attention_capabilities(void)
{
    yvex_attention_layer_plan layer = {
        .compute_contract = YVEX_ATTENTION_COMPUTE_BF16_EAGER_V1, .attention_class = YVEX_ATTENTION_CLASS_SWA,
        .tensor_scope = YVEX_TENSOR_SCOPE_MAIN_LAYER, .query_heads = 4u, .kv_heads = 2u,
        .head_dimension = 4u, .sliding_window = 128u};
    yvex_attention_summary summary = {.layer_count = 1u, .history_contract_ready = 1,
        .state_delta_contract_ready = 1, .cpu_reference_ready = 1,
        .cuda_execution_ready = 1, .full_execution_ready = 1};
    YVEX_CHECK(yvex_attention_plan_capabilities_match(&summary, &layer, 1u));
    summary.cuda_execution_ready = 0;
    YVEX_CHECK(yvex_attention_plan_capabilities_match(&summary, &layer, 1u));
    summary.cuda_execution_ready = 0; layer.compute_contract = YVEX_ATTENTION_COMPUTE_BF16_F32_RNE_V1;
    YVEX_CHECK(!yvex_attention_plan_capabilities_match(&summary, &layer, 1u));
    summary.cuda_execution_ready = 1;
    YVEX_CHECK(yvex_attention_plan_capabilities_match(&summary, &layer, 1u));
    layer.compute_contract = YVEX_ATTENTION_COMPUTE_BF16_EAGER_V1;
    layer.compressor_required = 1;
    YVEX_CHECK(!yvex_attention_plan_capabilities_match(&summary, &layer, 1u));
    layer.compressor_required = 0; layer.matrix_projection = YVEX_MATRIX_PROJECTION_PREPARED_BF16_F32_ACC_V1;
    YVEX_CHECK(!yvex_attention_plan_capabilities_match(&summary, &layer, 1u));
    return 0;
}

int yvex_test_lfm2_source(void)
{
    if (lfm_norm_control() || lfm_attention_capabilities()) return 1;
    const yvex_graph_execution_binding *family = yvex_graph_execution_find(
        YVEX_LFM2_ADAPTER_ID, YVEX_LFM2_ADAPTER_VERSION, YVEX_LFM2_TARGET);
    yvex_logits_family_policy logits = {0};
    YVEX_CHECK(family && family->compiler && family->compiler->logits_policy(&logits) &&
        logits.schema_version == YVEX_RUNTIME_LOGITS_SCHEMA_V1 &&
        logits.tied_output_head && !logits.separate_output_head && !logits.output_head_bias);
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
    if (lfm_program_control(&a)) return 1;
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
