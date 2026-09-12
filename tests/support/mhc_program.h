/* Independent mHC equations exercise the actual physical SSA host/device runner. */
#ifndef TESTS_SUPPORT_MHC_PROGRAM_H
#define TESTS_SUPPORT_MHC_PROGRAM_H
#include "tests/test.h"
#include <yvex/internal/program_stage.h>
#include <yvex/internal/quant_numeric.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static int test_mhc_compile(yvex_program_physical **out, unsigned int negative, yvex_error *err)
{
    const char *identity = "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789";
    yvex_ir_dialect dialects[] = {*yvex_ir_core_dialect(), *yvex_ir_neural_dialect()};
    yvex_ir_module *m = NULL;
    yvex_program_execution *e = NULL;
    yvex_ir_dimension rows = {.name = "rows", .minimum = 1u, .maximum = 3u, .multiple = 1u};
    yvex_ir_type t = {.kind = YVEX_IR_TENSOR, .scalar = YVEX_IR_F32, .rank = 3u};
    yvex_ir_id dim, types[6], block = YVEX_IR_NONE, function, op, args[5], result[2];
    yvex_program_parameter_binding bindings[4];
    yvex_ir_attribute attrs[] = {{.name = "epsilon", .kind = YVEX_IR_ATTR_F64, .value.real = 1e-5},
        {.name = "mhc_epsilon", .kind = YVEX_IR_ATTR_F64, .value.real = 1e-6}};
    unsigned int i;
    int rc = yvex_ir_module_open(&m, "mhc_oracle", identity, dialects, 2u, err);
    if (out) *out = NULL;
    if (rc == YVEX_OK) rc = yvex_ir_dimension_add(m, &rows, &dim, err);
    if (rc == YVEX_OK) {
        t.shape[0] = (yvex_ir_extent){dim, 0u};
        t.shape[1] = t.shape[2] = (yvex_ir_extent){YVEX_IR_NONE, 2u};
        rc = yvex_ir_type_intern(m, &t, &types[0], err);
    }
    for (i = 0u; rc == YVEX_OK && i < 4u; ++i) {
        t = (yvex_ir_type){.kind = YVEX_IR_TENSOR, .scalar = YVEX_IR_F32, .rank = i ? 1u : 2u};
        t.shape[0] = (yvex_ir_extent){YVEX_IR_NONE, i == 2u ? 1u : 2u};
        if (!i) t.shape[1] = (yvex_ir_extent){YVEX_IR_NONE, negative == 1u ? 5u : 4u};
        if (negative == 4u && i == 1u) t.scalar = YVEX_IR_BF16;
        rc = yvex_ir_type_intern(m, &t, &types[i + 1u], err);
    }
    if (rc == YVEX_OK) {
        t = (yvex_ir_type){.kind = YVEX_IR_TENSOR, .scalar = negative == 2u ? YVEX_IR_F32 : YVEX_IR_BF16, .rank = 2u};
        t.shape[0] = (yvex_ir_extent){dim, 0u}; t.shape[1] = (yvex_ir_extent){YVEX_IR_NONE, 2u};
        rc = yvex_ir_type_intern(m, &t, &types[5], err);
    }
    if (rc == YVEX_OK) {
        result[0] = result[1] = types[5];
        rc = yvex_ir_function_add(m, "final", types, 1u, result, 2u, 0u, &function, err);
    }
    if (rc == YVEX_OK) { block = yvex_ir_function_at(m, function)->body; args[0] = yvex_ir_block_at(m, block)->arguments[0]; }
    for (i = 0u; rc == YVEX_OK && i < 4u; ++i) {
        yvex_ir_attribute p[] = {{.name = "parameter", .kind = YVEX_IR_ATTR_SYMBOL},
            {.name = "source", .kind = YVEX_IR_ATTR_TEXT}};
        yvex_ir_operation_request r = {.operation = "core.parameter", .result_types = &types[i + 1u],
            .result_count = 1u, .attributes = p, .attribute_count = 2u};
        (void)snprintf(p[0].value.text, sizeof(p[0].value.text), "weight_%u", i);
        memcpy(p[1].value.text, identity, 65u);
        rc = yvex_ir_operation_add(m, block, &r, &op, err);
        if (rc == YVEX_OK) {
            args[i + 1u] = yvex_ir_operation_at(m, op)->results[0];
            bindings[i] = (yvex_program_parameter_binding){args[i + 1u], 32u + i, YVEX_GGUF_QTYPE_F32};
        }
    }
    if (rc == YVEX_OK) {
        if (negative == 3u) attrs[1].value.real = 0.0;
        yvex_ir_operation_request r = {.operation = "mhc.head_norm", .operands = args, .operand_count = 5u,
            .result_types = result, .result_count = 2u, .attributes = attrs, .attribute_count = 2u};
        rc = yvex_ir_operation_add(m, block, &r, &op, err);
    }
    if (rc == YVEX_OK) {
        result[0] = yvex_ir_operation_at(m, op)->results[0];
        result[1] = yvex_ir_operation_at(m, op)->results[negative == 5u ? 0u : 1u];
        yvex_ir_operation_request r = {.operation = "core.return", .operands = result, .operand_count = 2u};
        rc = yvex_ir_operation_add(m, block, &r, &op, err);
    }
    if (rc == YVEX_OK) rc = yvex_ir_seal(m, err);
    if (rc == YVEX_OK) rc = yvex_program_execution_compile(&e, m, err);
    if (rc == YVEX_OK) rc = yvex_program_physical_compile(out, e, "final", bindings, 4u, identity, err);
    yvex_program_execution_close(&e); yvex_ir_module_close(&m);
    return rc;
}

