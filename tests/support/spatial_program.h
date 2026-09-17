/* Compiled spatial operators versus independent scalar formulas. */
#ifndef TESTS_SUPPORT_SPATIAL_PROGRAM_H
#define TESTS_SUPPORT_SPATIAL_PROGRAM_H
#include "tests/test.h"
#include <yvex/internal/program_stage.h>
#include <yvex/internal/neural_operations.h>
#include <yvex/qtype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int spatial_fixture_compile(yvex_program_physical **out, int norm, unsigned int bad,
    yvex_error *err)
{
    const char *identity = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    yvex_ir_dialect dialects[] = {*yvex_ir_core_dialect(), *yvex_ir_neural_dialect()};
    yvex_ir_module *m = NULL;
    yvex_program_execution *e = NULL;
    yvex_ir_type x = {.kind = YVEX_IR_TENSOR, .scalar = YVEX_IR_F32, .rank = 4u,
        .shape = {{0u, 0u}, {YVEX_IR_NONE, 2u}, {YVEX_IR_NONE, 3u}, {YVEX_IR_NONE, 4u}}}, y = x;
    yvex_ir_type w = {.kind = YVEX_IR_TENSOR, .scalar = YVEX_IR_F32, .rank = norm ? 1u : 5u,
        .shape = {{YVEX_IR_NONE, norm ? 2u : 3u}, {YVEX_IR_NONE, 2u},
                  {YVEX_IR_NONE, 2u}, {YVEX_IR_NONE, 3u}, {YVEX_IR_NONE, 1u}}};
    yvex_ir_type bias = {.kind = YVEX_IR_TENSOR, .scalar = YVEX_IR_F32, .rank = 1u,
        .shape = {{YVEX_IR_NONE, norm ? 2u : 3u}}};
    y.shape[1].extent = norm ? 2u : 3u;
    if (bad == 1u) y.shape[2].extent++;
    if (bad == 2u) bias.shape[0].extent++;
    if (bad == 3u) w.scalar = YVEX_IR_BF16;
    if (bad == 4u) y.shape[0] = (yvex_ir_extent){YVEX_IR_NONE, 1u};
    yvex_ir_attribute attrs[] = {
        {.name = "stride_height", .kind = YVEX_IR_ATTR_U64, .value.integer = bad == 5u ? 0u : 1u},
        {.name = "stride_width", .kind = YVEX_IR_ATTR_U64, .value.integer = 1u},
        {.name = "padding_top", .kind = YVEX_IR_ATTR_U64, .value.integer = bad == 6u ? UINT64_MAX : 1u},
        {.name = "padding_bottom", .kind = YVEX_IR_ATTR_U64, .value.integer = 1u},
        {.name = "padding_left", .kind = YVEX_IR_ATTR_U64},
        {.name = "padding_right", .kind = YVEX_IR_ATTR_U64},
        {.name = "kernel_plane", .kind = YVEX_IR_ATTR_U64, .value.integer = bad == 7u ? 2u : 1u},
        {.name = "reflect", .kind = YVEX_IR_ATTR_BOOL, .value.integer = 1u}};
    yvex_ir_attribute norms[] = {
        {.name = "groups", .kind = YVEX_IR_ATTR_U64, .value.integer = bad == 5u ? 0u : bad == 6u ? 3u : 1u},
        {.name = "epsilon", .kind = YVEX_IR_ATTR_F64, .value.real = bad == 7u ? 0.0 : 1e-6}};
    yvex_ir_id dim, types[4], function, block = YVEX_IR_NONE, op, args[3] = {YVEX_IR_NONE};
    yvex_ir_dimension batch = {.name = "batch", .minimum = 2u, .maximum = 2u, .multiple = 1u};
    yvex_program_parameter_binding bindings[2];
    int rc = yvex_ir_module_open(&m, "spatial_fixture", identity, dialects, 2u, err);
    if (rc == YVEX_OK) rc = yvex_ir_dimension_add(m, &batch, &dim, err);
    yvex_ir_type ts[] = {x, y, w, bias};
    for (size_t i = 0u; rc == YVEX_OK && i < 4u; ++i) rc = yvex_ir_type_intern(m, ts + i, types + i, err);
    if (rc == YVEX_OK) rc = yvex_ir_function_add(m, "forward", types, 1u, types + 1u, 1u, 0u, &function, err);
    if (rc == YVEX_OK) {
        block = yvex_ir_function_at(m, function)->body;
        args[0] = yvex_ir_block_at(m, block)->arguments[0];
    }
    for (size_t i = 0u; rc == YVEX_OK && i < 2u; ++i) {
        yvex_ir_attribute a[] = {{.name = "source", .kind = YVEX_IR_ATTR_TEXT},
            {.name = "parameter", .kind = YVEX_IR_ATTR_SYMBOL}};
        memcpy(a[0].value.text, identity, 65u);
        snprintf(a[1].value.text, sizeof(a[1].value.text), "weight_%zu", i);
        yvex_ir_operation_request r = {.operation = "core.parameter", .result_types = types + i + 2u,
            .result_count = 1u, .attributes = a, .attribute_count = 2u};
        rc = yvex_ir_operation_add(m, block, &r, &op, err);
        if (rc == YVEX_OK) {
            args[i + 1u] = yvex_ir_operation_at(m, op)->results[0];
            bindings[i] = (yvex_program_parameter_binding){args[i + 1u], i, YVEX_GGUF_QTYPE_F32};
        }
    }
    yvex_ir_operation_request r = {.operation = norm ? "nn.spatial_group_norm_silu" : "signal.conv2d_slice",
        .operands = args, .operand_count = 3u, .result_types = types + 1u, .result_count = 1u,
        .attributes = norm ? norms : attrs, .attribute_count = norm ? 2u : 8u};
    if (rc == YVEX_OK) rc = yvex_ir_operation_add(m, block, &r, &op, err);
    if (rc == YVEX_OK) {
        yvex_ir_id result = yvex_ir_operation_at(m, op)->results[0];
        r = (yvex_ir_operation_request){.operation = "core.return", .operands = &result, .operand_count = 1u};
        rc = yvex_ir_operation_add(m, block, &r, &op, err);
    }
    if (rc == YVEX_OK) rc = yvex_ir_seal(m, err);
    if (rc == YVEX_OK) rc = yvex_program_execution_compile(&e, m, err);
    if (rc == YVEX_OK) rc = yvex_program_physical_compile(out, e, "forward", bindings, 2u, identity, err);
    yvex_program_execution_close(&e); yvex_ir_module_close(&m);
    return rc;
}

