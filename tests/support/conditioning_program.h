/* Row-indexed conditioning is ordinary typed computation, not a family loop. */
#ifndef TESTS_SUPPORT_CONDITIONING_PROGRAM_H
#define TESTS_SUPPORT_CONDITIONING_PROGRAM_H
#include "tests/test.h"
#include "tests/support/observation_program.h"
#include <yvex/internal/program_stage.h>
#include <yvex/internal/component.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int prepared_programs(yvex_program_physical **preparation, yvex_program_physical **step, yvex_error *err)
{
    const char *source = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    yvex_ir_dialect dialects[] = {*yvex_ir_core_dialect(), *yvex_ir_neural_dialect()};
    yvex_ir_module *m = NULL;
    yvex_program_execution *execution = NULL;
    yvex_ir_type t = {.kind = YVEX_IR_TENSOR, .scalar = YVEX_IR_F32, .rank = 2u,
        .shape = {{YVEX_IR_NONE, 3u}, {YVEX_IR_NONE, 4u}}};
    yvex_ir_id type, types[2], function, op;
    int rc = yvex_ir_module_open(&m, "prepared", source, dialects, 2u, err);
    if (rc == YVEX_OK) rc = yvex_ir_type_intern(m, &t, &type, err);
    types[0] = types[1] = type;
    for (size_t i = 0u; rc == YVEX_OK && i < 2u; ++i) {
        rc = yvex_ir_function_add(m, i ? "step" : "prepare", types, i + 1u, &type, 1u, 0u, &function, err);
        if (rc != YVEX_OK) break;
        yvex_ir_id block = yvex_ir_function_at(m, function)->body;
        const yvex_ir_id *arguments = yvex_ir_block_at(m, block)->arguments;
        yvex_ir_id args[] = {arguments[0], arguments[i]};
        yvex_ir_operation_request r = {.operation = "tensor.add", .operands = args, .operand_count = 2u,
            .result_types = &type, .result_count = 1u};
        rc = yvex_ir_operation_add(m, block, &r, &op, err);
        if (rc == YVEX_OK) {
            yvex_ir_id result = yvex_ir_operation_at(m, op)->results[0];
            r = (yvex_ir_operation_request){.operation = "core.return", .operands = &result, .operand_count = 1u};
            rc = yvex_ir_operation_add(m, block, &r, &op, err);
        }
    }
    if (rc == YVEX_OK) rc = yvex_ir_seal(m, err);
    if (rc == YVEX_OK) rc = yvex_program_execution_compile(&execution, m, err);
    if (rc == YVEX_OK) rc = yvex_program_physical_compile(preparation, execution, "prepare", NULL, 0u, source, err);
    if (rc == YVEX_OK) rc = yvex_program_physical_compile(step, execution, "step", NULL, 0u, source, err);
    yvex_program_execution_close(&execution); yvex_ir_module_close(&m);
    return rc;
}

static int prepared_cancel(void *context)
{
    return ++*(unsigned int *)context >= 2u;
}