static int test_stream_mean_compile(yvex_program_physical **out, unsigned int negative, yvex_error *err)
{
    const char *identity = "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789";
    yvex_ir_dialect dialects[] = {*yvex_ir_core_dialect(), *yvex_ir_neural_dialect()};
    yvex_ir_module *m = NULL;
    yvex_program_execution *execution = NULL;
    yvex_ir_dimension rows = {.name = "rows", .minimum = 1u, .maximum = 3u, .multiple = 1u};
    yvex_ir_type t = {.kind = YVEX_IR_TENSOR, .scalar = YVEX_IR_F32, .rank = 3u};
    yvex_ir_id dim, input, result, function, block = YVEX_IR_NONE, op;
    int rc = yvex_ir_module_open(&m, "feature_oracle", identity, dialects, 2u, err);
    if (out) *out = NULL;
    if (rc == YVEX_OK) rc = yvex_ir_dimension_add(m, &rows, &dim, err);
    if (rc == YVEX_OK) {
        t.shape[0] = (yvex_ir_extent){dim, 0u};
        t.shape[1] = (yvex_ir_extent){YVEX_IR_NONE, 3u};
        t.shape[2] = (yvex_ir_extent){YVEX_IR_NONE, 2u};
        if (negative == 3u) t.shape[1] = t.shape[0];
        rc = yvex_ir_type_intern(m, &t, &input, err);
    }
    if (rc == YVEX_OK) {
        t = (yvex_ir_type){.kind = YVEX_IR_TENSOR, .scalar = YVEX_IR_F32, .rank = 2u};
        t.shape[0] = (yvex_ir_extent){dim, 0u};
        t.shape[1] = (yvex_ir_extent){YVEX_IR_NONE, negative == 1u ? 4u : 2u};
        if (negative == 2u) t.scalar = YVEX_IR_BF16;
        if (negative == 4u) t.shape[0] = (yvex_ir_extent){YVEX_IR_NONE, 3u};
        rc = yvex_ir_type_intern(m, &t, &result, err);
    }
    if (rc == YVEX_OK) rc = yvex_ir_function_add(m, "feature", &input, 1u, &result, 1u, 0u, &function, err);
    if (rc == YVEX_OK) {
        block = yvex_ir_function_at(m, function)->body;
        yvex_ir_operation_request r = {.operation = "tensor.stream_mean",
            .operands = yvex_ir_block_at(m, block)->arguments, .operand_count = 1u,
            .result_types = &result, .result_count = 1u};
        rc = yvex_ir_operation_add(m, block, &r, &op, err);
    }
    if (rc == YVEX_OK) {
        yvex_ir_id value = yvex_ir_operation_at(m, op)->results[0];
        yvex_ir_operation_request r = {.operation = "core.return", .operands = &value, .operand_count = 1u};
        rc = yvex_ir_operation_add(m, block, &r, &op, err);
    }
    if (rc == YVEX_OK) rc = yvex_ir_seal(m, err);
    if (rc == YVEX_OK) rc = yvex_program_execution_compile(&execution, m, err);
    if (rc == YVEX_OK) rc = yvex_program_physical_compile(out, execution, "feature", NULL, 0u, identity, err);
    yvex_program_execution_close(&execution); yvex_ir_module_close(&m);
    return rc;
}

