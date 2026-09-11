/* Shared CPU/CUDA proof of physical representation versus result precision. */
#ifndef TESTS_SUPPORT_LINEAR_PROGRAM_H
#define TESTS_SUPPORT_LINEAR_PROGRAM_H
#include "tests/test.h"
#include <yvex/internal/program_kernels.h>
#include <yvex/qtype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int test_linear_compile(yvex_program_physical **out, unsigned int qtype,
    yvex_ir_scalar input_scalar, yvex_ir_scalar output_scalar, yvex_error *err)
{
    const char *source = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    yvex_ir_dialect dialects[] = {*yvex_ir_core_dialect(), *yvex_ir_neural_dialect()};
    yvex_ir_module *m = NULL;
    yvex_program_execution *execution = NULL;
    yvex_ir_dimension rows = {.name = "rows", .minimum = 1u, .maximum = 3u, .multiple = 1u};
    yvex_ir_type t = {.kind = YVEX_IR_TENSOR, .rank = 2u};
    yvex_ir_id dimension, types[3], function, block = YVEX_IR_NONE, op, args[2], result;
    yvex_program_parameter_binding binding = {0};
    int rc = yvex_ir_module_open(&m, "linear", source, dialects, 2u, err);
    if (rc == YVEX_OK) rc = yvex_ir_dimension_add(m, &rows, &dimension, err);
    if (rc == YVEX_OK) {
        t.scalar = input_scalar;
        t.shape[0] = (yvex_ir_extent){dimension, 0u};
        t.shape[1] = (yvex_ir_extent){YVEX_IR_NONE, 32u};
        rc = yvex_ir_type_intern(m, &t, &types[0], err);
    }
    if (rc == YVEX_OK) {
        t.scalar = output_scalar;
        t.shape[1].extent = 3u;
        rc = yvex_ir_type_intern(m, &t, &types[2], err);
    }
    if (rc == YVEX_OK) {
        t.scalar = YVEX_IR_BF16;
        t.shape[0] = (yvex_ir_extent){YVEX_IR_NONE, 3u};
        t.shape[1].extent = 32u;
        rc = yvex_ir_type_intern(m, &t, &types[1], err);
    }
    if (rc == YVEX_OK) rc = yvex_ir_function_add(m, "output", types, 1u, &types[2], 1u, 0u, &function, err);
    if (rc == YVEX_OK) {
        yvex_ir_attribute attrs[] = {{.name = "parameter", .kind = YVEX_IR_ATTR_SYMBOL,
            .value.text = "weight"}, {.name = "source", .kind = YVEX_IR_ATTR_TEXT}};
        yvex_ir_operation_request r = {.operation = "core.parameter", .result_types = &types[1],
            .result_count = 1u, .attributes = attrs, .attribute_count = 2u};
        memcpy(attrs[1].value.text, source, 65u);
        block = yvex_ir_function_at(m, function)->body;
        args[0] = yvex_ir_block_at(m, block)->arguments[0];
        rc = yvex_ir_operation_add(m, block, &r, &op, err);
    }
    if (rc == YVEX_OK) {
        args[1] = yvex_ir_operation_at(m, op)->results[0];
        binding = (yvex_program_parameter_binding){args[1], 7u, qtype};
        yvex_ir_operation_request r = {.operation = "nn.linear", .operands = args, .operand_count = 2u,
            .result_types = &types[2], .result_count = 1u};
        rc = yvex_ir_operation_add(m, block, &r, &op, err);
    }
    if (rc == YVEX_OK) {
        result = yvex_ir_operation_at(m, op)->results[0];
        yvex_ir_operation_request r = {.operation = "core.return", .operands = &result, .operand_count = 1u};
        rc = yvex_ir_operation_add(m, block, &r, &op, err);
    }
    if (rc == YVEX_OK) rc = yvex_ir_seal(m, err);
    if (rc == YVEX_OK) rc = yvex_program_execution_compile(&execution, m, err);
    if (rc == YVEX_OK) rc = yvex_program_physical_compile(out, execution, "output", &binding, 1u, source, err);
    yvex_program_execution_close(&execution);
    yvex_ir_module_close(&m);
    return rc;
}

static int test_linear_invoke(void *context, const yvex_program_device_invocation *r,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    return yvex_program_kernels_invoke(context, r, facts, err);
}

static int test_linear_cancel(void *context)
{
    unsigned int *calls = context;
    return ++*calls >= 2u;
}

