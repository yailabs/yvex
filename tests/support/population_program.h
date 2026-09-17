/* Independent row populations share one verified computational program. */
#ifndef TESTS_SUPPORT_POPULATION_PROGRAM_H
#define TESTS_SUPPORT_POPULATION_PROGRAM_H
#include "tests/test.h"
#include <yvex/internal/program_stage.h>
#include <yvex/internal/quant_numeric.h>
#include <yvex/qtype.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int population_program(yvex_program_physical **out, int bf16, yvex_error *err)
{
    const char *identity = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    yvex_ir_dialect dialects[] = {*yvex_ir_core_dialect(), *yvex_ir_neural_dialect()};
    yvex_ir_module *m = NULL;
    yvex_program_execution *execution = NULL;
    yvex_ir_dimension d = {.name = "patches", .minimum = 8u, .maximum = 8u, .multiple = 1u};
    yvex_ir_type t = {.kind = YVEX_IR_TENSOR, .scalar = bf16 ? YVEX_IR_BF16 : YVEX_IR_F32, .rank = 2u};
    yvex_ir_id dimension, types[6], f, b, op, value, weight;
    yvex_program_parameter_binding binding = {0};
    int rc = yvex_ir_module_open(&m, "populations", identity, dialects, 2u, err);
    if (rc == YVEX_OK) rc = yvex_ir_dimension_add(m, &d, &dimension, err);
    for (size_t i = 0u; rc == YVEX_OK && i < 6u; ++i) {
        const unsigned long long rows[] = {8u, 4u, 4u, 2u, 2u, 32u};
        const unsigned long long widths[] = {32u, 64u, 32u, 64u, 32u, 64u};
        t.shape[0] = (yvex_ir_extent){i ? YVEX_IR_NONE : dimension, i ? rows[i] : 0u};
        t.shape[1] = (yvex_ir_extent){YVEX_IR_NONE, widths[i]};
        rc = yvex_ir_type_intern(m, &t, &types[i], err);
    }
    if (rc == YVEX_OK) rc = yvex_ir_function_add(m, "forward", types, 1u, types + 4u, 1u, 0u, &f, err);
    if (rc == YVEX_OK) {
        yvex_ir_attribute attrs[] = {{.name = "source", .kind = YVEX_IR_ATTR_TEXT},
            {.name = "parameter", .kind = YVEX_IR_ATTR_SYMBOL, .value.text = "projection"}};
        memcpy(attrs[0].value.text, identity, 65u);
        b = yvex_ir_function_at(m, f)->body;
        value = yvex_ir_block_at(m, b)->arguments[0];
        yvex_ir_operation_request r = {.operation = "core.parameter", .result_types = types + 5u,
            .result_count = 1u, .attributes = attrs, .attribute_count = 2u};
        rc = yvex_ir_operation_add(m, b, &r, &op, err);
        if (rc == YVEX_OK) {
            weight = yvex_ir_operation_at(m, op)->results[0];
            binding = (yvex_program_parameter_binding){weight, 7u,
                bf16 ? YVEX_GGUF_QTYPE_BF16 : YVEX_GGUF_QTYPE_F32};
        }
    }
    for (size_t i = 1u; rc == YVEX_OK && i <= 4u; ++i) {
        yvex_ir_id operands[] = {value, weight};
        yvex_ir_operation_request r = {.operation = i % 2u ? "tensor.reshape" : "nn.linear",
            .operands = operands, .operand_count = i % 2u ? 1u : 2u,
            .result_types = types + i, .result_count = 1u};
        rc = yvex_ir_operation_add(m, b, &r, &op, err);
        if (rc == YVEX_OK) value = yvex_ir_operation_at(m, op)->results[0];
    }
    if (rc == YVEX_OK) {
        yvex_ir_operation_request r = {.operation = "core.return", .operands = &value, .operand_count = 1u};
        rc = yvex_ir_operation_add(m, b, &r, &op, err);
    }
    if (rc == YVEX_OK) rc = yvex_ir_seal(m, err);
    if (rc == YVEX_OK) rc = yvex_program_execution_compile(&execution, m, err);
    if (rc == YVEX_OK) rc = yvex_program_physical_compile(out, execution, "forward", &binding, 1u, identity, err);
    yvex_program_execution_close(&execution);
    yvex_ir_module_close(&m);
    return rc;
}