static int test_mhc_cancel(void *opaque) { (void)opaque; return 1; }
static int test_mhc_cancel_publication(void *opaque) { return ++*(unsigned int *)opaque == 3u; }

static int test_post_compile(yvex_program_physical **out, unsigned int negative, yvex_error *err)
{
    const char *identity = "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789";
    yvex_ir_dialect dialects[] = {*yvex_ir_core_dialect(), *yvex_ir_neural_dialect()};
    yvex_ir_module *m = NULL;
    yvex_program_execution *execution = NULL;
    yvex_ir_dimension rows = {.name = "rows", .minimum = 1u, .maximum = 3u, .multiple = 1u};
    yvex_ir_id dim, inputs[4], output, function, block = YVEX_IR_NONE, op;
    int rc = yvex_ir_module_open(&m, "post_oracle", identity, dialects, 2u, err);
    if (out) *out = NULL;
    if (rc == YVEX_OK) rc = yvex_ir_dimension_add(m, &rows, &dim, err);
    for (size_t i = 0u; rc == YVEX_OK && i < 4u; ++i) {
        yvex_ir_type t = {.kind = YVEX_IR_TENSOR, .scalar = YVEX_IR_F32,
            .rank = i == 0u || i == 3u ? 3u : 2u};
        t.shape[0] = (yvex_ir_extent){dim, 0u};
        t.shape[1] = (yvex_ir_extent){YVEX_IR_NONE, 2u};
        if (t.rank == 3u) t.shape[2] = t.shape[1];
        if (negative == 1u && i == 1u) t.shape[1].extent = 3u;
        if (negative == 2u && i == 3u) t.shape[2].extent = 3u;
        if (negative == 3u && i == 2u) t.shape[0] = (yvex_ir_extent){YVEX_IR_NONE, 3u};
        if (negative == 4u && i == 3u) t.scalar = YVEX_IR_BF16;
        rc = yvex_ir_type_intern(m, &t, &inputs[i], err);
        if (rc == YVEX_OK && i == 0u) {
            t.scalar = negative == 5u ? YVEX_IR_F32 : YVEX_IR_BF16;
            rc = yvex_ir_type_intern(m, &t, &output, err);
        }
    }
    if (rc == YVEX_OK) rc = yvex_ir_function_add(m, "post", inputs, 4u, &output, 1u, 0u, &function, err);
    if (rc == YVEX_OK) {
        block = yvex_ir_function_at(m, function)->body;
        yvex_ir_operation_request r = {.operation = "mhc.residual_post",
            .operands = yvex_ir_block_at(m, block)->arguments, .operand_count = 4u,
            .result_types = &output, .result_count = 1u};
        rc = yvex_ir_operation_add(m, block, &r, &op, err);
    }
    if (rc == YVEX_OK) {
        yvex_ir_id result = yvex_ir_operation_at(m, op)->results[0];
        yvex_ir_operation_request r = {.operation = "core.return", .operands = &result, .operand_count = 1u};
        rc = yvex_ir_operation_add(m, block, &r, &op, err);
    }
    if (rc == YVEX_OK) rc = yvex_ir_seal(m, err);
    if (rc == YVEX_OK) rc = yvex_program_execution_compile(&execution, m, err);
    if (rc == YVEX_OK) rc = yvex_program_physical_compile(out, execution, "post", NULL, 0u, identity, err);
    yvex_program_execution_close(&execution); yvex_ir_module_close(&m);
    return rc;
}