static int test_linear_execute(yvex_backend_kind kind)
{
    static const yvex_program_device_kernel implementation = {"linear.encoded.f32.v1", test_linear_invoke};
    yvex_backend_options options = {.kind = kind};
    yvex_backend *backend = NULL;
    yvex_program_physical *p = NULL, *alternate = NULL, *copy = NULL;
    yvex_program_device *vm = NULL;
    yvex_program_kernels *kernels = NULL, *rejected = NULL;
    yvex_backend_tensor_desc d = {.name = "linear-input", .dtype = YVEX_DTYPE_F32, .rank = 2u,
        .dims = {3u, 32u}, .bytes = 96u * sizeof(float)};
    yvex_device_tensor *input = NULL, *output = NULL, *resident = NULL;
    yvex_program_kernel_parameter parameter = {.tensor_id = 7u};
    yvex_program_device_argument argument = {0};
    yvex_program_device_result result;
    yvex_core_bytes bytes = {.maximum = 65536u};
    yvex_backend_memory_stats before, after;
    yvex_error err = {0};
    unsigned char encoded[192] = {0}, *mapped = encoded;
    float x[96] = {0}, actual[9], maximum = 0.0f;
    const unsigned int positions[] = {0u, 33u, 64u, 65u, 66u, 67u};
    const unsigned short weights[] = {0x3f80u, 0x4000u, 0x3f80u, 0xbf80u, 0x4000u, 0x3f00u};
    size_t i;
    unsigned int calls = 0u;
    int rc;
    YVEX_TEST_ASSERT(test_linear_compile(&p, YVEX_GGUF_QTYPE_BF16, YVEX_IR_F32, YVEX_IR_F32, &err) == YVEX_OK &&
        test_linear_compile(&alternate, YVEX_GGUF_QTYPE_F32, YVEX_IR_F32, YVEX_IR_F32, &err) == YVEX_OK,
        "same semantic program admits distinct physical weight representations");
    YVEX_TEST_ASSERT(!strcmp(yvex_program_physical_summary_get(p)->semantic_identity,
        yvex_program_physical_summary_get(alternate)->semantic_identity) &&
        strcmp(yvex_program_physical_summary_get(p)->identity, yvex_program_physical_summary_get(alternate)->identity),
        "physical precision changes physical identity, not semantic identity");
    YVEX_TEST_ASSERT(yvex_program_physical_encode(p, &bytes, &err) == YVEX_OK &&
        yvex_program_physical_decode(&copy, bytes.data, bytes.count, &err) == YVEX_OK &&
        !strcmp(yvex_program_physical_summary_get(copy)->identity, yvex_program_physical_summary_get(p)->identity),
        "F32 publication survives physical binary admission");
    yvex_program_physical_close(&alternate);
    YVEX_TEST_ASSERT(test_linear_compile(&alternate, YVEX_GGUF_QTYPE_BF16, YVEX_IR_F32, YVEX_IR_BF16, &err) !=
        YVEX_OK && !alternate, "implicit F32 input narrowing is not admitted");
    YVEX_TEST_ASSERT(yvex_backend_open(&backend, &options, &err) == YVEX_OK &&
        yvex_backend_get_memory_stats(backend, &before, &err) == YVEX_OK, "open independent linear backend");
    if (kind == YVEX_BACKEND_KIND_CUDA) {
        yvex_backend_tensor_desc weight = {.name = "linear-weight", .dtype = YVEX_DTYPE_I8,
            .rank = 1u, .dims = {192u}, .bytes = 192u};
        YVEX_TEST_ASSERT(yvex_backend_resident_alloc(backend, &weight, &resident, &mapped, &err) == YVEX_OK &&
            yvex_backend_resident_attach(backend, mapped, 192u, resident, 1u, &err) == YVEX_OK,
            "exact encoded parameter residency");
        memset(mapped, 0, 192u);
    }
    for (i = 0u; i < 6u; ++i) {
        mapped[2u * positions[i]] = (unsigned char)weights[i];
        mapped[2u * positions[i] + 1u] = (unsigned char)(weights[i] >> 8u);
    }
    for (i = 0u; i < 3u; ++i) {
        x[32u * i] = 1.001953125f + (float)i;
        x[32u * i + 1u] = 2.0f; x[32u * i + 2u] = -1.0f; x[32u * i + 3u] = 0.5f;
    }
    parameter.weight = (yvex_component_encoded_weight){.encoded = mapped, .encoded_bytes = 192u,
        .row_width = 32u, .row_count = 3u, .row_bytes = 64u, .qtype = YVEX_GGUF_QTYPE_BF16};
    YVEX_TEST_ASSERT(yvex_backend_tensor_alloc(backend, &d, &input, &err) == YVEX_OK &&
        yvex_backend_tensor_write(backend, input, x, sizeof(x), &err) == YVEX_OK, "bind non-BF16 F32 inputs");
    d.dims[1] = 3u; d.bytes = sizeof(actual);
    YVEX_TEST_ASSERT(yvex_backend_tensor_alloc(backend, &d, &output, &err) == YVEX_OK &&
        yvex_program_kernels_open(&kernels, p, &parameter, 1u, backend, 0u, 0u, &err) == YVEX_OK &&
        yvex_program_device_open(&vm, p, backend, 3u, 0u, 0u, &implementation, 1u, kernels, &err) == YVEX_OK,
        "linear program binds without unrelated neural backend requirements");
    argument.tensor = input;
    rc = yvex_program_device_run(vm, 3u, &argument, 1u, &output, 1u, NULL, NULL, &result, &err);
    if (rc != YVEX_OK) fprintf(stderr, "linear program: %s\n", yvex_error_message(&err));
    YVEX_TEST_ASSERT(rc == YVEX_OK && result.operations == 1u && output->is_written &&
        yvex_backend_tensor_read(backend, output, actual, sizeof(actual), &err) == YVEX_OK, "actual SSA execution");
    for (i = 0u; i < 9u; ++i) {
        float first = x[(i / 3u) * 32u];
        float expected = i % 3u == 0u ? first : i % 3u == 1u ? 4.0f : first - 3.75f;
        float error = fabsf(expected - actual[i]);
        if (error > maximum) maximum = error;
        if (!isfinite(actual[i]) || error != 0.0f)
            fprintf(stderr, "linear oracle backend=%s value=%zu expected=%.9g observed=%.9g max_abs=%.9g\n",
                kind == YVEX_BACKEND_KIND_CPU ? "CPU" : "CUDA", i,
                (double)expected, (double)actual[i], (double)maximum);
        YVEX_TEST_ASSERT(isfinite(actual[i]) && error == 0.0f, "independent sparse linear oracle, no BF16 output rounding");
    }
    YVEX_TEST_ASSERT(yvex_program_device_run(vm, 3u, &argument, 1u, &output, 1u,
        test_linear_cancel, &calls, &result, &err) == YVEX_ERR_CANCELLED && !output->is_written,
        "mid-operation CPU or post-operation CUDA cancellation publishes no output");
    x[0] = NAN;
    YVEX_TEST_ASSERT(yvex_backend_tensor_write(backend, input, x, sizeof(x), &err) == YVEX_OK &&
        yvex_program_device_run(vm, 3u, &argument, 1u, &output, 1u, NULL, NULL, &result, &err) ==
            YVEX_ERR_FORMAT && !output->is_written,
        "nonfinite encoded projection refuses publication on both backends");
    x[0] = 1.001953125f;
    YVEX_TEST_ASSERT(yvex_backend_tensor_write(backend, input, x, sizeof(x), &err) == YVEX_OK &&
        yvex_program_device_run(vm, 3u, &argument, 1u, &output, 1u, NULL, NULL, &result, &err) == YVEX_OK &&
        output->is_written, "cancelled or refused invocation does not poison reusable execution");
    parameter.weight.row_bytes--;
    YVEX_TEST_ASSERT(yvex_program_kernels_open(&rejected, p, &parameter, 1u, backend, 0u, 0u, &err) ==
        YVEX_ERR_FORMAT && !rejected, "block geometry mismatch fails at parameter binding");
    YVEX_TEST_ASSERT(yvex_program_device_close(&vm, &err) == YVEX_OK &&
        yvex_program_kernels_close(&kernels, &err) == YVEX_OK &&
        yvex_backend_tensor_release(backend, &input, &err) == YVEX_OK &&
        yvex_backend_tensor_release(backend, &output, &err) == YVEX_OK, "release program resources");
    if (resident) YVEX_TEST_ASSERT(yvex_backend_resident_detach(backend, &err) == YVEX_OK &&
        yvex_backend_tensor_release(backend, &resident, &err) == YVEX_OK, "release parameter residency");
    YVEX_TEST_ASSERT(yvex_backend_get_memory_stats(backend, &after, &err) == YVEX_OK &&
        before.allocated_bytes == after.allocated_bytes && yvex_backend_close_checked(&backend, &err) == YVEX_OK,
        "all allocations return to the exact baseline");
    printf("Linear program %s: independent sparse oracle values=9 first expected=1.001953125 observed=%.9g "
        "max_abs=%g tolerance=0; cancellation=unpublished; allocation_delta=0\n",
        kind == YVEX_BACKEND_KIND_CPU ? "CPU" : "CUDA", (double)actual[0], (double)maximum);
    yvex_program_physical_close(&copy);
    yvex_program_physical_close(&p);
    free(bytes.data);
    return 0;
}
#endif