static double spatial_reference(const float *x, int norm, size_t batch, size_t channel, size_t h, size_t w)
{
    if (!norm) {
        double value = (double)channel / 8.0;
        for (size_t c = 0u; c < 2u; ++c) for (int k = 0; k < 3; ++k) {
            int y = (int)h + k - 1;
            if (y < 0) y = -y;
            if (y >= 3) y = 4 - y;
            value += x[((batch * 2u + c) * 3u + (size_t)y) * 4u + w] *
                (double)((channel + 1u) * (c + 1u)) / 8.0;
        }
        return value;
    }
    double mean = 0.0, variance = 0.0;
    for (size_t i = 0u; i < 24u; ++i) mean += x[batch * 24u + i] / 24.0;
    for (size_t i = 0u; i < 24u; ++i) {
        double d = x[batch * 24u + i] - mean; variance += d * d / 24.0;
    }
    double y = (x[((batch * 2u + channel) * 3u + h) * 4u + w] - mean) / sqrt(variance + 1e-6);
    y = y * (double)(channel + 1u) + (channel ? -0.5 : 0.25);
    return y / (1.0 + exp(-y));
}

static int spatial_cancel(void *context) { (void)context; return 1; }

static int test_spatial_programs(yvex_backend_kind kind)
{
    yvex_error err;
    if (kind == YVEX_BACKEND_KIND_CPU) {
        for (int norm = 0; norm < 2; ++norm) for (unsigned int bad = 1u; bad <= 7u; ++bad) {
            yvex_program_physical *p = NULL;
            YVEX_TEST_ASSERT(spatial_fixture_compile(&p, norm, bad, &err) == YVEX_ERR_FORMAT && !p,
                "spatial type, shape, group, plane, padding and stride errors fail before execution");
        }
        printf("Spatial compiler negatives: 14 malformed programs refused before binding\n");
    }
    yvex_backend *backend = NULL;
    yvex_backend_options options = {.kind = kind};
    YVEX_TEST_ASSERT(yvex_backend_open(&backend, &options, &err) == YVEX_OK, "spatial backend opens");
    for (int norm = 0; norm < 2; ++norm) {
        yvex_program_physical *p = NULL, *decoded = NULL;
        yvex_program_stage *stage = NULL;
        yvex_device_tensor *resident = NULL;
        yvex_core_bytes wire = {.maximum = 1048576u};
        YVEX_TEST_ASSERT(spatial_fixture_compile(&p, norm, 0u, &err) == YVEX_OK, "spatial program compiles");
        YVEX_TEST_ASSERT(yvex_program_physical_encode(p, &wire, &err) == YVEX_OK &&
            yvex_program_physical_decode(&decoded, wire.data, wire.count, &err) == YVEX_OK &&
            !strcmp(yvex_program_physical_summary_get(p)->identity,
                yvex_program_physical_summary_get(decoded)->identity), "spatial binary identity is stable");
        float weights[2][36] = {{0}}, input[48], output[72];
        if (norm) { weights[0][0] = 1.0f; weights[0][1] = 2.0f; weights[1][0] = 0.25f; weights[1][1] = -0.5f; }
        else for (size_t o = 0u; o < 3u; ++o) {
            weights[1][o] = (float)o / 8.0f;
            for (size_t c = 0u; c < 2u; ++c) for (size_t k = 0u; k < 3u; ++k) {
                weights[0][(o * 2u + c) * 6u + k] = 99.0f;
                weights[0][(o * 2u + c) * 6u + 3u + k] = (float)((o + 1u) * (c + 1u)) / 8.0f;
            }
        }
        unsigned char *arena = (unsigned char *)weights;
        if (kind == YVEX_BACKEND_KIND_CUDA) {
            yvex_backend_tensor_desc d = {.name = "spatial-parameters", .dtype = YVEX_DTYPE_I8,
                .rank = 1u, .dims = {sizeof(weights)}, .bytes = sizeof(weights)};
            arena = NULL;
            YVEX_TEST_ASSERT(yvex_backend_resident_alloc(backend, &d, &resident, &arena, &err) == YVEX_OK,
                "spatial resident arena allocation");
            memcpy(arena, weights, sizeof(weights));
            YVEX_TEST_ASSERT(yvex_backend_resident_attach(backend, arena, d.bytes, resident, 1u, &err) == YVEX_OK,
                "spatial residency attaches");
        }
        yvex_program_kernel_parameter parameters[2] = {0};
        for (size_t i = 0u; i < 2u; ++i) {
            unsigned long long count = norm ? 2u : i ? 3u : 36u, width = norm ? 2u : i ? 3u : 1u;
            parameters[i] = (yvex_program_kernel_parameter){.tensor_id = i,
                .weight = {.encoded = arena + i * sizeof(weights[0]), .encoded_bytes = count * sizeof(float),
                    .qtype = YVEX_GGUF_QTYPE_F32, .row_count = count / width,
                    .row_width = width, .row_bytes = width * sizeof(float)}};
        }
        int rc = yvex_program_stage_open(&stage, decoded, parameters, 2u, backend, 2u, 1,
            1048576u, 1048576u, &err);
        if (kind == YVEX_BACKEND_KIND_CPU) {
            YVEX_TEST_ASSERT(rc == YVEX_ERR_UNSUPPORTED && !stage, "unimplemented CPU spatial kernel fails closed");
        } else {
            if (rc != YVEX_OK) fprintf(stderr, "spatial prepare: %s\n", yvex_error_message(&err));
            YVEX_TEST_ASSERT(rc == YVEX_OK, "CUDA spatial program prepares");
            for (size_t i = 0u; i < 48u; ++i) input[i] = (float)((int)i - 24) / 8.0f;
            const float *inputs[] = {input}; float *outputs[] = {output};
            yvex_backend_operation_facts facts;
            double maximum = 0.0, tolerance = norm ? 2e-6 : 0.0;
            size_t channels = norm ? 2u : 3u;
            for (size_t run = 0u; run < 2u; ++run) {
                YVEX_TEST_ASSERT(yvex_program_stage_host(stage, 2u, inputs, 1u, outputs, 1u,
                    NULL, NULL, &facts, &err) == YVEX_OK, "spatial compiled program executes");
                for (size_t b = 0u; b < 2u; ++b) for (size_t c = 0u; c < channels; ++c)
                    for (size_t h = 0u; h < 3u; ++h) for (size_t w = 0u; w < 4u; ++w) {
                        double actual = output[((b * channels + c) * 3u + h) * 4u + w];
                        double error = fabs(actual - spatial_reference(input, norm, b, c, h, w));
                        if (error > maximum) maximum = error;
                        YVEX_TEST_ASSERT(isfinite(actual) && error <= tolerance, "independent spatial scalar oracle");
                    }
            }
            for (size_t i = 0u; i < 72u; ++i) output[i] = -77.0f;
            YVEX_TEST_ASSERT(yvex_program_stage_host(stage, 2u, inputs, 1u, outputs, 1u,
                spatial_cancel, NULL, &facts, &err) == YVEX_ERR_CANCELLED, "spatial cancellation refuses publication");
            for (size_t i = 0u; i < 72u; ++i) YVEX_TEST_ASSERT(output[i] == -77.0f, "all publication sentinels retained");
            printf("Spatial CUDA %s values=%zu repeats=2 max_abs=%.12g tolerance=%.9g cancellation=unpublished\n",
                norm ? "group_norm_silu" : "conv2d_slice", 24u * channels, maximum, tolerance);
            YVEX_TEST_ASSERT(yvex_program_stage_close(&stage, &err) == YVEX_OK, "spatial stage closes");
            YVEX_TEST_ASSERT(yvex_backend_resident_detach(backend, &err) == YVEX_OK &&
                yvex_backend_tensor_release(backend, &resident, &err) == YVEX_OK, "spatial residency closes");
        }
        yvex_program_physical_close(&p); yvex_program_physical_close(&decoded); free(wire.data);
    }
    YVEX_TEST_ASSERT(yvex_backend_close_checked(&backend, &err) == YVEX_OK, "spatial resources reclaimed");
    return 0;
}
#endif