static int test_post_device(yvex_program_stage *stage, yvex_backend *backend,
    const float *const *inputs, const float *expected)
{
    const unsigned long long counts[] = {12u, 6u, 6u, 12u};
    yvex_device_tensor *buffers[4] = {0}, *output = NULL;
    const yvex_device_tensor *arguments[4];
    yvex_backend_tensor_desc d = {.name = "post-device", .dtype = YVEX_DTYPE_F32,
        .rank = 3u, .dims = {3u, 2u, 2u}, .bytes = 12u * sizeof(float)};
    yvex_backend_operation_facts facts;
    yvex_error err = {0};
    float actual[12];
    YVEX_TEST_ASSERT(yvex_backend_tensor_alloc(backend, &d, &output, &err) == YVEX_OK,
        "post result device storage allocates");
    for (size_t i = 0u; i < 4u; ++i) {
        d.rank = i == 0u || i == 3u ? 3u : 2u;
        d.dims[2] = d.rank == 3u ? 2u : 0u;
        d.bytes = counts[i] * sizeof(float);
        YVEX_TEST_ASSERT(yvex_backend_tensor_alloc(backend, &d, &buffers[i], &err) == YVEX_OK &&
            yvex_backend_tensor_write(backend, buffers[i], inputs[i], d.bytes, &err) == YVEX_OK,
            "independent device operand is explicitly initialized");
        arguments[i] = buffers[i];
    }
    YVEX_TEST_ASSERT(yvex_program_stage_device(stage, 3u, arguments, 4u, &output, 1u,
        NULL, NULL, &facts, &err) == YVEX_OK &&
        yvex_backend_tensor_read(backend, output, actual, sizeof(actual), &err) == YVEX_OK &&
        !memcmp(actual, expected, sizeof(actual)), "four borrowed device inputs produce the exact post result");
    YVEX_TEST_ASSERT(yvex_program_stage_device(stage, 3u, arguments, 3u, &output, 1u,
        NULL, NULL, &facts, &err) == YVEX_ERR_INVALID_ARG, "device call cannot omit a dependency");
    arguments[2] = NULL;
    YVEX_TEST_ASSERT(yvex_program_stage_device(stage, 3u, arguments, 4u, &output, 1u,
        NULL, NULL, &facts, &err) != YVEX_OK, "missing device operand refuses before execution");
    arguments[2] = buffers[2];
    YVEX_TEST_ASSERT(yvex_program_stage_device(stage, 3u, arguments, 4u, &buffers[0], 1u,
        NULL, NULL, &facts, &err) == YVEX_ERR_FORMAT, "device result cannot overwrite a borrowed operand");
    for (size_t i = 0u; i < 4u; ++i)
        YVEX_TEST_ASSERT(yvex_backend_tensor_release(backend, &buffers[i], &err) == YVEX_OK,
            "device operand releases");
    YVEX_TEST_ASSERT(yvex_backend_tensor_release(backend, &output, &err) == YVEX_OK, "device result releases");
    return 0;
}

