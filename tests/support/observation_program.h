/* Ordered diagnostic publication must not retain every intermediate tensor. */
#ifndef TESTS_SUPPORT_OBSERVATION_PROGRAM_H
#define TESTS_SUPPORT_OBSERVATION_PROGRAM_H
#include "tests/test.h"
#include <yvex/internal/program_stage.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int observation_program(yvex_program_physical **out, unsigned int malformed, yvex_error *err)
{
    const char *source = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    yvex_ir_dialect dialects[] = {*yvex_ir_core_dialect(), *yvex_ir_neural_dialect()};
    yvex_ir_module *m = NULL, *canonical = NULL;
    yvex_program_execution *execution = NULL;
    yvex_ir_type t = {.kind = YVEX_IR_TENSOR, .scalar = YVEX_IR_F32, .rank = 2u,
        .shape = {{YVEX_IR_NONE, 3u}, {YVEX_IR_NONE, 4u}}};
    if (malformed == 1u) t.scalar = YVEX_IR_INDEX;
    if (malformed == 2u) {
        t.kind = YVEX_IR_STATE;
        memcpy(t.domain, "sequence.test", sizeof("sequence.test"));
    }
    yvex_ir_id type, function, op, value, input;
    int rc = yvex_ir_module_open(&m, "observed", source, dialects, 2u, err);
    if (rc == YVEX_OK) rc = yvex_ir_type_intern(m, &t, &type, err);
    if (rc == YVEX_OK) rc = yvex_ir_function_add(m, "forward", &type, 1u, &type, 1u,
        malformed == 3u ? 0u : YVEX_IR_PUBLISH | YVEX_IR_ORDERED, &function, err);
    if (rc != YVEX_OK) goto done;
    yvex_ir_id block = yvex_ir_function_at(m, function)->body;
    input = value = yvex_ir_block_at(m, block)->arguments[0];
    for (size_t i = 0u; rc == YVEX_OK && i < 2u; ++i) {
        yvex_ir_id args[] = {value, value};
        if (malformed != 1u && malformed != 2u) {
            yvex_ir_operation_request add = {.operation = "tensor.add", .operands = args, .operand_count = 2u,
                .result_types = &type, .result_count = 1u};
            rc = yvex_ir_operation_add(m, block, &add, &op, err);
            if (rc != YVEX_OK) break;
            value = yvex_ir_operation_at(m, op)->results[0];
        }
        yvex_ir_attribute tag = {.name = "tag", .kind = YVEX_IR_ATTR_U64, .value.integer = 11u + i};
        if (malformed == 4u) tag.kind = YVEX_IR_ATTR_BOOL;
        yvex_ir_operation_request observe = {.operation = "core.observe", .operands = &value,
            .operand_count = 1u, .attributes = &tag, .attribute_count = malformed == 5u ? 0u : 1u};
        if (malformed == 6u) { observe.result_types = &type; observe.result_count = 1u; }
        rc = yvex_ir_operation_add(m, block, &observe, &op, err);
    }
    if (rc == YVEX_OK) {
        yvex_ir_operation_request ret = {.operation = "core.return", .operands = &input, .operand_count = 1u};
        rc = yvex_ir_operation_add(m, block, &ret, &op, err);
    }
    if (rc == YVEX_OK) rc = yvex_ir_seal(m, err);
    yvex_ir_pass passes[] = {*yvex_ir_canonical_pass(), *yvex_ir_dead_code_pass()};
    if (rc == YVEX_OK) rc = yvex_ir_pass_pipeline(m, passes, 2u, &canonical, NULL, err);
    if (rc == YVEX_OK) rc = yvex_program_execution_compile(&execution, canonical, err);
    if (rc == YVEX_OK) rc = yvex_program_physical_compile(out, execution, "forward", NULL, 0u, source, err);
done:
    yvex_program_execution_close(&execution); yvex_ir_module_close(&canonical); yvex_ir_module_close(&m);
    return rc;
}

typedef struct {
    unsigned int calls, refuse_at, cancel_at;
    const float *input;
    float maximum;
} observation_fixture;

