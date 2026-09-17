/* Signal lowering: explicit channel/sample geometry, immutable parameters,
 * independent sparse-kernel oracle and closed-form anti-aliased activation. */
#ifndef TESTS_SUPPORT_SIGNAL_PROGRAM_H
#define TESTS_SUPPORT_SIGNAL_PROGRAM_H
#include "tests/test.h"
#include <yvex/internal/program_stage.h>
#include <yvex/internal/neural_operations.h>
#include <yvex/internal/component.h>
#include <yvex/internal/signal_program.h>
#include <yvex/qtype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    float parameters[4][16];
    unsigned long long counts[4], widths[4], output_channels, output_length;
    size_t parameter_count;
} signal_fixture;

static int signal_program(yvex_program_physical **out, signal_fixture *fixture,
    unsigned int scenario, unsigned int malformed, yvex_error *err)
{
    const char *source = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    int snake = scenario == 4u, transposed = scenario == 1u || scenario == 3u;
    int normalized = scenario == 2u || scenario == 3u, biased = scenario != 3u;
    yvex_ir_dialect dialects[] = {*yvex_ir_core_dialect(), *yvex_ir_neural_dialect()};
    yvex_ir_module *m = NULL;
    yvex_program_execution *e = NULL;
    yvex_ir_dimension batch = {.name = "batch", .minimum = 1u, .maximum = 2u, .multiple = 1u};
    yvex_ir_type x = {.kind = YVEX_IR_TENSOR, .scalar = YVEX_IR_F32, .rank = 3u,
        .shape = {{0u, 0u}, {YVEX_IR_NONE, 2u}, {YVEX_IR_NONE, 4u}}}, y = x;
    yvex_ir_id dimension, input_type, output_type, function, block = YVEX_IR_NONE, op, args[5];
    yvex_program_parameter_binding bindings[4];
    yvex_ir_attribute attrs[] = {
        {.name = "stride", .kind = YVEX_IR_ATTR_U64, .value.integer = transposed ? 2u : 1u},
        {.name = "dilation", .kind = YVEX_IR_ATTR_U64, .value.integer = 1u},
        {.name = "padding", .kind = YVEX_IR_ATTR_U64},
        {.name = "output_padding", .kind = YVEX_IR_ATTR_U64},
        {.name = "transposed", .kind = YVEX_IR_ATTR_BOOL, .value.integer = (unsigned int)transposed}};
    memset(fixture, 0, sizeof(*fixture));
    fixture->output_channels = snake ? 2u : 3u;
    fixture->output_length = snake ? 4u : transposed ? 8u : 3u;
    fixture->parameter_count = snake ? 4u : 1u + (size_t)normalized + (size_t)biased;
    y.shape[1].extent = fixture->output_channels; y.shape[2].extent = fixture->output_length;
    if (malformed == 1u) y.shape[2].extent++;
    if (malformed == 2u) attrs[0].value.integer = 0u;
    if (malformed == 3u) attrs[3].value.integer = 2u;
    if (malformed == 4u) y.scalar = YVEX_IR_BF16;
    if (malformed == 7u) attrs[1].value.integer = UINT64_MAX;
    int rc = yvex_ir_module_open(&m, "signal", source, dialects, 2u, err);
    if (rc == YVEX_OK) rc = yvex_ir_dimension_add(m, &batch, &dimension, err);
    if (rc == YVEX_OK) rc = yvex_ir_type_intern(m, &x, &input_type, err);
    if (rc == YVEX_OK) rc = yvex_ir_type_intern(m, &y, &output_type, err);
    if (rc == YVEX_OK) rc = yvex_ir_function_add(m, "forward", &input_type, 1u,
        &output_type, 1u, 0u, &function, err);
    if (rc == YVEX_OK) { block = yvex_ir_function_at(m, function)->body; args[0] = yvex_ir_block_at(m, block)->arguments[0]; }
    for (size_t i = 0u; rc == YVEX_OK && i < fixture->parameter_count; ++i) {
        yvex_ir_type t = {.kind = YVEX_IR_TENSOR, .scalar = YVEX_IR_F32, .rank = 1u,
            .shape = {{YVEX_IR_NONE, snake ? (i < 2u ? 2u : 12u) : 3u}}};
        if (!snake && !i) {
            t.rank = 3u;
            t.shape[0].extent = transposed ? 2u : 3u;
            t.shape[1] = (yvex_ir_extent){YVEX_IR_NONE, transposed ? 3u : 2u};
            t.shape[2] = (yvex_ir_extent){YVEX_IR_NONE, 2u};
        } else if (!snake && normalized && i == 1u) t.shape[0].extent = transposed ? 2u : 3u;
        if (malformed == 5u && i == 0u) t.shape[0].extent++;
        if (malformed == 6u && i == fixture->parameter_count - 1u) t.shape[0].extent++;
        yvex_ir_id type;
        rc = yvex_ir_type_intern(m, &t, &type, err);
        yvex_ir_attribute a[] = {{.name = "source", .kind = YVEX_IR_ATTR_TEXT},
            {.name = "parameter", .kind = YVEX_IR_ATTR_SYMBOL}};
        memcpy(a[0].value.text, source, 65u);
        snprintf(a[1].value.text, sizeof(a[1].value.text), "weight_%zu", i);
        yvex_ir_operation_request r = {.operation = "core.parameter", .result_types = &type,
            .result_count = 1u, .attributes = a, .attribute_count = 2u};
        if (rc == YVEX_OK) rc = yvex_ir_operation_add(m, block, &r, &op, err);
        if (rc != YVEX_OK) break;
        args[i + 1u] = yvex_ir_operation_at(m, op)->results[0];
        bindings[i] = (yvex_program_parameter_binding){args[i + 1u], i, YVEX_GGUF_QTYPE_F32};
        unsigned long long count = 1u;
        for (size_t axis = 0u; axis < t.rank; ++axis) count *= t.shape[axis].extent;
        fixture->counts[i] = count; fixture->widths[i] = t.shape[t.rank - 1u].extent;
        if (count > 16u) { rc = YVEX_ERR_BOUNDS; break; }
        if (snake) {
            if (i >= 2u) fixture->parameters[i][5] = i == 2u ? 0.5f : 1.0f;
        } else if (!i) {
            for (size_t row = 0u; row < t.shape[0].extent; ++row)
                fixture->parameters[i][row * t.shape[1].extent * 2u] = 1.0f;
        } else {
            for (size_t n = 0u; n < count; ++n) fixture->parameters[i][n] = normalized && i == 1u ? 0.5f : 0.25f;
        }
    }
    const char *name = snake ? "signal.alias_snake" : !normalized ? "signal.conv1d" :
        biased ? "signal.normalized_conv1d" : "signal.normalized_conv1d_unbiased";
    yvex_ir_operation_request operation = {.operation = name, .operands = args,
        .operand_count = fixture->parameter_count + 1u, .result_types = &output_type, .result_count = 1u,
        .attributes = snake ? NULL : attrs, .attribute_count = snake ? 0u : malformed == 8u ? 4u : 5u};
    if (rc == YVEX_OK) rc = yvex_ir_operation_add(m, block, &operation, &op, err);
    if (rc == YVEX_OK) {
        yvex_ir_id value = yvex_ir_operation_at(m, op)->results[0];
        operation = (yvex_ir_operation_request){.operation = "core.return", .operands = &value, .operand_count = 1u};
        rc = yvex_ir_operation_add(m, block, &operation, &op, err);
    }
    if (rc == YVEX_OK) rc = yvex_ir_seal(m, err);
    if (rc == YVEX_OK) rc = yvex_program_execution_compile(&e, m, err);
    if (rc == YVEX_OK) rc = yvex_program_physical_compile(out, e, "forward", bindings,
        fixture->parameter_count, source, err);
    yvex_program_execution_close(&e); yvex_ir_module_close(&m);
    return rc;
}

