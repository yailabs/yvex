/* Synthetic, bounded architecture-import consumer; never a model support claim. */
#ifndef TESTS_SUPPORT_PROGRAM_SEQUENCE_H
#define TESTS_SUPPORT_PROGRAM_SEQUENCE_H
#include <yvex/internal/families/qwen3_5.h>
#include <yvex/internal/program_physical.h>
#include <yvex/qtype.h>
#include <string.h>

static int test_sequence_program_kind(yvex_ir_module **module, yvex_program_physical **physical,
    int hybrid, yvex_error *err)
{
    static const char source[] = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    yvex_qwen3_5_architecture a = {.source_multimodal = 1,
        .text = {.hidden_size = 32u, .intermediate_size = 64u, .vocabulary_size = 64u,
            .layer_count = 2u, .linear_attention_layers = 2u, .full_attention_interval = 3u,
            .maximum_positions = 8u, .attention_heads = 1u, .kv_heads = 1u, .attention_head_dimension = 32u,
            .rope_theta = 10000u, .partial_rotary_factor = 0.5, .rope_partial_rotary_factor = 0.5,
            .mrope_sections = {8u, 0u, 0u}, .linear_key_heads = 1u, .linear_value_heads = 1u,
            .linear_key_head_dimension = 32u, .linear_value_head_dimension = 32u, .linear_convolution_kernel = 3u,
            .bos_token_id = 0u, .eos_token_id = 63u, .rms_norm_epsilon = 1e-6,
            .layers = {YVEX_QWEN3_5_LAYER_LINEAR_ATTENTION, YVEX_QWEN3_5_LAYER_LINEAR_ATTENTION},
            .model_type = "qwen3_5_text", .source_dtype = "bfloat16", .recurrent_state_dtype = "float32",
            .hidden_activation = "silu", .output_gate_type = "swish", .attention_output_gate = 1, .use_cache = 1},
        .vision = {.output_hidden_size = 32u, .model_type = "qwen3_5"},
        .generation = {.bos_token_id = 0u, .pad_token_id = 63u, .stop_token_count = 1u,
            .stop_token_ids = {63u}, .temperature = 1.0, .top_p = 1.0}};
    yvex_program_parameter_binding bindings[64];
    yvex_program_execution *execution = NULL;
    size_t i, count = 0u;
    if (hybrid) {
        a.text.layer_count = 3u;
        a.text.full_attention_layers = 1u;
        a.text.layers[2] = YVEX_QWEN3_5_LAYER_FULL_ATTENTION;
    }
    int rc = yvex_qwen3_5_program_build(module, &a, source, err);
    for (i = 0u; rc == YVEX_OK && i < yvex_ir_operation_count(*module); ++i) {
        const yvex_ir_operation *op = yvex_ir_operation_at(*module, (yvex_ir_id)i);
        if (strcmp(op->definition->name, "core.parameter")) continue;
        if (count >= 64u) { rc = YVEX_ERR_BOUNDS; break; }
        bindings[count] = (yvex_program_parameter_binding){op->results[0], count, YVEX_GGUF_QTYPE_BF16};
        count++;
    }
    if (rc == YVEX_OK) rc = yvex_program_execution_compile(&execution, *module, err);
    if (rc == YVEX_OK) rc = yvex_program_physical_compile(physical, execution, "forward", bindings, count, source, err);
    yvex_program_execution_close(&execution);
    return rc;
}
static int test_sequence_program(yvex_ir_module **module, yvex_program_physical **physical, yvex_error *err)
{
    return test_sequence_program_kind(module, physical, 0, err);
}
#endif
