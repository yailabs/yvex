/* Qualify pinned Qwen3.5 hybrid semantics without model weights. */
#include "tests/test.h"

#include <yvex/internal/core.h>
#include <yvex/internal/families/qwen3_5.h>
#include <yvex/internal/program.h>
#include <yvex/internal/graph.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int qwen_test_dir(const char *path)
{
    return mkdir(path, 0777) == 0 || errno == EEXIST;
}

static int qwen_test_config(const char *path, unsigned long long interval)
{
    FILE *fp = fopen(path, "wb");
    unsigned long long layer;

    if (!fp) return 0;
    if (fprintf(
            fp,
            "{\"architectures\":[\"Qwen3_5ForConditionalGeneration\"],"
            "\"image_token_id\":248056,\"language_model_only\":false,"
            "\"model_type\":\"qwen3_5\",\"text_config\":{"
            "\"attention_bias\":false,\"attention_dropout\":0.0,"
            "\"attn_output_gate\":true,\"bos_token_id\":248044,"
            "\"dtype\":\"bfloat16\",\"eos_token_id\":248044,"
            "\"full_attention_interval\":%llu,\"head_dim\":256,"
            "\"hidden_act\":\"silu\",\"hidden_size\":5120,"
            "\"intermediate_size\":17408,\"layer_types\":[",
            interval) < 0) {
        fclose(fp);
        return 0;
    }
    for (layer = 0ull; layer < 64ull; ++layer) {
        const char *kind = (layer + 1ull) % 4ull == 0ull
                               ? "full_attention"
                               : "linear_attention";

        if (fprintf(fp, "%s\"%s\"", layer ? "," : "", kind) < 0) {
            fclose(fp);
            return 0;
        }
    }
    if (fprintf(
            fp,
            "],\"linear_conv_kernel_dim\":4,\"linear_key_head_dim\":128,"
            "\"linear_num_key_heads\":16,\"linear_num_value_heads\":48,"
            "\"linear_value_head_dim\":128,\"mamba_ssm_dtype\":\"float32\","
            "\"max_position_embeddings\":262144,"
            "\"model_type\":\"qwen3_5_text\",\"mtp_num_hidden_layers\":1,"
            "\"mtp_use_dedicated_embeddings\":false,"
            "\"num_attention_heads\":24,\"num_hidden_layers\":64,"
            "\"num_key_value_heads\":4,\"output_gate_type\":\"swish\","
            "\"pad_token_id\":null,\"partial_rotary_factor\":0.25,"
            "\"rms_norm_eps\":0.000001,\"rope_parameters\":{"
            "\"mrope_interleaved\":true,\"mrope_section\":[11,11,10],"
            "\"partial_rotary_factor\":0.25,\"rope_theta\":10000000,"
            "\"rope_type\":\"default\"},\"tie_word_embeddings\":false,"
            "\"use_cache\":true,\"vocab_size\":248320},"
            "\"tie_word_embeddings\":false,"
            "\"transformers_version\":\"5.8.0.dev0\","
            "\"video_token_id\":248057,\"vision_config\":{"
            "\"deepstack_visual_indexes\":[],\"depth\":27,"
            "\"hidden_act\":\"gelu_pytorch_tanh\",\"hidden_size\":1152,"
            "\"intermediate_size\":4304,\"model_type\":\"qwen3_5\","
            "\"num_heads\":16,\"num_position_embeddings\":2304,"
            "\"out_hidden_size\":5120,\"patch_size\":16,"
            "\"spatial_merge_size\":2,\"temporal_patch_size\":2},"
            "\"vision_end_token_id\":248054,\"vision_start_token_id\":248053}") < 0) {
        fclose(fp);
        return 0;
    }
    return fclose(fp) == 0;
}

static int qwen_test_generation(const char *path)
{
    FILE *fp = fopen(path, "wb");

    if (!fp) return 0;
    if (fputs("{\"bos_token_id\":248044,\"do_sample\":true,"
              "\"eos_token_id\":[248046,248044],\"pad_token_id\":248044,"
              "\"temperature\":1.0,\"top_k\":20,\"top_p\":0.95}", fp) < 0) {
        fclose(fp);
        return 0;
    }
    return fclose(fp) == 0;
}