static int test_program_populations(yvex_backend_kind kind, int bf16)
{
    yvex_program_physical *p = NULL, *decoded = NULL;
    yvex_program_stage *stage = NULL;
    yvex_backend *backend = NULL;
    yvex_device_tensor *resident = NULL;
    yvex_backend_options options = {.kind = kind};
    yvex_backend_memory_stats before, after;
    yvex_backend_operation_facts facts;
    yvex_core_bytes wire = {.maximum = 1048576u};
    yvex_error err = {0};
    unsigned char encoded[8192] = {0}, *host = encoded;
    unsigned long long bytes = bf16 ? 4096u : 8192u;
    float input[256], output[64], maximum = 0.0f;
    const float *inputs[] = {input};
    float *outputs[] = {output};
    for (size_t i = 0u; i < 32u; ++i) {
        if (bf16) { unsigned short one = yvex_quant_bf16_encode(1.0f); memcpy(encoded + (i * 64u + i) * 2u, &one, 2u); }
        else { float one = 1.0f; memcpy(encoded + (i * 64u + i) * 4u, &one, 4u); }
    }
    for (size_t i = 0u; i < 256u; ++i) input[i] = (float)i / 16.0f;
    int rc = population_program(&p, bf16, &err);
    if (rc == YVEX_OK) rc = yvex_program_physical_encode(p, &wire, &err);
    if (rc == YVEX_OK) rc = yvex_program_physical_decode(&decoded, wire.data, wire.count, &err);
    if (rc != YVEX_OK) fprintf(stderr, "population compile/import: %s: %s\n",
        yvex_error_where(&err), yvex_error_message(&err));
    YVEX_TEST_ASSERT(rc == YVEX_OK &&
        !strcmp(yvex_program_physical_summary_get(p)->identity,
                yvex_program_physical_summary_get(decoded)->identity), "row-changing program survives authenticated import");
    YVEX_TEST_ASSERT(yvex_backend_open(&backend, &options, &err) == YVEX_OK &&
        yvex_backend_get_memory_stats(backend, &before, &err) == YVEX_OK, "population backend opens");
    if (kind == YVEX_BACKEND_KIND_CUDA) {
        yvex_backend_tensor_desc d = {.name = "population-weight", .dtype = YVEX_DTYPE_I8,
            .rank = 1u, .dims = {bytes}, .bytes = bytes};
        host = NULL;
        YVEX_TEST_ASSERT(yvex_backend_resident_alloc(backend, &d, &resident, &host, &err) == YVEX_OK,
            "exact population parameter residency");
        memcpy(host, encoded, (size_t)bytes);
        YVEX_TEST_ASSERT(yvex_backend_resident_attach(backend, host, bytes, resident, 1u, &err) == YVEX_OK,
            "register borrowed physical parameter storage");
    }
    yvex_program_kernel_parameter parameter = {.tensor_id = 7u,
        .weight = {.encoded = host, .encoded_bytes = bytes, .row_count = 32u, .row_width = 64u,
            .row_bytes = bf16 ? 128u : 256u, .qtype = bf16 ? YVEX_GGUF_QTYPE_BF16 : YVEX_GGUF_QTYPE_F32}};
    YVEX_TEST_ASSERT(yvex_program_stage_open(&stage, decoded, &parameter, 1u, backend, 8u, 1,
        1048576u, 1048576u, &err) == YVEX_OK, "bind one model with distinct 8/4/2 populations");
    for (size_t run = 0u; run < 2u; ++run) {
        rc = yvex_program_stage_host(stage, 8u, inputs, 1u, outputs, 1u, NULL, NULL, &facts, &err);
        if (rc != YVEX_OK) fprintf(stderr, "population execution: %s: %s\n",
            yvex_error_where(&err), yvex_error_message(&err));
        YVEX_TEST_ASSERT(rc == YVEX_OK, "execute distinct populations with identical linear widths");
        for (size_t row = 0u; row < 2u; ++row) for (size_t col = 0u; col < 32u; ++col) {
            float difference = output[row * 32u + col] - input[row * 128u + col];
            if (difference < 0.0f) difference = -difference;
            if (difference > maximum) maximum = difference;
        }
        YVEX_TEST_ASSERT(maximum == 0.0f, "independent reshape/identity-projection oracle is exact");
    }
    YVEX_TEST_ASSERT(yvex_program_stage_close(&stage, &err) == YVEX_OK, "release population stage");
    if (resident) YVEX_TEST_ASSERT(yvex_backend_resident_detach(backend, &err) == YVEX_OK &&
        yvex_backend_tensor_release(backend, &resident, &err) == YVEX_OK, "release population parameters");
    YVEX_TEST_ASSERT(yvex_backend_get_memory_stats(backend, &after, &err) == YVEX_OK &&
        after.allocated_bytes == before.allocated_bytes && yvex_backend_close_checked(&backend, &err) == YVEX_OK,
        "population execution restores allocation baseline");
    yvex_program_physical_close(&decoded);
    yvex_program_physical_close(&p);
    free(wire.data);
    printf("Program populations %s %s: rows=8->4->2 values=64 repeats=2 first=%.9g last=%.9g "
        "max_abs=%g tolerance=0; binary identity stable; allocation_delta=0\n",
        kind == YVEX_BACKEND_KIND_CUDA ? "CUDA" : "CPU", bf16 ? "BF16" : "F32",
        (double)output[0], (double)output[63], (double)maximum);
    return 0;
}
static int index_program(yvex_program_physical **out, unsigned int negative, yvex_error *err)
{
    const char *identity = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    yvex_ir_dialect dialects[] = {*yvex_ir_core_dialect(), *yvex_ir_neural_dialect()};
    yvex_ir_module *m = NULL;
    yvex_program_execution *execution = NULL;
    yvex_ir_dimension rows = {.name = "rows", .minimum = 1u, .maximum = 4u, .multiple = 1u};
    yvex_ir_id dim = YVEX_IR_NONE, types[4] = {0}, inputs[4], fn, block = YVEX_IR_NONE, op, value = YVEX_IR_NONE;
    int rc = yvex_ir_module_open(&m, "index_program", identity, dialects, 2u, err);
    if (rc == YVEX_OK) rc = yvex_ir_dimension_add(m, &rows, &dim, err);
    for (unsigned int i = 0u; rc == YVEX_OK && i < 4u; ++i) {
        yvex_ir_type t = {.kind = YVEX_IR_TENSOR, .scalar = i == 0u || i == 3u ? YVEX_IR_INDEX : YVEX_IR_F32,
            .rank = i == 0u || i == 3u ? 1u : 2u,
            .shape = {{i == 1u ? YVEX_IR_NONE : dim, i == 1u ? 6u : 0u}, {YVEX_IR_NONE, 2u}}};
        if (i == 3u && negative == 3u) t.scalar = YVEX_IR_F32;
        if (t.rank == 1u) t.shape[1] = (yvex_ir_extent){0};
        if (i == 3u && negative == 4u) t.shape[0] = (yvex_ir_extent){YVEX_IR_NONE, 5u};
        rc = yvex_ir_type_intern(m, &t, types + i, err);
    }
    inputs[0] = inputs[1] = inputs[2] = types[0]; inputs[3] = types[1];
    if (rc == YVEX_OK) rc = yvex_ir_function_add(m, "forward", inputs, 4u,
        types + (negative == 7u ? 0u : 2u), 1u, 0u, &fn, err);
    const yvex_ir_id *arguments = NULL;
    if (rc == YVEX_OK) {
        block = yvex_ir_function_at(m, fn)->body;
        arguments = yvex_ir_block_at(m, block)->arguments;
    }
    for (unsigned int i = 0u; rc == YVEX_OK && i < 3u; ++i) {
        yvex_ir_id args[] = {arguments[i ? 2u : 0u], i ? value : arguments[1]};
        yvex_ir_attribute domains[] = {
            {.name = "major_extent", .kind = YVEX_IR_ATTR_U64,
                .value.integer = negative == 1u ? 0u : negative == 2u ? UINT64_MAX :
                    negative == 5u ? UINT64_C(4294967296) : i ? 1u : 2u},
            {.name = "minor_extent", .kind = YVEX_IR_ATTR_U64, .value.integer = i ? 6u : 3u}};
        yvex_ir_operation_request r = {.operation = "tensor.index_linearize", .operands = args,
            .operand_count = 2u, .result_types = types + 3u, .result_count = 1u,
            .attributes = domains, .attribute_count = negative == 6u ? 1u : 2u};
        rc = yvex_ir_operation_add(m, block, &r, &op, err);
        if (rc == YVEX_OK) value = yvex_ir_operation_at(m, op)->results[0];
    }
    if (rc == YVEX_OK && negative != 7u) {
        yvex_ir_id args[] = {arguments[3], value};
        yvex_ir_operation_request r = {.operation = "tensor.indexed_rows", .operands = args,
            .operand_count = 2u, .result_types = types + 2u, .result_count = 1u};
        rc = yvex_ir_operation_add(m, block, &r, &op, err);
        if (rc == YVEX_OK) value = yvex_ir_operation_at(m, op)->results[0];
    }
    if (rc == YVEX_OK) {
        yvex_ir_operation_request r = {.operation = "core.return", .operands = &value, .operand_count = 1u};
        rc = yvex_ir_operation_add(m, block, &r, &op, err);
    }
    if (rc == YVEX_OK) rc = yvex_ir_seal(m, err);
    if (rc == YVEX_OK) rc = yvex_program_execution_compile(&execution, m, err);
    if (rc == YVEX_OK) rc = yvex_program_physical_compile(out, execution, "forward", NULL, 0u, identity, err);
    yvex_program_execution_close(&execution); yvex_ir_module_close(&m);
    return rc;
}