static int observation_publish(void *opaque, unsigned long long tag, const yvex_ir_type *type,
    unsigned long long rows, const float *values, unsigned long long count, yvex_error *err)
{
    observation_fixture *f = opaque;
    (void)rows;
    f->calls++;
    if (tag != 10u + f->calls || type->scalar != YVEX_IR_F32 || count != 12u) return YVEX_ERR_FORMAT;
    float scale = f->calls == 1u ? 2.0f : 4.0f;
    for (size_t i = 0u; i < count; ++i) {
        float difference = values[i] - scale * f->input[i];
        if (difference < 0.0f) difference = -difference;
        if (difference > f->maximum) f->maximum = difference;
    }
    if (f->refuse_at == f->calls) {
        yvex_error_set(err, YVEX_ERR_IO, "test.observation", "operator evidence sink refused publication");
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
}

static int observation_cancel(void *opaque)
{
    observation_fixture *f = opaque;
    return f->cancel_at && f->calls >= f->cancel_at;
}

static int test_observation_program(yvex_backend_kind kind)
{
    yvex_program_physical *program = NULL, *copy = NULL, *again = NULL;
    yvex_program_stage *stage = NULL;
    yvex_backend *backend = NULL;
    yvex_backend_options options = {.kind = kind};
    yvex_backend_operation_facts facts;
    yvex_backend_memory_stats before, after;
    yvex_error err = {0};
    yvex_core_bytes wire = {.maximum = 1048576u};
    for (unsigned int negative = 1u; negative <= 6u; ++negative)
        YVEX_TEST_ASSERT(observation_program(&program, negative, &err) != YVEX_OK && !program,
            "observation type/effect/tag/arity incompatibility refused during compilation");
    YVEX_TEST_ASSERT(observation_program(&program, 0u, &err) == YVEX_OK &&
        observation_program(&again, 0u, &err) == YVEX_OK &&
        yvex_program_physical_summary_get(program)->step_count == 4u &&
        !strcmp(yvex_program_physical_summary_get(program)->identity,
            yvex_program_physical_summary_get(again)->identity),
        "otherwise dead arithmetic and ordered observation survive canonical passes deterministically");
    YVEX_TEST_ASSERT(yvex_program_physical_encode(program, &wire, &err) == YVEX_OK &&
        yvex_program_physical_decode(&copy, wire.data, wire.count, &err) == YVEX_OK &&
        !strcmp(yvex_program_physical_summary_get(program)->identity,
            yvex_program_physical_summary_get(copy)->identity), "observation effects/tag survive binary import");
    YVEX_TEST_ASSERT(yvex_backend_open(&backend, &options, &err) == YVEX_OK &&
        yvex_backend_get_memory_stats(backend, &before, &err) == YVEX_OK &&
        yvex_program_stage_open(&stage, copy, NULL, 0u, backend, 1u, 1, 1048576u, 1048576u, &err) == YVEX_OK,
        "inspection stage reserves bounded scratch before execution");
    float input[12], output[12];
    for (size_t i = 0u; i < 12u; ++i) { input[i] = ((float)i - 6.0f) / 8.0f; output[i] = 777.0f; }
    const float *inputs[] = {input}; float *outputs[] = {output};
    YVEX_TEST_ASSERT(yvex_program_stage_host(stage, 1u, inputs, 1u, outputs, 1u,
        NULL, NULL, &facts, &err) == YVEX_ERR_STATE && facts.kernel_launches == 0u,
        "missing evidence owner fails before dispatch, never silently skips observation");
    observation_fixture fixture = {.input = input};
    yvex_program_observer observer = {observation_publish, &fixture};
    YVEX_TEST_ASSERT(yvex_program_stage_observe(stage, &observer, &err) == YVEX_OK,
        "attach borrowed observation owner");
    for (unsigned int run = 0u; run < 4u; ++run) {
        fixture.calls = 0u; fixture.refuse_at = run == 1u ? 1u : run == 2u ? 2u : 0u;
        fixture.cancel_at = run == 3u ? 1u : 0u;
        for (size_t i = 0u; i < 12u; ++i) output[i] = 777.0f;
        int rc = yvex_program_stage_host(stage, 1u, inputs, 1u, outputs, 1u,
            observation_cancel, &fixture, &facts, &err);
        YVEX_TEST_ASSERT(rc == (run == 0u ? YVEX_OK : run == 3u ? YVEX_ERR_CANCELLED : YVEX_ERR_IO) &&
            fixture.calls == (run == 1u || run == 3u ? 1u : 2u) && fixture.maximum == 0.0f,
            "typed observations are exact and sequential; early/late refusal and cancellation stop work");
        for (size_t i = 0u; i < 12u; ++i)
            YVEX_TEST_ASSERT(output[i] == (run ? 777.0f : input[i]), "no final tensor escapes failed observation");
    }
    YVEX_TEST_ASSERT(yvex_program_stage_observe(stage, NULL, &err) == YVEX_OK &&
        yvex_program_stage_host(stage, 1u, inputs, 1u, outputs, 1u, NULL, NULL, &facts, &err) == YVEX_ERR_STATE,
        "detaching a borrowed callback does not retain a stale observation context");
    YVEX_TEST_ASSERT(yvex_program_stage_close(&stage, &err) == YVEX_OK &&
        yvex_backend_get_memory_stats(backend, &after, &err) == YVEX_OK &&
        before.allocated_bytes == after.allocated_bytes && yvex_backend_close_checked(&backend, &err) == YVEX_OK,
        "observation scratch and execution storage return to baseline");
    printf("Ordered observations %s: 24 intermediate values max_abs=0 tolerance=0; six IR negatives; "
        "deterministic import; missing/detached sink refused; early/late refusal and cancellation unpublished; "
        "allocation_delta=0\n", kind == YVEX_BACKEND_KIND_CUDA ? "CUDA" : "CPU");
    yvex_program_physical_close(&program); yvex_program_physical_close(&copy);
    yvex_program_physical_close(&again); free(wire.data);
    return 0;
}
#endif