static void qwen_test_verification(yvex_source_verification *verification,
                                   const char *root)
{
    memset(verification, 0, sizeof(*verification));
    verification->verified = 1;
    verification->config_valid = 1;
    yvex_core_text_copy(verification->resolved_source_path,
                        sizeof(verification->resolved_source_path), root);
    yvex_core_text_copy(verification->repository_id,
                        sizeof(verification->repository_id),
                        "Qwen/Qwen3.8-27B");
    yvex_core_text_copy(verification->revision,
                        sizeof(verification->revision),
                        "1d4bf0f2ff6012fd82039f2fa52739d0dd7c60c0");
    yvex_core_text_copy(verification->model_type,
                        sizeof(verification->model_type), "qwen3_5");
    yvex_core_text_copy(verification->architecture,
                        sizeof(verification->architecture),
                        "Qwen3_5ForConditionalGeneration");
}

static int qwen_test_program(const yvex_qwen3_5_architecture *architecture)
{
    static const char source[] = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    yvex_ir_module *program = NULL, *repeat = NULL, *imported = NULL;
    yvex_program_execution *lowered = NULL;
    yvex_ir_dialect dialects[] = {
        *yvex_ir_core_dialect(), *yvex_ir_neural_dialect(), *yvex_ir_sequence_dialect()};
    yvex_ir_id function, id;
    const yvex_ir_function *forward;
    const yvex_ir_block *body;
    yvex_core_bytes bytes = {.maximum = 4u * 1024u * 1024u};
    unsigned int delta = 0u, attention = 0u, norms = 0u, ffn = 0u, parameters = 0u;
    yvex_error error;
    int rc = yvex_qwen3_5_program_build(&program, architecture, source, &error);
    if (rc != YVEX_OK) fprintf(stderr, "Qwen program refusal: %s\n", yvex_error_message(&error));
    YVEX_TEST_ASSERT(rc == YVEX_OK && yvex_ir_function_count(program) == 3u &&
                     yvex_ir_function_find(program, "forward", &function), "typed forward/output programs");
    forward = yvex_ir_function_at(program, function);
    body = yvex_ir_block_at(program, forward->body);
    YVEX_TEST_ASSERT(body->argument_count == 114u && forward->result_count == 113u,
                     "tokens + position + 48 convolution/48 recurrent/16 KV states; distinct output versions");
    for (id = body->first_operation; id != YVEX_IR_NONE; id = yvex_ir_operation_at(program, id)->next) {
        const yvex_ir_operation *op = yvex_ir_operation_at(program, id);
        YVEX_TEST_ASSERT(strcmp(op->definition->name, "core.call"), "all 64 FFN component calls were legalized");
        delta += !strcmp(op->definition->name, "sequence.gated_delta");
        attention += !strcmp(op->definition->name, "attention.gated_causal");
        norms += !strcmp(op->definition->name, "nn.rms_norm");
        ffn += !strcmp(op->definition->name, "nn.silu_product");
        parameters += !strcmp(op->definition->name, "core.parameter");
    }
    YVEX_TEST_ASSERT(delta == 48u && attention == 16u && norms == 129u && ffn == 64u && parameters == 850u,
                     "source projection preserves real hybrid operations and every forward parameter");
    parameters = 0u;
    for (id = 0u; id < yvex_ir_operation_count(program); ++id) {
        const yvex_ir_operation *op = yvex_ir_operation_at(program, id);
        const yvex_ir_type *type;
        const yvex_ir_attribute *parameter;
        yvex_native_weight_info tensor = {0};
        yvex_qwen3_5_tensor_binding classified = {0};
        yvex_qwen3_5_failure failure = {0};
        unsigned int dimension;
        if (strcmp(op->definition->name, "core.parameter")) continue;
        type = yvex_ir_type_at(program, yvex_ir_value_at(program, op->results[0])->type);
        parameter = yvex_ir_attribute_get(program, id, "parameter");
        tensor.name = parameter->value.text;
        tensor.dtype = YVEX_NATIVE_DTYPE_BF16;
        tensor.rank = type->rank;
        tensor.data_bytes = 2u;
        for (dimension = 0u; dimension < type->rank; ++dimension) {
            tensor.dims[dimension] = type->shape[dimension].extent;
            YVEX_TEST_ASSERT(yvex_core_u64_mul(tensor.data_bytes, tensor.dims[dimension], &tensor.data_bytes),
                             "parameter byte geometry is bounded");
        }
        rc = yvex_model_register_qwen3_5()->tensor_classify(architecture, &tensor, &classified, &failure, &error);
        if (rc != YVEX_OK) fprintf(stderr, "Qwen parameter %s: %s\n", tensor.name, yvex_error_message(&error));
        YVEX_TEST_ASSERT(rc == YVEX_OK && classified.classification == YVEX_QWEN3_5_TENSOR_TEXT_EXECUTION_REQUIRED,
                         "each program parameter agrees with the independent source tensor-role classifier");
        parameters++;
    }
    YVEX_TEST_ASSERT(parameters == 851u, "all forward and output parameters have source-admitted geometry");
    YVEX_TEST_ASSERT(yvex_qwen3_5_program_build(&repeat, architecture, source, &error) == YVEX_OK &&
                     !strcmp(yvex_ir_identity(program), yvex_ir_identity(repeat)), "repeatable source projection");
    rc = yvex_ir_encode(program, &bytes, &error);
    if (rc == YVEX_OK) rc = yvex_ir_decode(&imported, bytes.data, bytes.count, dialects, 3u, &error);
    if (rc != YVEX_OK) fprintf(stderr, "Qwen program roundtrip: %s\n", yvex_error_message(&error));
    YVEX_TEST_ASSERT(rc == YVEX_OK &&
                     !strcmp(yvex_ir_identity(program), yvex_ir_identity(imported)), "hybrid program roundtrip");
    YVEX_TEST_ASSERT(yvex_program_execution_compile(&lowered, imported, &error) == YVEX_OK &&
        yvex_program_execution_entry_count(lowered) == 3u,
        "imported Qwen module lowers every computational entry to explicit dependency/value slots");
    {
        const yvex_program_entry *entry = yvex_program_execution_entry_at(lowered, 1u);
        size_t step, dependency;
        YVEX_TEST_ASSERT(entry && !strcmp(entry->symbol, "forward") && entry->input_count == 114u &&
            entry->result_count == 113u, "execution form preserves all 112 typed state inputs and successors");
        for (step = 0u; step < entry->step_count; ++step)
            for (dependency = 0u; dependency < entry->steps[step].dependency_count; ++dependency)
                YVEX_TEST_ASSERT(entry->steps[step].dependencies[dependency] < step,
                    "all whole-forward dependencies are compiled before their use");
        printf("Qwen execution form: entries=3 forward_steps=%zu values=%zu state_inputs=112 state_results=112\n",
               entry->step_count, entry->value_count);
    }
    printf("Qwen program: forward inputs=114 results=113; delta=48 attention=16 RMSNorm=129 FFN=64 parameters=851\n");
    free(bytes.data);
    yvex_program_execution_close(&lowered);
    yvex_ir_module_close(&imported);
    yvex_ir_module_close(&repeat);
    yvex_ir_module_close(&program);
    return 0;
}