static int test_post_execute(yvex_backend_kind kind)
{
    float residual[] = {1, 2, 3, 4, -1, 2, 3, -4, 16777216, 1, 1, 2};
    float core[] = {5, 6, 2, -2, -16777216, 3}, gates[] = {0.5f, 1, -0.5f, 0.25f, 1, 0};
    float mix[] = {1, 0, 0, 1, 0, 0.5f, 1, 0, 1, 0, 1, 1};
    const float expected[] = {3.5f, 5, 8, 10, 2, -3, 0, 0.5f, 1, 6, 1, 2};
    const float *inputs[] = {residual, core, gates, mix};
    float output[12] = {0}, single[12] = {0}, *outputs[] = {output};
    yvex_program_physical *p = NULL, *copy = NULL, *bad = NULL;
    yvex_program_stage *stage = NULL, *refused = NULL;
    yvex_backend *backend = NULL;
    yvex_backend_options options = {.kind = kind};
    yvex_backend_memory_stats before, after;
    yvex_backend_operation_facts facts;
    yvex_core_bytes bytes = {.maximum = 65536u}, again = {.maximum = 65536u};
    yvex_error err = {0};
    unsigned int i, calls = 0u;
    YVEX_TEST_ASSERT(test_post_compile(&p, 0u, &err) == YVEX_OK, "four-input residual post compiles");
    for (i = 1u; i <= 5u; ++i)
        YVEX_TEST_ASSERT(test_post_compile(&bad, i, &err) != YVEX_OK && !bad,
            "post verifier rejects width, mixing geometry, row identity, operand and result precision");
    YVEX_TEST_ASSERT(yvex_program_physical_encode(p, &bytes, &err) == YVEX_OK &&
        yvex_program_physical_decode(&copy, bytes.data, bytes.count, &err) == YVEX_OK &&
        yvex_program_physical_encode(copy, &again, &err) == YVEX_OK && bytes.count == again.count &&
        !memcmp(bytes.data, again.data, bytes.count), "four-input physical program roundtrips identically");
    YVEX_TEST_ASSERT(yvex_backend_open(&backend, &options, &err) == YVEX_OK &&
        yvex_backend_get_memory_stats(backend, &before, &err) == YVEX_OK, "post backend opens");
    YVEX_TEST_ASSERT(yvex_program_stage_open(&refused, copy, NULL, 0u, backend, 3u, 1, 1u, 1u, &err) != YVEX_OK &&
        yvex_program_stage_close(&refused, &err) == YVEX_OK, "aggregate input preparation budget refuses");
    YVEX_TEST_ASSERT(yvex_program_stage_open(&stage, copy, NULL, 0u, backend, 3u, 1, 0u, 0u, &err) == YVEX_OK,
        "four independent inputs stage once at cold preparation");
    if (test_post_device(stage, backend, inputs, expected) != 0) return 1;
    YVEX_TEST_ASSERT(yvex_program_stage_host(stage, 3u, inputs, 4u, outputs, 1u,
        NULL, NULL, &facts, &err) == YVEX_OK && !memcmp(output, expected, sizeof(output)),
        "post preserves independent source-to-target mixing and ordered F64 accumulation exactly");
    for (i = 0u; i < 3u; ++i) {
        const float *row[] = {residual + 4u * i, core + 2u * i, gates + 2u * i, mix + 4u * i};
        outputs[0] = single + 4u * i;
        YVEX_TEST_ASSERT(yvex_program_stage_host(stage, 1u, row, 4u, outputs, 1u,
            NULL, NULL, &facts, &err) == YVEX_OK, "post single invocation succeeds");
    }
    YVEX_TEST_ASSERT(!memcmp(single, expected, sizeof(single)), "post chunk equals single invocation");
    outputs[0] = output;
    for (i = 0u; i < 12u; ++i) output[i] = 123.0f;
    YVEX_TEST_ASSERT(yvex_program_stage_host(stage, 3u, inputs, 3u, outputs, 1u,
        NULL, NULL, &facts, &err) == YVEX_ERR_INVALID_ARG, "missing input cannot be inferred");
    inputs[2] = NULL;
    YVEX_TEST_ASSERT(yvex_program_stage_host(stage, 3u, inputs, 4u, outputs, 1u,
        NULL, NULL, &facts, &err) == YVEX_ERR_INVALID_ARG, "null independent input refuses");
    inputs[2] = gates;
    YVEX_TEST_ASSERT(yvex_program_stage_host(stage, 3u, inputs, 4u, outputs, 1u,
        test_mhc_cancel_publication, &calls, &facts, &err) == YVEX_ERR_CANCELLED,
        "post cancellation precedes host publication");
    mix[11] = NAN;
    YVEX_TEST_ASSERT(yvex_program_stage_host(stage, 3u, inputs, 4u, outputs, 1u,
        NULL, NULL, &facts, &err) != YVEX_OK, "last-row nonfinite computation refuses");
    for (i = 0u; i < 12u; ++i) YVEX_TEST_ASSERT(output[i] == 123.0f, "no partial host output escapes");
    mix[11] = 1;
    YVEX_TEST_ASSERT(yvex_program_stage_host(stage, 3u, inputs, 4u, outputs, 1u,
        NULL, NULL, &facts, &err) == YVEX_OK && !memcmp(output, expected, sizeof(output)),
        "post runner remains reusable after failed execution");
    YVEX_TEST_ASSERT(yvex_program_stage_close(&stage, &err) == YVEX_OK &&
        yvex_backend_get_memory_stats(backend, &after, &err) == YVEX_OK &&
        after.allocated_bytes == before.allocated_bytes && yvex_backend_close_checked(&backend, &err) == YVEX_OK,
        "four-input runner closes without leaked allocations");
    printf("mHC post %s: inputs=4 outputs=12 expected/observed[0]=3.5 [8]=1 max_abs=0 tolerance=0; "
        "chunk==single; five verifier negatives; late numerical refusal/cancel unpublished; allocation delta=0\n",
        kind == YVEX_BACKEND_KIND_CPU ? "CPU" : "CUDA");
    free(bytes.data); free(again.data);
    yvex_program_physical_close(&copy); yvex_program_physical_close(&p);
    return 0;
}