static int test_prepared_program(yvex_backend_kind kind)
{
    yvex_program_physical *preparation = NULL, *step = NULL;
    yvex_program_prepared *prepared = NULL, *refused = NULL;
    yvex_backend *backend = NULL;
    yvex_backend_options options = {.kind = kind};
    yvex_backend_memory_stats before, after;
    yvex_backend_operation_facts facts;
    yvex_error err = {0};
    float x[12], y[12], output[12], expected[12];
    for (size_t i = 0u; i < 12u; ++i) { x[i] = (float)i / 8.0f; y[i] = -(float)i / 16.0f; }
    yvex_program_host_input inputs[] = {{0}, {.values = y}}, prepare_input[] = {{.values = x}};
    float *outputs[] = {output};
    size_t link = 0u;
    YVEX_TEST_ASSERT(prepared_programs(&preparation, &step, &err) == YVEX_OK &&
        yvex_backend_open(&backend, &options, &err) == YVEX_OK &&
        yvex_backend_get_memory_stats(backend, &before, &err) == YVEX_OK, "fixed-shape prepared fixture opens");
    for (unsigned int negative = 0u; negative < 5u; ++negative) {
        size_t bad = negative == 0u ? 2u : 0u;
        YVEX_TEST_ASSERT(yvex_program_prepared_open(&refused, preparation, step, NULL, 0u, backend,
            negative == 1u ? 2u : 1u, 1u, prepare_input, &bad, negative == 2u ? 0u : 1u,
            negative == 3u ? 1u : 1048576u, negative == 4u ? 1u : 1048576u, NULL, &facts, &err) != YVEX_OK &&
            !refused, "invalid prepared link/population/host/device budget leaves no owner");
    }
    YVEX_TEST_ASSERT(yvex_program_prepared_open(&prepared, preparation, step, NULL, 0u, backend,
        1u, 1u, prepare_input, &link, 1u, 1048576u, 1048576u, NULL, &facts, &err) == YVEX_OK,
        "typed preparation executes once");
    yvex_program_physical_close(&preparation); yvex_program_physical_close(&step);
    unsigned long long host, device, retained;
    yvex_program_prepared_resources(prepared, &host, &device, &retained);
    YVEX_TEST_ASSERT(retained == sizeof(x) && host >= retained, "prepared bytes are accounted exactly");
    for (size_t i = 0u; i < 12u; ++i) { expected[i] = 2.0f * x[i] + y[i]; x[i] = 999.0f; }
    for (size_t run = 0u; run < 2u; ++run)
        YVEX_TEST_ASSERT(yvex_program_prepared_run(prepared, inputs, 2u, outputs, 1u, NULL, NULL, NULL,
            &facts, &err) == YVEX_OK && !memcmp(expected, output, sizeof(output)),
            "mutated original input cannot change retained prepared values; repeat exact");
    inputs[0].values = x;
    YVEX_TEST_ASSERT(yvex_program_prepared_run(prepared, inputs, 2u, outputs, 1u, NULL, NULL, NULL,
        &facts, &err) != YVEX_OK && !memcmp(expected, output, sizeof(output)),
        "caller cannot replace a prepared input");
    inputs[0].values = NULL;
    unsigned int calls = 0u;
    for (size_t i = 0u; i < 12u; ++i) output[i] = 777.0f;
    YVEX_TEST_ASSERT(yvex_program_prepared_run(prepared, inputs, 2u, outputs, 1u, prepared_cancel, &calls, NULL,
        &facts, &err) == YVEX_ERR_CANCELLED, "late cancellation refuses before host publication");
    for (size_t i = 0u; i < 12u; ++i) YVEX_TEST_ASSERT(output[i] == 777.0f, "cancel leaves every output untouched");
    YVEX_TEST_ASSERT(yvex_program_prepared_run(prepared, inputs, 2u, outputs, 1u, NULL, NULL, NULL,
        &facts, &err) == YVEX_OK && !memcmp(expected, output, sizeof(output)),
        "cancelled invocation preserves immutable preparation for subsequent use");
    YVEX_TEST_ASSERT(yvex_program_prepared_close(&prepared, &err) == YVEX_OK &&
        yvex_backend_get_memory_stats(backend, &after, &err) == YVEX_OK &&
        before.allocated_bytes == after.allocated_bytes && yvex_backend_close_checked(&backend, &err) == YVEX_OK,
        "prepared cleanup returns all resources to baseline");
    printf("Prepared composition %s: 12 values, preparations=1, repeats=3 max_abs=0 tolerance=0; "
        "retained_bytes=48; importer programs released; original mutation isolated; "
        "5 admission negatives; substitution/cancel unpublished; "
        "allocation_delta=0\n", kind == YVEX_BACKEND_KIND_CUDA ? "CUDA" : "CPU");
    yvex_program_physical_close(&preparation); yvex_program_physical_close(&step);
    return 0;
}