static int qwen_test_lowering(const yvex_graph_execution_binding *execution,
    const yvex_source_verification *verification)
{
    yvex_semantic_model_ir *semantic = NULL;
    yvex_source_verification source = *verification;
    const yvex_semantic_decoder_layer *layers;
    const yvex_semantic_attention_layer *attention;
    unsigned long long count, index;
    yvex_error error;
    int rc;
    yvex_core_text_copy(source.manifest_payload_identity, sizeof(source.manifest_payload_identity),
                        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
    rc = execution->compiler->binding_pipeline->semantic_model_build(&semantic, &source, &error);
    if (rc != YVEX_OK) fprintf(stderr, "Qwen lowering refusal: %s\n", yvex_error_message(&error));
    YVEX_TEST_ASSERT(rc == YVEX_OK &&
                     yvex_ir_identity(yvex_semantic_model_ir_program(semantic)) &&
                     yvex_semantic_model_ir_decoder_view(semantic, &layers, &count) && count == 64u,
                     "actual compiler lowers source-bound program into current execution records");
    for (index = 0u; index < count; ++index) {
        const yvex_semantic_decoder_layer *layer = &layers[index];
        int full = (index + 1u) % 4u == 0u;
        YVEX_TEST_ASSERT(layer->hidden_width == 5120u && layer->intermediate_width == 17408u &&
                         layer->normalization_epsilon == 1.0e-6 &&
                         layer->normalization_weight_convention == YVEX_NORMALIZATION_WEIGHT_ONE_PLUS &&
                         layer->mixer == (full ? YVEX_SEMANTIC_DECODER_MIXER_FULL_CAUSAL_ATTENTION :
                                                YVEX_SEMANTIC_DECODER_MIXER_GATED_DELTA),
                         "lowered shape, normalization and operation classes preserve the admitted source");
        if (!full)
            YVEX_TEST_ASSERT(layer->gated_delta.query_heads == 16u && layer->gated_delta.value_heads == 48u &&
                             layer->gated_delta.key_head_dimension == 128u &&
                             layer->gated_delta.value_head_dimension == 128u &&
                             layer->gated_delta.convolution_kernel == 4u &&
                             layer->gated_delta.recurrent_state_dtype == YVEX_DTYPE_F32 &&
                             layer->gated_delta.numeric_contract == YVEX_SEQUENCE_MIXER_NUMERIC_F32_RECURRENCE,
                             "gated-delta geometry and F32 recurrence survive program lowering");
    }
    YVEX_TEST_ASSERT(yvex_semantic_model_ir_attention_view(semantic, YVEX_TENSOR_SCOPE_MAIN_LAYER,
                                                          &attention, &count) && count == 16u,
                     "attention schedule is projected from typed attention operations");
    for (index = 0u; index < count; ++index)
        YVEX_TEST_ASSERT(attention[index].layer_index == 4u * index + 3u &&
                         attention[index].query_heads == 24u && attention[index].kv_heads == 4u &&
                         attention[index].head_dimension == 256u && attention[index].rope_head_dimension == 64u &&
                         attention[index].position.maximum_context == 262144u &&
                         attention[index].position.theta == 10000000u &&
                         attention[index].compute_contract == YVEX_ATTENTION_COMPUTE_BF16_F32_RNE_V1,
                         "attention population, position semantics and rounding contract preserved");
    printf("Qwen lowering: 64/64 execution records and 16/16 attention records match source geometry/numerical class\n");
    yvex_semantic_model_ir_close(&semantic);
    return 0;
}

int yvex_test_qwen3_5_architecture(void)
{
    const char *root = "build/tests/qwen3-5-architecture";
    const yvex_qwen3_5_api *api = yvex_model_register_qwen3_5();
    const yvex_graph_execution_binding *execution =
        yvex_graph_execution_find(0ull, 0ull, YVEX_QWEN3_8_27B_TARGET_ID);
    const yvex_qwen3_5_architecture *architecture;
    yvex_qwen3_5_model *model = NULL;
    yvex_qwen3_5_failure failure;
    yvex_source_verification verification;
    yvex_error err;
    char config[512], generation[512];
    int rc;

    YVEX_TEST_ASSERT(qwen_test_dir("build") && qwen_test_dir("build/tests") &&
                         qwen_test_dir(root),
                     "create Qwen semantic fixture root");
    snprintf(config, sizeof(config), "%s/config.json", root);
    snprintf(generation, sizeof(generation), "%s/generation_config.json", root);
    YVEX_TEST_ASSERT(qwen_test_config(config, 4ull) &&
                         qwen_test_generation(generation),
                     "write pinned-shape Qwen configuration fixture");
    qwen_test_verification(&verification, root);
    yvex_error_clear(&err);
    rc = api ? api->open(&model, &verification, &failure, &err)
             : YVEX_ERR_STATE;
    if (rc != YVEX_OK)
        fprintf(stderr, "Qwen architecture refusal: %s field=%s reason=%s\n",
                yvex_error_message(&err), failure.field,
                failure.reason ? failure.reason : "none");
    YVEX_TEST_ASSERT(api && api->schema_version == 3u &&
                         rc == YVEX_OK,
                     "open authenticated Qwen3.5 semantic architecture");
    YVEX_TEST_ASSERT(
        execution && execution->compiler &&
            !strcmp(execution->logical_transform_identity,
                    YVEX_QWEN3_5_LOGICAL_TRANSFORM_IDENTITY) &&
            !strcmp(execution->compiler->logical_transform_identity,
                    YVEX_QWEN3_5_LOGICAL_TRANSFORM_IDENTITY),
        "Qwen deployment and compiler expose one current logical transform identity");
    architecture = api->architecture(model);
    YVEX_TEST_ASSERT(
        architecture && strcmp(architecture->product_id, "qwen3.8-27b") == 0 &&
            strcmp(architecture->semantic_family, "qwen3_5") == 0 &&
            architecture->source_multimodal && architecture->text_specialization &&
            architecture->vision_execution_deferred &&
            architecture->mtp_acceleration_deferred &&
            yvex_sha256_hex_valid(architecture->architecture_identity),
        "release, architecture family, specialization, and identity remain distinct");
    YVEX_TEST_ASSERT(
        architecture->text.hidden_size == 5120ull &&
            architecture->text.layer_count == 64ull &&
            architecture->text.linear_attention_layers == 48ull &&
            architecture->text.full_attention_layers == 16ull &&
            architecture->text.intermediate_size == 17408ull &&
            architecture->text.vocabulary_size == 248320ull &&
            architecture->text.maximum_positions == 262144ull &&
            architecture->text.rotary_dimension == 64ull &&
            architecture->text.recurrent_state_f32 &&
            api->layer_kind(model, 2ull) ==
                YVEX_QWEN3_5_LAYER_LINEAR_ATTENTION &&
            api->layer_kind(model, 3ull) ==
                YVEX_QWEN3_5_LAYER_FULL_ATTENTION,
        "pinned hybrid text topology is exact");
    YVEX_TEST_ASSERT(
        architecture->text.attention_heads == 24ull &&
            architecture->text.kv_heads == 4ull &&
            architecture->text.attention_head_dimension == 256ull &&
            architecture->text.linear_key_heads == 16ull &&
            architecture->text.linear_value_heads == 48ull &&
            architecture->text.linear_key_head_dimension == 128ull &&
            architecture->text.linear_value_head_dimension == 128ull &&
            architecture->text.linear_convolution_kernel == 4ull,
        "full-attention and recurrent sequence-mixer geometry is exact");
    YVEX_TEST_ASSERT(
        architecture->vision.depth == 27ull &&
            architecture->vision.hidden_size == 1152ull &&
            architecture->vision.output_hidden_size == 5120ull &&
            architecture->generation.stop_token_count == 2ull &&
            architecture->generation.stop_token_ids[0] == 248046ull &&
            architecture->generation.stop_token_ids[1] == 248044ull,
        "deferred vision and generation facts remain accounted");
    YVEX_TEST_ASSERT(qwen_test_program(architecture) == 0, "Qwen typed program source projection");
    YVEX_TEST_ASSERT(qwen_test_lowering(execution, &verification) == 0, "Qwen compiler consumer");
    api->close(&model);
    YVEX_TEST_ASSERT(!model, "close semantic architecture");

    YVEX_TEST_ASSERT(qwen_test_config(config, 5ull),
                     "write inconsistent hybrid interval fixture");
    yvex_error_clear(&err);
    YVEX_TEST_ASSERT(
        api->open(&model, &verification, &failure, &err) != YVEX_OK && !model &&
            failure.code == YVEX_QWEN3_5_FAILURE_CONFIGURATION &&
            strcmp(failure.field, "layer_types") == 0,
        "inconsistent hybrid topology fails closed");
    verification.revision[0] = '0';
    YVEX_TEST_ASSERT(
        api->open(&model, &verification, &failure, &err) != YVEX_OK &&
            failure.code == YVEX_QWEN3_5_FAILURE_SOURCE_IDENTITY,
        "mutable or foreign source identity cannot construct Qwen semantics");
    return 0;
}