static int signal_cancel(void *context)
{
    unsigned int *calls = context;
    return ++*calls >= 2u;
}

static int test_signal_recipe(void)
{
    const char *identity = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    yvex_alias_decoder_name_templates names = {"input", "pre", "ups", "res", "activation", "post", 3u};
    yvex_alias_decoder_recipe recipe = {
        .input_channels = 32u, .projection_channels = 2048u, .input_kernel = 1u,
        .pre_channels = 1024u, .pre_kernel = 7u,
        .stage_count = 7u, .residual_blocks = 3u, .residual_layers = 3u,
        .rates = {5u, 5u, 2u, 2u, 2u, 2u, 2u},
        .upsample_kernels = {9u, 9u, 4u, 4u, 4u, 4u, 4u},
        .residual_kernels = {3u, 7u, 11u}, .residual_dilations = {1u, 3u, 5u},
        .final_channels = 1u, .final_kernel = 7u};
    yvex_signal_program program = {0}, repeat = {0};
    yvex_error err = {0};
    int rc = yvex_signal_program_compile(&program, &recipe, identity, 1u, 1u,
        yvex_alias_decoder_template_name, &names, &err);
    if (rc != YVEX_OK) fprintf(stderr, "signal recipe: %s: %s\n",
        yvex_error_where(&err), yvex_error_message(&err));
    YVEX_TEST_ASSERT(rc == YVEX_OK, "full multistage signal recipe lowers");
    YVEX_TEST_ASSERT(program.parameter_count == 914u && program.output_length == 800u &&
        program.output_values == 800u, "source convolution geometry and parameter linkage preserved");
    YVEX_TEST_ASSERT(yvex_signal_program_compile(&repeat, &recipe, identity, 1u, 1u,
        yvex_alias_decoder_template_name, &names, &err) == YVEX_OK, "repeat signal compilation");
    YVEX_TEST_ASSERT(!strcmp(yvex_program_physical_summary_get(program.physical)->identity,
        yvex_program_physical_summary_get(repeat.physical)->identity), "deterministic signal program identity");
    yvex_signal_program_close(&repeat);
    yvex_signal_program_close(&program);
    for (unsigned int negative = 0u; negative < 4u; ++negative) {
        yvex_alias_decoder_recipe bad = recipe;
        if (!negative) bad.rates[0] = 0u;
        if (negative == 1u) bad.pre_channels = 1023u;
        if (negative == 2u) bad.residual_dilations[0] = UINT64_MAX;
        if (negative == 3u) bad.stage_count = YVEX_ALIAS_DECODER_MAX_STAGES + 1u;
        YVEX_TEST_ASSERT(yvex_signal_program_compile(&program, &bad, identity, 1u, 1u,
            yvex_alias_decoder_template_name, &names, &err) != YVEX_OK && !program.physical,
            "invalid source recipe refuses without publishing a program");
    }
    printf("Signal recipe: seven stages, 914 parameters, 800 outputs; repeat identity exact; four negatives refused\n");
    return 0;
}