static int conditioning_program(yvex_program_physical **out, unsigned int malformed, yvex_error *err)
{
    const char *source = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    yvex_ir_dialect dialects[] = {*yvex_ir_core_dialect(), *yvex_ir_neural_dialect()};
    yvex_ir_module *m = NULL;
    yvex_program_execution *e = NULL;
    yvex_ir_dimension rows = {.name = "rows", .minimum = 1u, .maximum = 4u, .multiple = 1u};
    yvex_ir_type x = {.kind = YVEX_IR_TENSOR, .scalar = YVEX_IR_BF16, .rank = 2u,
        .shape = {{0u, 0u}, {YVEX_IR_NONE, 8u}}};
    yvex_ir_type table = {.kind = YVEX_IR_TENSOR, .scalar = YVEX_IR_BF16, .rank = 3u,
        .shape = {{YVEX_IR_NONE, 3u}, {YVEX_IR_NONE, 6u}, {YVEX_IR_NONE, 8u}}};
    yvex_ir_type indices = {.kind = YVEX_IR_TENSOR, .scalar = YVEX_IR_INDEX, .rank = 1u,
        .shape = {{0u, 0u}}};
    yvex_ir_id dimension, types[4], results[2], f, b = YVEX_IR_NONE, op, args[4], hidden;
    if (malformed == 1u) table.shape[2].extent++;
    if (malformed == 2u) indices.scalar = YVEX_IR_F32;
    if (malformed == 3u) table.scalar = YVEX_IR_F32;
    if (malformed == 4u) indices.shape[0] = (yvex_ir_extent){YVEX_IR_NONE, 3u};
    int rc = yvex_ir_module_open(&m, "conditioning", source, dialects, 2u, err);
    if (rc == YVEX_OK) rc = yvex_ir_dimension_add(m, &rows, &dimension, err);
    if (rc == YVEX_OK) rc = yvex_ir_type_intern(m, &x, &hidden, err);
    x.scalar = YVEX_IR_F32;
    if (rc == YVEX_OK) rc = yvex_ir_type_intern(m, &x, &types[0], err);
    if (rc == YVEX_OK) rc = yvex_ir_type_intern(m, &table, &types[1], err);
    if (rc == YVEX_OK) rc = yvex_ir_type_intern(m, &indices, &types[2], err);
    types[3] = hidden; results[0] = hidden; results[1] = hidden;
    if (rc == YVEX_OK) rc = yvex_ir_function_add(m, "forward", types, 4u, results, 2u, 0u, &f, err);
    if (rc == YVEX_OK) {
        b = yvex_ir_function_at(m, f)->body;
        memcpy(args, yvex_ir_block_at(m, b)->arguments, sizeof(args));
        const char *operations[] = {"tensor.cast", "nn.silu", "tensor.cast", "tensor.cast"};
        for (size_t i = 0u; rc == YVEX_OK && i < 4u; ++i) {
            yvex_ir_id type = i == 2u ? types[0] : hidden;
            yvex_ir_operation_request r = {.operation = operations[i], .operands = args,
                .operand_count = 1u, .result_types = &type, .result_count = 1u};
            rc = yvex_ir_operation_add(m, b, &r, &op, err);
            if (rc == YVEX_OK) args[0] = yvex_ir_operation_at(m, op)->results[0];
        }
        yvex_ir_attribute attrs[] = {
            {.name = "shift", .kind = YVEX_IR_ATTR_U64, .value.integer = malformed == 5u ? 6u : 0u},
            {.name = "scale", .kind = YVEX_IR_ATTR_U64, .value.integer = malformed == 6u ? 6u : 1u}};
        yvex_ir_operation_request r = {.operation = "nn.indexed_modulate", .operands = args,
            .operand_count = 3u, .result_types = &hidden, .result_count = 1u,
            .attributes = attrs, .attribute_count = malformed == 7u ? 1u : 2u};
        if (rc == YVEX_OK) rc = yvex_ir_operation_add(m, b, &r, &op, err);
        if (rc == YVEX_OK) results[0] = yvex_ir_operation_at(m, op)->results[0];
    }
    if (rc == YVEX_OK) {
        args[0] = results[0];
        yvex_ir_attribute gate = {.name = "gate", .kind = YVEX_IR_ATTR_U64,
            .value.integer = malformed == 8u ? 6u : 2u};
        yvex_ir_operation_request r = {.operation = "nn.indexed_gated_residual", .operands = args,
            .operand_count = 4u, .result_types = &hidden, .result_count = 1u,
            .attributes = &gate, .attribute_count = 1u};
        rc = yvex_ir_operation_add(m, b, &r, &op, err);
        if (rc == YVEX_OK) results[1] = yvex_ir_operation_at(m, op)->results[0];
    }
    if (rc == YVEX_OK) {
        yvex_ir_operation_request r = {.operation = "core.return", .operands = results, .operand_count = 2u};
        rc = yvex_ir_operation_add(m, b, &r, &op, err);
    }
    if (rc == YVEX_OK) rc = yvex_ir_seal(m, err);
    if (rc == YVEX_OK) rc = yvex_program_execution_compile(&e, m, err);
    if (rc == YVEX_OK) rc = yvex_program_physical_compile(out, e, "forward", NULL, 0u, source, err);
    yvex_program_execution_close(&e); yvex_ir_module_close(&m);
    return rc;
}