static int test_stream_mean_execute(yvex_backend_kind kind)
{
    float input[] = {2, 4, 4, 6, 6, 8, -1, 2, -3, 4, -5, 6, 16777216, 1, 1, 2, -16777216, 3};
    const float expected[] = {4, 6, -3, 4, 1.0f / 3.0f, 2};
    float output[6] = {0}, single[6] = {0}, *outputs[] = {output};
    const float *inputs[] = {input};
    yvex_program_physical *p = NULL, *copy = NULL, *bad = NULL;
    yvex_program_stage *stage = NULL, *refused = NULL;
    yvex_backend *backend = NULL;
    yvex_backend_options options = {.kind = kind};
    yvex_backend_memory_stats before, after;
    yvex_backend_operation_facts facts;
    yvex_core_bytes bytes = {.maximum = 65536u}, again = {.maximum = 65536u};
    yvex_error err = {0};
    float maximum = 0.0f;
    unsigned int i, calls = 0u;
    YVEX_TEST_ASSERT(test_stream_mean_compile(&p, 0u, &err) == YVEX_OK, "stream mean compiles to typed work");
    for (i = 1u; i <= 4u; ++i)
        YVEX_TEST_ASSERT(test_stream_mean_compile(&bad, i, &err) != YVEX_OK && !bad,
            "stream mean rejects width, precision, dynamic stream count and inconsistent row population");
    YVEX_TEST_ASSERT(yvex_program_physical_encode(p, &bytes, &err) == YVEX_OK &&
        yvex_program_physical_decode(&copy, bytes.data, bytes.count, &err) == YVEX_OK &&
        yvex_program_physical_encode(copy, &again, &err) == YVEX_OK && bytes.count == again.count &&
        !memcmp(bytes.data, again.data, bytes.count), "stream mean physical identity roundtrips exactly");
    YVEX_TEST_ASSERT(yvex_backend_open(&backend, &options, &err) == YVEX_OK &&
        yvex_backend_get_memory_stats(backend, &before, &err) == YVEX_OK, "stream mean backend opens");
    YVEX_TEST_ASSERT(yvex_program_stage_open(&refused, copy, NULL, 0u, backend, 3u, 1, 1u, 1u, &err) != YVEX_OK &&
        yvex_program_stage_close(&refused, &err) == YVEX_OK, "stream mean insufficient preparation budget refuses");
    YVEX_TEST_ASSERT(yvex_program_stage_open(&stage, copy, NULL, 0u, backend, 3u, 1, 0u, 0u, &err) == YVEX_OK,
        "stream mean reusable physical runner opens");
    YVEX_TEST_ASSERT(yvex_program_stage_host(stage, 3u, inputs, 1u, outputs, 1u, NULL, NULL, &facts, &err) == YVEX_OK,
        "stream mean executes through physical SSA");
    for (i = 0u; i < 6u; ++i) {
        float delta = fabsf(output[i] - expected[i]);
        if (delta > maximum) maximum = delta;
        YVEX_TEST_ASSERT(output[i] == expected[i], "stream mean preserves F64 accumulation without BF16 rounding");
    }
    for (i = 0u; i < 3u; ++i) {
        outputs[0] = single + 2u * i;
        inputs[0] = input + 6u * i;
        YVEX_TEST_ASSERT(yvex_program_stage_host(stage, 1u, inputs, 1u, outputs, 1u,
            NULL, NULL, &facts, &err) == YVEX_OK, "stream mean single-row invocation executes");
    }
    YVEX_TEST_ASSERT(!memcmp(output, single, sizeof(output)), "chunk and single stream reductions are exact");
    outputs[0] = output;
    inputs[0] = input;
    for (i = 0u; i < 6u; ++i) output[i] = 123.0f;
    YVEX_TEST_ASSERT(yvex_program_stage_host(stage, 3u, inputs, 1u, outputs, 1u,
        test_mhc_cancel_publication, &calls, &facts, &err) == YVEX_ERR_CANCELLED,
        "stream mean cancels before publishing the result");
    for (i = 0u; i < 6u; ++i) YVEX_TEST_ASSERT(output[i] == 123.0f, "cancelled stream mean publishes nothing");
    input[0] = NAN;
    YVEX_TEST_ASSERT(yvex_program_stage_host(stage, 3u, inputs, 1u, outputs, 1u, NULL, NULL, &facts, &err) != YVEX_OK,
        "non-finite stream reduction refuses");
    for (i = 0u; i < 6u; ++i) YVEX_TEST_ASSERT(output[i] == 123.0f, "failed reduction publishes nothing");
    input[0] = 2.0f;
    YVEX_TEST_ASSERT(yvex_program_stage_host(stage, 3u, inputs, 1u, outputs, 1u, NULL, NULL, &facts, &err) == YVEX_OK &&
        !memcmp(output, expected, sizeof(output)), "valid reduction recovers after cancellation and numerical refusal");
    YVEX_TEST_ASSERT(yvex_program_stage_close(&stage, &err) == YVEX_OK &&
        yvex_backend_get_memory_stats(backend, &after, &err) == YVEX_OK &&
        after.allocated_bytes == before.allocated_bytes, "stream mean resources return to baseline");
    printf("stream mean %s: outputs=6 mean[4]=%.9g max_abs=%.9g tolerance=0 chunk==single; "
        "4 verifier negatives; cancellation/numeric refusal unpublished; allocation delta=0\n",
        kind == YVEX_BACKEND_KIND_CPU ? "CPU" : "CUDA", output[4], maximum);
    YVEX_TEST_ASSERT(yvex_backend_close_checked(&backend, &err) == YVEX_OK, "stream mean backend closes");
    free(bytes.data); free(again.data);
    yvex_program_physical_close(&copy); yvex_program_physical_close(&p);
    return 0;
}