static int test_signal_programs(yvex_backend_kind kind)
{
    yvex_backend *backend = NULL;
    yvex_backend_options options = {.kind = kind};
    yvex_error err;
    if (kind == YVEX_BACKEND_KIND_CPU) {
        if (test_signal_recipe()) return 1;
        for (unsigned int bad = 1u; bad <= 8u; ++bad) {
            signal_fixture fixture;
            yvex_program_physical *invalid = NULL;
            int rc = signal_program(&invalid, &fixture, 2u, bad, &err);
            YVEX_TEST_ASSERT(rc == YVEX_ERR_FORMAT && !invalid,
                "compiler refuses wrong geometry, stride, padding, precision, contraction, gain/bias or attributes");
        }
        for (unsigned int bad = 1u; bad <= 6u; ++bad) {
            if (bad == 2u || bad == 3u) continue;
            signal_fixture fixture;
            yvex_program_physical *invalid = NULL;
            YVEX_TEST_ASSERT(signal_program(&invalid, &fixture, 4u, bad, &err) == YVEX_ERR_FORMAT && !invalid,
                "compiler refuses Snake output shape, precision, channel parameters and filter geometry");
        }
        printf("Signal IR negatives: 12 invalid geometry/precision/parameter/attribute cases refused before execution\n");
    }
    YVEX_TEST_ASSERT(yvex_backend_open(&backend, &options, &err) == YVEX_OK, "signal backend opens");
    for (unsigned int scenario = 0u; scenario < 5u; ++scenario) {
        signal_fixture fixture;
        yvex_program_physical *p = NULL, *decoded = NULL;
        yvex_program_stage *stage = NULL;
        yvex_device_tensor *resident = NULL;
        yvex_program_kernel_parameter parameters[4];
        yvex_backend_operation_facts facts;
        yvex_core_bytes wire = {.maximum = 1048576u};
        int rc = signal_program(&p, &fixture, scenario, 0u, &err);
        if (rc == YVEX_OK) rc = yvex_program_physical_encode(p, &wire, &err);
        if (rc == YVEX_OK) rc = yvex_program_physical_decode(&decoded, wire.data, wire.count, &err);
        if (rc != YVEX_OK) fprintf(stderr, "signal compile: %s: %s\n", yvex_error_where(&err), yvex_error_message(&err));
        YVEX_TEST_ASSERT(rc == YVEX_OK && !strcmp(yvex_program_physical_summary_get(p)->identity,
            yvex_program_physical_summary_get(decoded)->identity), "signal physical import preserves identity");
        unsigned char *arena = (unsigned char *)fixture.parameters;
        if (kind == YVEX_BACKEND_KIND_CUDA) {
            yvex_backend_tensor_desc d = {.name = "signal-parameters", .dtype = YVEX_DTYPE_I8,
                .rank = 1u, .dims = {sizeof(fixture.parameters)}, .bytes = sizeof(fixture.parameters)};
            arena = NULL;
            YVEX_TEST_ASSERT(yvex_backend_resident_alloc(backend, &d, &resident, &arena, &err) == YVEX_OK,
                "signal parameter arena allocation");
            memcpy(arena, fixture.parameters, sizeof(fixture.parameters));
            YVEX_TEST_ASSERT(yvex_backend_resident_attach(backend, arena, d.bytes, resident, 1u, &err) == YVEX_OK,
                "signal residency binds exact encoded source");
        }
        for (size_t i = 0u; i < fixture.parameter_count; ++i) {
            unsigned long long bytes = fixture.counts[i] * sizeof(float);
            unsigned char *host = arena + i * sizeof(fixture.parameters[0]);
            parameters[i] = (yvex_program_kernel_parameter){.tensor_id = i,
                .weight = {.encoded = host, .encoded_bytes = bytes, .qtype = YVEX_GGUF_QTYPE_F32,
                    .row_count = fixture.counts[i] / fixture.widths[i], .row_width = fixture.widths[i],
                    .row_bytes = fixture.widths[i] * sizeof(float)}};
        }
        rc = yvex_program_stage_open(&stage, decoded, parameters, fixture.parameter_count,
            backend, 2u, 1, 1048576u, 1048576u, &err);
        if (rc != YVEX_OK) fprintf(stderr, "signal prepare: %s: %s\n", yvex_error_where(&err), yvex_error_message(&err));
        YVEX_TEST_ASSERT(rc == YVEX_OK, "signal physical program prepares");
        float input[16], output[48], maximum = 0.0f;
        for (size_t i = 0u; i < 16u; ++i) input[i] = (float)((int)i - 8) / 8.0f;
        const float *inputs[] = {input}; float *outputs[] = {output};
        for (size_t run = 0u; run < 2u; ++run) {
            rc = yvex_program_stage_host(stage, 2u, inputs, 1u, outputs, 1u, NULL, NULL, &facts, &err);
            if (rc != YVEX_OK) fprintf(stderr, "signal execute: %s: %s\n", yvex_error_where(&err), yvex_error_message(&err));
            YVEX_TEST_ASSERT(rc == YVEX_OK, "signal program executes without procedural decoder");
            for (size_t b = 0u; b < 2u; ++b) for (size_t c = 0u; c < fixture.output_channels; ++c)
                for (size_t t = 0u; t < fixture.output_length; ++t) {
                    double expected = scenario == 3u ? 0.0 : 0.25;
                    if (scenario == 4u) { double x = input[(b * 2u + c) * 4u + t]; expected = x + sin(x) * sin(x); }
                    else if (scenario == 1u || scenario == 3u) {
                        if (!c && !(t % 2u)) expected += (input[b * 8u + t / 2u] + input[b * 8u + 4u + t / 2u]) *
                            (scenario == 3u ? 0.5 : 1.0);
                    } else expected += input[b * 8u + t] * (scenario == 2u ? 0.5 : 1.0);
                    size_t index = (b * fixture.output_channels + c) * fixture.output_length + t;
                    float difference = (float)fabs(output[index] - expected);
                    if (difference > maximum) maximum = difference;
                    YVEX_TEST_ASSERT(isfinite(output[index]) && difference <= (scenario == 4u ? 1e-6f : 0.0f),
                        "independent sparse convolution / closed-form Snake oracle");
                }
        }
        for (size_t i = 0u; i < 48u; ++i) output[i] = -77.0f;
        unsigned int calls = 0u;
        rc = yvex_program_stage_host(stage, 2u, inputs, 1u, outputs, 1u, signal_cancel, &calls, &facts, &err);
        YVEX_TEST_ASSERT(rc == YVEX_ERR_CANCELLED && output[0] == -77.0f, "cancelled signal result is unpublished");
        YVEX_TEST_ASSERT(yvex_program_stage_close(&stage, &err) == YVEX_OK, "release signal execution");
        if (kind == YVEX_BACKEND_KIND_CUDA) {
            YVEX_TEST_ASSERT(yvex_backend_resident_detach(backend, &err) == YVEX_OK, "release signal residency");
            YVEX_TEST_ASSERT(yvex_backend_tensor_release(backend, &resident, &err) == YVEX_OK, "release weight arena");
        }
        printf("Signal program %s scenario=%u values=%llu repeats=2 max_abs=%.9g tolerance=%.9g; "
            "binary identity stable; cancellation unpublished\n", kind == YVEX_BACKEND_KIND_CUDA ? "CUDA" : "CPU",
            scenario, 2u * fixture.output_channels * fixture.output_length, (double)maximum,
            scenario == 4u ? 1e-6 : 0.0);
        yvex_program_physical_close(&p); yvex_program_physical_close(&decoded); free(wire.data);
    }
    YVEX_TEST_ASSERT(yvex_backend_close_checked(&backend, &err) == YVEX_OK, "signal resources reclaimed");
    return 0;
}
#endif