/* Finite, normal fixture values: mathematical nearest-even quantization,
 * independent of production encoding and of the CUDA implementation. */
static float conditioning_round(float value)
{
    if (value == 0.0f) return value;
    int exponent;
    double mantissa = frexp((double)value, &exponent) * 256.0;
    double lower = floor(mantissa), fraction = mantissa - lower;
    if (fraction > 0.5 || (fraction == 0.5 && fmod(lower, 2.0) != 0.0)) lower += 1.0;
    return (float)ldexp(lower, exponent - 8);
}

static int conditioning_cancel(void *context)
{
    return ++*(unsigned int *)context >= 2u;
}

static int conditioning_parameter(void *context, unsigned long long id, char name[256], yvex_error *err)
{
    (void)context; (void)id; (void)name;
    yvex_error_set(err, YVEX_ERR_STATE, "test.conditioning", "parameter-free program must not resolve names");
    return YVEX_ERR_STATE;
}

static int position_program(yvex_program_physical **out, unsigned int malformed, yvex_error *err)
{
    const char *source = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    yvex_ir_dialect dialects[] = {*yvex_ir_core_dialect(), *yvex_ir_neural_dialect()};
    yvex_ir_module *m = NULL;
    yvex_program_execution *execution = NULL;
    yvex_ir_dimension dim = {.name = "rows", .minimum = 2u, .maximum = 2u, .multiple = 1u};
    const unsigned long long rows[] = {2u, 2u, 1u, 1u, 3u, 2u, 3u, 2u, 3u, 2u, 3u};
    const unsigned long long widths[] = {3u, 0u, 3u, 0u, 0u, 1u, 2u, 0u, 3u, 4u, 8u};
    yvex_ir_id dimension, types[11], result_types[5], results[5], function, block, op;
    int rc = yvex_ir_module_open(&m, "position_and_partition", source, dialects, 2u, err);
    if (rc == YVEX_OK) rc = yvex_ir_dimension_add(m, &dim, &dimension, err);
    for (size_t i = 0u; rc == YVEX_OK && i < 11u; ++i) {
        yvex_ir_type t = {.kind = YVEX_IR_TENSOR,
            .scalar = i == 1u || i == 3u || i == 4u ? YVEX_IR_INDEX : i == 10u ? YVEX_IR_BF16 : YVEX_IR_F32,
            .rank = widths[i] ? 2u : 1u,
            .shape = {{YVEX_IR_NONE, rows[i]}, {YVEX_IR_NONE, widths[i]}}};
        if (!widths[i]) memset(t.shape + 1u, 0, sizeof(t.shape[1]));
        if (i == 5u || i == 9u) t.shape[0] = (yvex_ir_extent){dimension, 0u};
        if (malformed == 1u && i == 3u) t.scalar = YVEX_IR_F32;
        if (malformed == 2u && i == 2u) t.shape[1].extent++;
        if (malformed == 3u && i == 8u) t.shape[0].extent++;
        if (malformed == 4u && i == 9u) t.shape[1].extent++;
        if (malformed == 5u && i == 10u) t.shape[1].extent++;
        if (malformed == 6u && i == 7u) t.scalar = YVEX_IR_BF16;
        rc = yvex_ir_type_intern(m, &t, &types[i], err);
    }
    result_types[0] = result_types[1] = types[8];
    result_types[2] = types[9]; result_types[3] = result_types[4] = types[10];
    if (rc == YVEX_OK) rc = yvex_ir_function_add(m, "forward", types, 8u, result_types, 5u, 0u, &function, err);
    if (rc == YVEX_OK) {
        block = yvex_ir_function_at(m, function)->body;
        const yvex_ir_id *args = yvex_ir_block_at(m, block)->arguments;
        yvex_ir_operation_request r = {.operation = "tensor.partition_rows", .operands = args,
            .operand_count = malformed == 7u ? 3u : 4u, .result_types = types + 8u, .result_count = 1u};
        rc = yvex_ir_operation_add(m, block, &r, &op, err);
        if (rc == YVEX_OK) {
            results[0] = yvex_ir_operation_at(m, op)->results[0];
            yvex_ir_id gather[] = {results[0], args[4]};
            r = (yvex_ir_operation_request){.operation = "tensor.indexed_rows", .operands = gather,
                .operand_count = 2u, .result_types = types + 8u, .result_count = 1u};
            rc = yvex_ir_operation_add(m, block, &r, &op, err);
        }
        if (rc == YVEX_OK) {
            results[1] = yvex_ir_operation_at(m, op)->results[0];
            yvex_ir_attribute period = {.name = "maximum_period", .kind = YVEX_IR_ATTR_F64,
                .value.real = malformed == 8u ? 1.0 : 10000.0};
            r = (yvex_ir_operation_request){.operation = "nn.sinusoidal_embedding", .operands = args + 5u,
                .operand_count = 1u, .result_types = types + 9u, .result_count = 1u,
                .attributes = &period, .attribute_count = 1u};
            rc = yvex_ir_operation_add(m, block, &r, &op, err);
        }
        if (rc == YVEX_OK) {
            results[2] = yvex_ir_operation_at(m, op)->results[0];
            r = (yvex_ir_operation_request){.operation = "tensor.axis_rotary", .operands = args + 6u,
                .operand_count = 2u, .result_types = result_types + 3u, .result_count = 2u};
            rc = yvex_ir_operation_add(m, block, &r, &op, err);
        }
        if (rc == YVEX_OK) {
            memcpy(results + 3u, yvex_ir_operation_at(m, op)->results, sizeof(yvex_ir_id) * 2u);
            r = (yvex_ir_operation_request){.operation = "core.return", .operands = results, .operand_count = 5u};
            rc = yvex_ir_operation_add(m, block, &r, &op, err);
        }
    }
    if (rc == YVEX_OK) rc = yvex_ir_seal(m, err);
    if (rc == YVEX_OK) rc = yvex_program_execution_compile(&execution, m, err);
    if (rc == YVEX_OK) rc = yvex_program_physical_compile(out, execution, "forward", NULL, 0u, source, err);
    yvex_program_execution_close(&execution); yvex_ir_module_close(&m);
    return rc;
}