static int index_program_cancel(void *opaque)
{
    unsigned int *calls = opaque;
    return ++*calls >= 3u;
}

static int test_program_index_values(yvex_backend_kind kind)
{
    yvex_program_physical *p = NULL, *decoded = NULL;
    yvex_program_stage *stage = NULL;
    yvex_backend *backend = NULL;
    yvex_backend_options options = {.kind = kind};
    yvex_backend_memory_stats before, after;
    yvex_backend_operation_facts facts;
    yvex_core_bytes wire = {.maximum = 1048576u};
    yvex_error err;
    unsigned int major[] = {0u, 1u, 1u, 0u}, minor[] = {2u, 0u, 2u, 0u}, zero[4] = {0};
    float table[12], output[8], expected[] = {4, 5, 6, 7, 10, 11, 0, 1};
    yvex_program_host_input inputs[] = {{.indices = major}, {.indices = minor},
        {.indices = zero}, {.values = table}};
    for (unsigned int i = 0u; i < 12u; ++i) table[i] = (float)i;
    for (unsigned int i = 1u; i <= 7u; ++i)
        YVEX_TEST_ASSERT(index_program(&p, i, &err) != YVEX_OK && !p,
            "index domain, overflow, shape, scalar, physical precision, attribute and result negatives fail closed");
    YVEX_TEST_ASSERT(index_program(&p, 0u, &err) == YVEX_OK &&
        yvex_program_physical_encode(p, &wire, &err) == YVEX_OK &&
        yvex_program_physical_decode(&decoded, wire.data, wire.count, &err) == YVEX_OK &&
        !strcmp(yvex_program_physical_summary_get(p)->identity,
            yvex_program_physical_summary_get(decoded)->identity), "index SSA preserves canonical binary identity");
    YVEX_TEST_ASSERT(yvex_program_physical_summary_get(p)->storage_count == 2u,
        "three index versions share two compiler-proven disjoint host slots");
    YVEX_TEST_ASSERT(yvex_backend_open(&backend, &options, &err) == YVEX_OK &&
        yvex_backend_get_memory_stats(backend, &before, &err) == YVEX_OK &&
        yvex_program_stage_open(&stage, decoded, NULL, 0u, backend, 4u, 1, 1048576u, 1048576u, &err) == YVEX_OK,
        "typed index stages bind exact host storage under the aggregate resource budget");
    for (unsigned int i = 0u; i < 3u; ++i) {
        unsigned long long rows = i == 1u ? 1u : 4u;
        int rc = yvex_program_stage_host_inputs(stage, rows, inputs, 4u, (float *[]){output}, 1u,
            NULL, NULL, &facts, &err);
        if (rc != YVEX_OK) fprintf(stderr, "index SSA: %s: %s\n", yvex_error_where(&err), yvex_error_message(&err));
        YVEX_TEST_ASSERT(rc == YVEX_OK && !memcmp(expected, output, (size_t)rows * 2u * sizeof(float)),
            "produced index values drive real row selection with exact integer semantics at each population");
    }
    for (unsigned int i = 0u; i < 4u; ++i) {
        unsigned int calls = 0u;
        major[1] = i == 0u ? 2u : 1u; minor[2] = i == 1u ? 3u : 2u;
        inputs[1].indices = i == 2u ? NULL : minor;
        memset(output, 0x5a, sizeof(output));
        YVEX_TEST_ASSERT(yvex_program_stage_host_inputs(stage, 4u, inputs, 4u, (float *[]){output}, 1u,
            i == 3u ? index_program_cancel : NULL, &calls, &facts, &err) != YVEX_OK &&
            ((unsigned char *)output)[0] == 0x5a,
            "invalid coordinates, missing input and mid-operation cancellation publish no result");
    }
    YVEX_TEST_ASSERT(yvex_program_stage_host_inputs(stage, 4u, inputs, 4u, (float *[]){output}, 1u,
        NULL, NULL, &facts, &err) == YVEX_OK && !memcmp(output, expected, sizeof(output)),
        "aborted partial index versions cannot contaminate a valid retry");
    YVEX_TEST_ASSERT(yvex_program_stage_close(&stage, &err) == YVEX_OK &&
        yvex_backend_get_memory_stats(backend, &after, &err) == YVEX_OK &&
        before.allocated_bytes == after.allocated_bytes && yvex_backend_close_checked(&backend, &err) == YVEX_OK,
        "index SSA and device results return to the resource baseline");
    yvex_program_physical_close(&p); yvex_program_physical_close(&decoded); free(wire.data);
    printf("Index SSA %s: versions=3 host_slots=2 rows=4,1,4 outputs=8 expected=4,5,6,7,10,11,0,1 "
        "observed=4,5,6,7,10,11,0,1 max_abs=0 tolerance=0 compiler_negatives=7 runtime_negatives=4 retry=exact\n",
        kind == YVEX_BACKEND_KIND_CUDA ? "CUDA" : "CPU");
    return 0;
}
#endif