static int test_mhc_execute(yvex_backend_kind kind)
{
    const float fn[] = {0.125f, -0.25f, 0.5f, 0.75f, -0.5f, 0.25f, -0.125f, 0.5f};
    const float base[] = {-0.125f, 0.25f}, scale[] = {0.5f}, norm[] = {1.0f, 2.0f};
    const float *weights[] = {fn, base, scale, norm};
    const unsigned long long widths[] = {4u, 2u, 1u, 2u}, counts[] = {2u, 1u, 1u, 1u};
    float x[] = {3.5f, 5.0f, 8.0f, 10.0f, 1.0f, -2.0f, 3.0f, -4.0f, 0.5f, 2.0f, 1.0f, -0.5f};
    float actual[6] = {0}, before[6] = {0}, expected[6], pre[6], *outputs[] = {actual, before};
    yvex_program_kernel_parameter parameters[4];
    yvex_program_physical *p = NULL, *copy = NULL, *bad = NULL;
    yvex_program_stage *stage = NULL, *refused = NULL;
    yvex_backend *backend = NULL;
    yvex_backend_options options = {.kind = kind};
    yvex_backend_operation_facts facts;
    yvex_backend_memory_stats initial, final;
    yvex_core_bytes bytes = {.maximum = 65536u}, again = {.maximum = 65536u};
    yvex_error err = {0};
    unsigned int i, row, lane, stream, cancel_calls = 0u;
    float maximum = 0.0f;
    int rc;
    YVEX_TEST_ASSERT(test_mhc_compile(&p, 0u, &err) == YVEX_OK, "typed mHC two-result program compiles");
    for (i = 1u; i <= 5u; ++i)
        YVEX_TEST_ASSERT(test_mhc_compile(&bad, i, &err) != YVEX_OK && !bad,
            "mHC malformed geometry, scalar, epsilon and aliased publication fail during compilation");
    YVEX_TEST_ASSERT(yvex_program_physical_encode(p, &bytes, &err) == YVEX_OK &&
        yvex_program_physical_decode(&copy, bytes.data, bytes.count, &err) == YVEX_OK &&
        yvex_program_physical_encode(copy, &again, &err) == YVEX_OK && bytes.count == again.count &&
        !memcmp(bytes.data, again.data, bytes.count), "mHC canonical physical bytes and two-result identity roundtrip");
    for (i = 0u; i < 4u; ++i)
        parameters[i] = (yvex_program_kernel_parameter){32u + i, {.encoded = (const unsigned char *)weights[i],
            .encoded_bytes = widths[i] * counts[i] * sizeof(float), .qtype = YVEX_GGUF_QTYPE_F32,
            .row_width = widths[i], .row_count = counts[i], .row_bytes = widths[i] * sizeof(float)}};
    for (row = 0u; row < 3u; ++row) {
        double squares = 0.0, inv;
        for (i = 0u; i < 4u; ++i) squares += (double)x[row * 4u + i] * x[row * 4u + i];
        inv = 1.0 / sqrt(squares / 4.0 + 1e-5);
        for (lane = 0u; lane < 2u; ++lane) {
            float sum = 0.0f;
            for (stream = 0u; stream < 2u; ++stream) {
                double dot = 0.0;
                for (i = 0u; i < 4u; ++i) dot += (double)fn[stream * 4u + i] * x[row * 4u + i];
                double gate = 1.0 / (1.0 + exp(-(dot * inv * scale[0] + base[stream]))) + 1e-6;
                sum += (float)(gate * x[row * 4u + stream * 2u + lane]);
            }
            pre[row * 2u + lane] = yvex_quant_bf16_decode(yvex_quant_bf16_encode(sum));
        }
        squares = ((double)pre[row * 2u] * pre[row * 2u] + (double)pre[row * 2u + 1u] * pre[row * 2u + 1u]) / 2.0;
        inv = 1.0 / sqrt(squares + 1e-5);
        for (lane = 0u; lane < 2u; ++lane)
            expected[row * 2u + lane] = yvex_quant_bf16_decode(yvex_quant_bf16_encode(
                (float)((double)pre[row * 2u + lane] * inv * norm[lane])));
    }
    YVEX_TEST_ASSERT(yvex_backend_open(&backend, &options, &err) == YVEX_OK &&
        yvex_backend_get_memory_stats(backend, &initial, &err) == YVEX_OK, "open mHC oracle backend");
    rc = yvex_program_stage_open(&stage, copy, parameters, 4u, backend, 3u, 1, 0u, 0u, &err);
    if (rc != YVEX_OK) fprintf(stderr, "mHC stage open: %s (%s)\n", yvex_error_message(&err), yvex_error_where(&err));
    YVEX_TEST_ASSERT(rc == YVEX_OK, "parameter-bound mHC stage opens on the selected backend");
    rc = yvex_program_stage_host(stage, 3u, (const float *[]){x}, 1u, outputs, 2u, NULL, NULL, &facts, &err);
    if (rc != YVEX_OK) fprintf(stderr, "mHC stage run: %s (%s)\n", yvex_error_message(&err), yvex_error_where(&err));
    YVEX_TEST_ASSERT(rc == YVEX_OK, "actual two-result mHC stage executes");
    for (i = 0u; i < 6u; ++i) {
        float a = fabsf(actual[i] - expected[i]), b = fabsf(before[i] - pre[i]);
        if (a > maximum) maximum = a;
        if (b > maximum) maximum = b;
        YVEX_TEST_ASSERT(isfinite(actual[i]) && isfinite(before[i]) && a == 0.0f && b == 0.0f,
            "both rounding boundaries equal independent full equations exactly");
    }
    actual[0] = before[0] = 123.0f;
    YVEX_TEST_ASSERT(yvex_program_stage_host(stage, 3u, (const float *[]){x}, 1u, outputs, 2u, test_mhc_cancel_publication,
        &cancel_calls, &facts, &err) == YVEX_ERR_CANCELLED && cancel_calls == 3u &&
        actual[0] == 123.0f && before[0] == 123.0f, "cancellation after transfers publishes neither host result");
    outputs[1] = actual + 1u;
    YVEX_TEST_ASSERT(yvex_program_stage_host(stage, 3u, (const float *[]){x}, 1u, outputs, 2u, NULL, NULL, &facts, &err) ==
        YVEX_ERR_FORMAT && actual[0] == 123.0f, "partially overlapping host results refuse before execution");
    outputs[1] = before;
    YVEX_TEST_ASSERT(yvex_program_stage_host(stage, 3u, (const float *[]){x}, 1u, outputs, 2u, test_mhc_cancel, NULL, &facts, &err) ==
        YVEX_ERR_CANCELLED && actual[0] == 123.0f && before[0] == 123.0f, "cancel publishes neither result");
    x[0] = NAN;
    YVEX_TEST_ASSERT(yvex_program_stage_host(stage, 3u, (const float *[]){x}, 1u, outputs, 2u, NULL, NULL, &facts, &err) ==
        YVEX_ERR_FORMAT && actual[0] == 123.0f && before[0] == 123.0f, "nonfinite input publishes neither result");
    x[0] = 3.5f;
    YVEX_TEST_ASSERT(yvex_program_stage_host(stage, 1u, (const float *[]){x}, 1u, outputs, 2u, NULL, NULL, &facts, &err) == YVEX_OK &&
        actual[0] == expected[0] && before[0] == pre[0], "refused stage remains reusable at a smaller population");
    YVEX_TEST_ASSERT(yvex_program_stage_open(&refused, copy, parameters, 4u, backend, 3u, 1, 1u, 0u, &err) ==
        YVEX_ERR_BOUNDS && !refused, "stage admission refuses an insufficient budget");
    YVEX_TEST_ASSERT(yvex_program_stage_close(&stage, &err) == YVEX_OK &&
        yvex_backend_get_memory_stats(backend, &final, &err) == YVEX_OK &&
        initial.allocated_bytes == final.allocated_bytes && yvex_backend_close_checked(&backend, &err) == YVEX_OK,
        "mHC program cleanup restores initial allocation count");
    printf("mHC program %s: outputs=12 normalized[0]=%.9g pre[0]=%.9g max_abs=%g tolerance=0; "
        "compiler_negatives=5 cancellation=unpublished allocation_delta=0\n",
        kind == YVEX_BACKEND_KIND_CPU ? "CPU" : "CUDA", (double)expected[0], (double)pre[0], (double)maximum);
    yvex_program_physical_close(&p); yvex_program_physical_close(&copy);
    free(bytes.data); free(again.data);
    return 0;
}
#endif