static int test_position_program(yvex_backend_kind kind)
{
    yvex_program_physical *p = NULL, *decoded = NULL;
    yvex_program_stage *stage = NULL;
    yvex_backend *backend = NULL;
    yvex_backend_options options = {.kind = kind};
    yvex_backend_operation_facts facts;
    yvex_backend_memory_stats before, after;
    yvex_core_bytes wire = {.maximum = 1048576u};
    yvex_error err = {0};
    for (unsigned int i = 1u; i <= 8u; ++i) {
        YVEX_TEST_ASSERT(position_program(&p, i, &err) != YVEX_OK && !p, "position/partition verifier negative");
    }
    int rc = position_program(&p, 0u, &err);
    if (rc != YVEX_OK) fprintf(stderr, "position compile %s: %s\n", yvex_error_where(&err), yvex_error_message(&err));
    if (rc == YVEX_OK) rc = yvex_program_physical_encode(p, &wire, &err);
    if (rc == YVEX_OK) rc = yvex_program_physical_decode(&decoded, wire.data, wire.count, &err);
    if (rc != YVEX_OK) fprintf(stderr, "position import %s: %s\n", yvex_error_where(&err), yvex_error_message(&err));
    YVEX_TEST_ASSERT(rc == YVEX_OK &&
        !strcmp(yvex_program_physical_summary_get(p)->identity, yvex_program_physical_summary_get(decoded)->identity),
        "typed index/position program binary identity");
    YVEX_TEST_ASSERT(yvex_backend_open(&backend, &options, &err) == YVEX_OK &&
        yvex_backend_get_memory_stats(backend, &before, &err) == YVEX_OK, "position backend");
    rc = yvex_program_stage_open(&stage, decoded, NULL, 0u, backend, 2u, 1, 1048576u, 1048576u, &err);
    if (rc != YVEX_OK) fprintf(stderr, "position bind %s: %s\n", yvex_error_where(&err), yvex_error_message(&err));
    YVEX_TEST_ASSERT(rc == YVEX_OK, "position program admits implementations");
    float x[] = {-0.0f, 2, 3, 4, 5, 6}, y[] = {7, 8, 9}, time[] = {0.0f, 0.625f};
    float positions[] = {0, 1, -2, 3, 4, -5}, freq[] = {1.0f, 0.25f};
    unsigned int xi[] = {2u, 0u}, yi[] = {1u}, select[] = {1u, 2u, 1u};
    yvex_program_host_input inputs[] = {{.values = x}, {.indices = xi}, {.values = y}, {.indices = yi},
        {.indices = select}, {.values = time}, {.values = positions}, {.values = freq}};
    float merged[9], selected[9], embedding[8], cosine[24], sine[24];
    float *outputs[] = {merged, selected, embedding, cosine, sine};
    rc = yvex_program_stage_host_inputs(stage, 2u, inputs, 8u, outputs, 5u, NULL, NULL, &facts, &err);
    if (rc != YVEX_OK) fprintf(stderr, "position execute %s: %s\n", yvex_error_where(&err), yvex_error_message(&err));
    YVEX_TEST_ASSERT(rc == YVEX_OK, "typed multi-population position execution");
    float expected[] = {4, 5, 6, 7, 8, 9, -0.0f, 2, 3};
    YVEX_TEST_ASSERT(!memcmp(merged, expected, sizeof(expected)), "partition exact with negative zero");
    for (size_t row = 0u; row < 3u; ++row)
        YVEX_TEST_ASSERT(!memcmp(selected + row * 3u, expected + select[row] * 3u, 3u * sizeof(float)),
            "gather allows repeated source rows and preserves all bits");
    double max_embedding = 0.0;
    for (size_t row = 0u; row < 2u; ++row) for (size_t lane = 0u; lane < 2u; ++lane) {
        double angle = (double)time[row] * pow(10000.0, -(double)lane / 2.0);
        max_embedding = fmax(max_embedding, fabs(embedding[row * 4u + lane] - cos(angle)));
        max_embedding = fmax(max_embedding, fabs(embedding[row * 4u + 2u + lane] - sin(angle)));
    }
    YVEX_TEST_ASSERT(max_embedding <= 1e-6, "independent double precision sinusoidal oracle");
    for (size_t row = 0u; row < 3u; ++row) for (size_t column = 0u; column < 8u; ++column) {
        size_t pair = column % 4u, i = row * 8u + column;
        double angle = (double)positions[row * 2u + pair / 2u] * freq[pair % 2u];
        YVEX_TEST_ASSERT(cosine[i] == conditioning_round((float)cos(angle)) &&
            sine[i] == conditioning_round((float)sin(angle)), "axis-major BF16 tables match independent oracle exactly");
    }
    for (unsigned int scenario = 0u; scenario < 4u; ++scenario) {
        yi[0] = scenario == 0u ? 0u : 1u;
        select[0] = scenario == 1u ? 3u : 1u;
        positions[0] = scenario == 2u ? INFINITY : 0.0f;
        unsigned int cancellation = 0u;
        for (size_t i = 0u; i < 9u; ++i) merged[i] = selected[i] = 123.0f;
        rc = yvex_program_stage_host_inputs(stage, 2u, inputs, 8u, outputs, 5u,
            scenario == 3u ? conditioning_cancel : NULL, &cancellation, &facts, &err);
        YVEX_TEST_ASSERT(rc != YVEX_OK && merged[0] == 123.0f && selected[0] == 123.0f,
            "partition overlap, gather bounds, nonfinite positions and cancellation publish nothing");
    }
    YVEX_TEST_ASSERT(yvex_program_stage_close(&stage, &err) == YVEX_OK &&
        yvex_backend_get_memory_stats(backend, &after, &err) == YVEX_OK &&
        before.allocated_bytes == after.allocated_bytes && yvex_backend_close_checked(&backend, &err) == YVEX_OK,
        "position operations release all staged resources");
    printf("Position/partition %s: 18 copied values bitwise exact; 48 rotary values max_abs=0; "
        "8 sinusoidal values max_abs=%.9g tolerance=1e-6; 8 IR + 4 execution negatives; allocation_delta=0\n",
        kind == YVEX_BACKEND_KIND_CUDA ? "CUDA" : "CPU", max_embedding);
    yvex_program_physical_close(&decoded); yvex_program_physical_close(&p); free(wire.data);
    return 0;
}

static int test_conditioning_program(yvex_backend_kind kind)
{
    yvex_program_physical *p = NULL, *decoded = NULL;
    yvex_program_stage *stage = NULL;
    yvex_backend *backend = NULL;
    yvex_backend_options options = {.kind = kind};
    yvex_backend_memory_stats before, after;
    yvex_backend_operation_facts facts;
    yvex_core_bytes wire = {.maximum = 1048576u};
    yvex_error err = {0};
    float x[32], table[144], update[32], modulated[32], gated[32], maximum = 0.0f;
    unsigned int indices[] = {2u, 0u, 2u, 1u};
    yvex_program_host_input inputs[] = {{.values = x}, {.values = table},
        {.indices = indices}, {.values = update}};
    float *outputs[] = {modulated, gated};
    for (size_t i = 0u; i < 32u; ++i) {
        x[i] = (float)((int)i - 15) / 16.0f + 1.0f / 512.0f;
        update[i] = (float)((int)i - 10) / 32.0f;
    }
    for (size_t i = 0u; i < 144u; ++i) table[i] = (float)((int)(i % 31u) - 14) / 128.0f;
    for (unsigned int malformed = 1u; malformed <= 8u; ++malformed) {
        YVEX_TEST_ASSERT(conditioning_program(&p, malformed, &err) != YVEX_OK && !p,
            "conditioning type/shape/slot/attribute errors fail before execution");
    }
    int rc = conditioning_program(&p, 0u, &err);
    if (rc == YVEX_OK) rc = yvex_program_physical_encode(p, &wire, &err);
    if (rc == YVEX_OK) rc = yvex_program_physical_decode(&decoded, wire.data, wire.count, &err);
    if (rc != YVEX_OK) fprintf(stderr, "conditioning compile: %s: %s\n",
        yvex_error_where(&err), yvex_error_message(&err));
    YVEX_TEST_ASSERT(rc == YVEX_OK && !strcmp(yvex_program_physical_summary_get(p)->identity,
        yvex_program_physical_summary_get(decoded)->identity), "conditioning identity survives binary import");
    YVEX_TEST_ASSERT(yvex_backend_open(&backend, &options, &err) == YVEX_OK &&
        yvex_backend_get_memory_stats(backend, &before, &err) == YVEX_OK, "conditioning backend opens");
    rc = yvex_program_stage_open(&stage, decoded, NULL, 0u, backend, 4u, 1, 1048576u, 1048576u, &err);
    if (rc != YVEX_OK) fprintf(stderr, "conditioning bind: %s: %s\n",
        yvex_error_where(&err), yvex_error_message(&err));
    YVEX_TEST_ASSERT(rc == YVEX_OK, "conditioning has no family or parameter-name execution dependency");
    for (size_t run = 0u; run < 2u; ++run) {
        rc = yvex_program_stage_host_inputs(stage, 4u, inputs, 4u, outputs, 2u, NULL, NULL, &facts, &err);
        if (rc != YVEX_OK) fprintf(stderr, "conditioning execution: %s: %s\n",
            yvex_error_where(&err), yvex_error_message(&err));
        YVEX_TEST_ASSERT(rc == YVEX_OK, "indexed modulation and residual execute as dependent SSA values");
        for (size_t i = 0u; i < 32u; ++i) {
            size_t offset = (size_t)indices[i / 8u] * 48u + i % 8u;
            float scale = conditioning_round(1.0f + table[offset + 8u]);
            float rounded = conditioning_round(x[i]);
            float activation = conditioning_round((float)((double)rounded / (1.0 + exp(-(double)rounded))));
            float expected = conditioning_round(conditioning_round(activation * scale) + table[offset]);
            float expected_gate = conditioning_round(expected + conditioning_round(update[i] * table[offset + 16u]));
            float difference = fmaxf(fabsf(modulated[i] - expected), fabsf(gated[i] - expected_gate));
            if (difference > maximum) maximum = difference;
        }
        YVEX_TEST_ASSERT(maximum == 0.0f, "independent nearest-even operation-boundary oracle is exact");
    }
    for (size_t i = 0u; i < 32u; ++i) modulated[i] = gated[i] = 123.0f;
    indices[3] = 3u;
    YVEX_TEST_ASSERT(yvex_program_stage_host_inputs(stage, 4u, inputs, 4u, outputs, 2u,
        NULL, NULL, &facts, &err) != YVEX_OK, "out-of-range conditioning index fails closed");
    indices[3] = 1u;
    unsigned int calls = 0u;
    YVEX_TEST_ASSERT(yvex_program_stage_host_inputs(stage, 4u, inputs, 4u, outputs, 2u,
        conditioning_cancel, &calls, &facts, &err) != YVEX_OK, "conditioning cancellation refuses publication");
    for (size_t i = 0u; i < 32u; ++i)
        YVEX_TEST_ASSERT(modulated[i] == 123.0f && gated[i] == 123.0f, "no partial host result escapes failure");
    YVEX_TEST_ASSERT(yvex_program_stage_close(&stage, &err) == YVEX_OK, "release direct conditioning stage");
    const float *component_inputs[] = {x, table, NULL, update};
    const unsigned int *component_indices[] = {NULL, NULL, indices, NULL};
    unsigned long long input_capacity[] = {32u, 144u, 4u, 32u}, output_capacity[] = {32u, 32u};
    yvex_component_execution component = {.schema_version = YVEX_COMPONENT_EXECUTION_SCHEMA_V2,
        .backend = backend, .program_stage = &stage};
    memcpy(component.residency_identity, yvex_program_physical_summary_get(p)->identity, 65u);
    yvex_component_program_request request = {.program = p, .parameter_name = conditioning_parameter,
        .inputs = component_inputs, .index_inputs = component_indices, .input_capacity = input_capacity,
        .outputs = outputs, .output_capacity = output_capacity, .input_count = 4u, .output_count = 2u,
        .rows = 4u, .host_limit = 1048576u, .device_limit = 1048576u};
    yvex_component_program_result result;
    YVEX_TEST_ASSERT(yvex_component_tensor_program_execute(&component, &request, &result, &err) == YVEX_OK &&
        result.complete && !stage, "common component consumer accepts typed float/index inputs");
    char prior_identity[65];
    memcpy(prior_identity, result.execution_identity, sizeof(prior_identity));
    indices[0] = 0u;
    YVEX_TEST_ASSERT(yvex_component_tensor_program_execute(&component, &request, &result, &err) == YVEX_OK &&
        strcmp(prior_identity, result.execution_identity), "index values participate in execution identity");
    component_inputs[2] = x;
    YVEX_TEST_ASSERT(yvex_component_tensor_program_execute(&component, &request, &result, &err) != YVEX_OK &&
        !result.complete && !stage, "ambiguous float/index representation refuses before invocation");
    component_inputs[2] = NULL; component_indices[2] = NULL;
    YVEX_TEST_ASSERT(yvex_component_tensor_program_execute(&component, &request, &result, &err) != YVEX_OK &&
        !result.complete && !stage, "missing index input fails closed at component admission");
    YVEX_TEST_ASSERT(
        yvex_backend_get_memory_stats(backend, &after, &err) == YVEX_OK &&
        before.allocated_bytes == after.allocated_bytes && yvex_backend_close_checked(&backend, &err) == YVEX_OK,
        "conditioning cleanup restores resource baseline");
    printf("Indexed conditioning %s: F32/BF16 casts + SiLU + modulation + gated residual; values=64 repeats=2 max_abs=%g tolerance=0; "
        "8 compiler negatives; index/cancellation unpublished; component identity/typed-input negatives; allocation_delta=0\n",
        kind == YVEX_BACKEND_KIND_CUDA ? "CUDA" : "CPU", (double)maximum);
    yvex_program_physical_close(&decoded); yvex_program_physical_close(&p); free(wire.data);
    return test_position_program(kind) || test_prepared_program(kind) || test_observation_program(kind);
}
#endif
