/* Compiler-owned tensor work versus an independent scalar BF16 oracle. */
#include "tests/test.h"
#include "tests/support/signal_program.h"
#include "tests/support/spatial_program.h"
#include "tests/support/conditioning_program.h"
#include "tests/support/joint_program.h"
#include "tests/support/linear_program.h"
#include "tests/support/mhc_program.h"
#include "tests/support/mhc_ingress_program.h"
#include "tests/support/shared_expert_program.h"
#include "tests/support/mhc_cuda_control.h"
#include "tests/support/text_program.h"
#include "tests/support/population_program.h"
#include "tests/support/vision_program.h"
#include "tests/support/dense_program.h"
#include <yvex/internal/program_kernels.h>
#include <yvex/qtype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const yvex_component_encoded_weight *weights;
    uint32_t seen;
    int missing;
} text_binding_probe;

static int text_binding_name(void *opaque, unsigned long long id, char name[256], yvex_error *err)
{
    text_binding_probe *probe = opaque;
    (void)err;
    if (id >= 23u || (probe->seen & (1u << id))) return YVEX_ERR_FORMAT;
    probe->seen |= 1u << id;
    if (probe->missing && id == 22u) return YVEX_ERR_STATE;
    snprintf(name, 256u, "text_parameter_%llu", id);
    return YVEX_OK;
}

static int text_binding_weight(void *opaque, const char *name, yvex_component_encoded_weight *out, yvex_error *err)
{
    text_binding_probe *probe = opaque;
    unsigned long long id;
    char tail;
    (void)err;
    if (sscanf(name, "text_parameter_%llu%c", &id, &tail) != 1 || id >= 23u) return YVEX_ERR_FORMAT;
    *out = probe->weights[id];
    return YVEX_OK;
}

static int text_binding_workspace(void *opaque, unsigned long long bytes, yvex_error *err)
{
    (void)opaque; (void)bytes; (void)err;
    return YVEX_OK;
}

static int text_compiled_binding(const yvex_component_execution *original,
    const yvex_component_text_request *input, const yvex_component_encoded_weight *weights, const float *expected)
{
    text_binding_probe probe = {.weights = weights};
    yvex_component_execution component = *original;
    yvex_component_text_request request = *input;
    yvex_runtime_av_conditioning_result result;
    yvex_error err;
    component.materialization = (yvex_materialization_session *)&probe;
    component.owner_context = &probe;
    component.weight_view = text_binding_weight;
    component.workspace_reserve = text_binding_workspace;
    request.parameter_name = text_binding_name;
    request.parameter_context = &probe;
    YVEX_TEST_ASSERT(yvex_component_text_execute(&component, &request, &result, &err) == YVEX_OK &&
        result.complete && !*component.program_stage && result.hidden_width == 32u && result.layer_count == 2u &&
        probe.seen == (1u << 23u) - 1u && !memcmp(request.output, expected, 96u * sizeof(float)),
        "text binding uses each compiled ID once, preserves all results and derives report geometry from IR");
    for (unsigned int failure = 0u; failure < 6u; ++failure) {
        probe.seen = 0u; probe.missing = failure == 0u;
        request.program = failure == 1u ? NULL : input->program;
        request.parameter_name = failure == 2u ? NULL : text_binding_name;
        request.maximum_host_bytes = failure == 3u ? 1u : 0u;
        request.output_capacity = failure == 4u ? 95u : 96u;
        request.token_count = failure == 5u ? 4u : input->token_count;
        memset(request.output, 0x5a, 96u * sizeof(float));
        YVEX_TEST_ASSERT(yvex_component_text_execute(&component, &request, &result, &err) != YVEX_OK &&
            !result.complete && !*component.program_stage && ((unsigned char *)request.output)[0] == 0x5a,
            "missing binding/program/resolver, host budget, output and population bounds refuse unpublished");
    }
    printf("text_compiled_binding multimodal=%d parameters=23 source_recipe=absent role_resolver=absent "
        "values=96 max_abs=0 tolerance=0 negatives=6 unpublished=true\n", input->multimodal != NULL);
    return 0;
}

typedef struct {
    yvex_backend *backend;
    yvex_program_physical *plan;
    yvex_program_device *execution;
    yvex_program_kernels *kernels;
    yvex_device_tensor *weights, *input, *output;
    yvex_program_kernel_parameter parameters[3];
    yvex_program_device_argument argument;
    unsigned int linear_operations;
    float input_values[96];
    yvex_backend_memory_stats before;
} program_fixture;

static int program_fixture_compile(yvex_program_physical **out, yvex_error *err)
{
    const char *source = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    const char *names[] = {"gate", "up", "down"};
    yvex_ir_dialect dialects[] = {*yvex_ir_core_dialect(), *yvex_ir_neural_dialect()};
    yvex_ir_module *module = NULL;
    yvex_program_execution *execution = NULL;
    yvex_ir_dimension rows = {.name = "rows", .minimum = 1u, .maximum = 3u, .multiple = 1u};
    yvex_ir_type t = {.kind = YVEX_IR_TENSOR, .scalar = YVEX_IR_BF16, .rank = 2u};
    yvex_ir_id dimension, hidden, intermediate, types[3], parameters[3], function, op;
    yvex_ir_id block = YVEX_IR_NONE, input = YVEX_IR_NONE, values[4], operands[2];
    yvex_program_parameter_binding bindings[3];
    size_t i;
    int rc = yvex_ir_module_open(&module, "tensor_fixture", source, dialects, 2u, err);
    if (rc == YVEX_OK) rc = yvex_ir_dimension_add(module, &rows, &dimension, err);
    if (rc == YVEX_OK) {
        t.shape[0] = (yvex_ir_extent){dimension, 0u};
        t.shape[1] = (yvex_ir_extent){YVEX_IR_NONE, 32u};
        rc = yvex_ir_type_intern(module, &t, &hidden, err);
    }
    if (rc == YVEX_OK) {
        t.shape[1].extent = 64u;
        rc = yvex_ir_type_intern(module, &t, &intermediate, err);
    }
    for (i = 0u; rc == YVEX_OK && i < 3u; ++i) {
        t.shape[0] = (yvex_ir_extent){YVEX_IR_NONE, i == 2u ? 32u : 64u};
        t.shape[1].extent = i == 2u ? 64u : 32u;
        rc = yvex_ir_type_intern(module, &t, &types[i], err);
    }
    if (rc == YVEX_OK) rc = yvex_ir_function_add(module, "forward", &hidden, 1u, &hidden, 1u, 0u,
        &function, err);
    if (rc == YVEX_OK) {
        block = yvex_ir_function_at(module, function)->body;
        input = yvex_ir_block_at(module, block)->arguments[0];
    }
    for (i = 0u; rc == YVEX_OK && i < 3u; ++i) {
        yvex_ir_attribute attrs[] = {{.name = "parameter", .kind = YVEX_IR_ATTR_SYMBOL},
            {.name = "source", .kind = YVEX_IR_ATTR_TEXT}};
        yvex_ir_operation_request r = {.operation = "core.parameter", .result_types = &types[i],
            .result_count = 1u, .attributes = attrs, .attribute_count = 2u};
        snprintf(attrs[0].value.text, sizeof(attrs[0].value.text), "%s", names[i]);
        snprintf(attrs[1].value.text, sizeof(attrs[1].value.text), "%s", source);
        rc = yvex_ir_operation_add(module, block, &r, &op, err);
        if (rc == YVEX_OK) {
            parameters[i] = yvex_ir_operation_at(module, op)->results[0];
            bindings[i] = (yvex_program_parameter_binding){parameters[i], i, YVEX_GGUF_QTYPE_BF16};
        }
    }
    for (i = 0u; rc == YVEX_OK && i < 4u; ++i) {
        yvex_ir_operation_request r = {.operation = i == 2u ? "nn.silu_product" : "nn.linear",
            .operands = operands, .operand_count = 2u, .result_types = i == 3u ? &hidden : &intermediate,
            .result_count = 1u};
        operands[0] = i < 2u ? input : i == 2u ? values[0] : values[2];
        operands[1] = i < 2u ? parameters[i] : i == 2u ? values[1] : parameters[2];
        rc = yvex_ir_operation_add(module, block, &r, &op, err);
        if (rc == YVEX_OK) values[i] = yvex_ir_operation_at(module, op)->results[0];
    }
    if (rc == YVEX_OK) {
        yvex_ir_operation_request r = {.operation = "core.return", .operands = &values[3], .operand_count = 1u};
        rc = yvex_ir_operation_add(module, block, &r, &op, err);
    }
    if (rc == YVEX_OK) rc = yvex_ir_seal(module, err);
    if (rc == YVEX_OK) rc = yvex_program_execution_compile(&execution, module, err);
    if (rc == YVEX_OK) rc = yvex_program_physical_compile(out, execution, "forward", bindings, 3u, source, err);
    yvex_program_execution_close(&execution);
    yvex_ir_module_close(&module);
    return rc;
}

static int program_invoke(void *context, const yvex_program_device_invocation *r,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    program_fixture *f = context;
    int rc = yvex_program_kernels_invoke(f->kernels, r, facts, err);
    if (rc == YVEX_OK && !strcmp(r->step->implementation, "linear.bf16.f32acc.v1")) f->linear_operations++;
    return rc;
}

static const yvex_program_device_kernel implementations[] = {
    {"linear.bf16.f32acc.v1", program_invoke}, {"silu_product.bf16.v1", program_invoke}
};

static float scalar_bf16(float x)
{
    uint32_t bits;
    memcpy(&bits, &x, sizeof(bits));
    bits = (bits + 0x7fffu + ((bits >> 16u) & 1u)) & 0xffff0000u;
    memcpy(&x, &bits, sizeof(x));
    return x;
}

static int program_cuda_prepare(program_fixture *f)
{
    yvex_error err;
    yvex_backend_tensor_desc desc = {.name = "program-fixture-parameter", .dtype = YVEX_DTYPE_I8, .rank = 1u};
    unsigned char *arena = NULL;
    unsigned int i, row;
    YVEX_TEST_ASSERT(yvex_backend_get_memory_stats(f->backend, &f->before, &err) == YVEX_OK,
        "observe memory before physical program ownership");
    desc.dims[0] = desc.bytes = 3u * 32u * 64u * 2u;
    YVEX_TEST_ASSERT(yvex_backend_resident_alloc(f->backend, &desc, &f->weights, &arena, &err) == YVEX_OK &&
        yvex_backend_resident_attach(f->backend, arena, desc.bytes, f->weights, 1ull, &err) == YVEX_OK,
        "one admitted parameter arena owns all physical weight subranges");
    for (i = 0u; i < 3u; ++i) {
        unsigned char *mapped = arena + i * 32u * 64u * 2u;
        unsigned int rows = i == 2u ? 32u : 64u, width = i == 2u ? 64u : 32u;
        size_t bytes = rows * width * 2u;
        memset(mapped, 0, bytes);
        for (row = 0u; row < rows; ++row) {
            size_t offset = (row * width + row % 32u) * 2u;
            mapped[offset] = 0x80u;
            mapped[offset + 1u] = 0x3fu; /* Exact BF16 1; down selects the first bank. */
        }
        f->parameters[i].tensor_id = i;
        f->parameters[i].weight = (yvex_component_encoded_weight){.encoded = mapped, .encoded_bytes = bytes,
            .row_count = rows, .row_width = width, .row_bytes = width * 2u, .qtype = YVEX_GGUF_QTYPE_BF16};
    }
    desc = (yvex_backend_tensor_desc){.name = "program-fixture-activation", .dtype = YVEX_DTYPE_F32,
        .rank = 2u, .dims = {3u, 32u}, .bytes = sizeof(f->input_values)};
    for (i = 0u; i < 96u; ++i) f->input_values[i] = (float)((int)(i % 17u) - 8) / 8.0f;
    YVEX_TEST_ASSERT(yvex_backend_tensor_alloc(f->backend, &desc, &f->input, &err) == YVEX_OK &&
        yvex_backend_tensor_alloc(f->backend, &desc, &f->output, &err) == YVEX_OK &&
        yvex_backend_tensor_write(f->backend, f->input, f->input_values, desc.bytes, &err) == YVEX_OK,
        "bind rank-2 input and disjoint output");
    f->argument.tensor = f->input;
    YVEX_TEST_ASSERT(program_fixture_compile(&f->plan, &err) == YVEX_OK &&
        yvex_program_kernels_open(&f->kernels, f->plan, f->parameters, 3u, f->backend, 0u, 0u, &err) == YVEX_OK &&
        yvex_program_device_open(&f->execution, f->plan, f->backend, 3u, 0u, 0u,
            implementations, 2u, f, &err) == YVEX_OK &&
        yvex_program_kernels_prepare(f->kernels, 1u, 0u, 0u, &err) == YVEX_OK &&
        yvex_program_kernels_prepare(f->kernels, 3u, 0u, 0u, &err) == YVEX_OK,
        "lowered program prepares exact single and multi-row populations");
    return 0;
}

static int program_cuda_execute(program_fixture *f)
{
    yvex_error err;
    yvex_program_device_result result;
    yvex_program_kernels *refused = NULL;
    yvex_device_tensor *outputs[] = {f->output};
    float observed[96];
    const unsigned long long population[] = {3u, 1u, 3u, 1u};
    unsigned int run, i;
    float maximum = 0.0f;
    int rc;
    for (run = 0u; run < 4u; ++run) {
        unsigned long long rows = population[run];
        yvex_device_tensor output;
        YVEX_TEST_ASSERT(yvex_backend_tensor_f32_subview(f->output, 0u, rows * 32u, &output),
            "caller binds only the actual output population");
        output.rank = 2u;
        output.dims[0] = rows;
        output.dims[1] = 32u;
        outputs[0] = &output;
        f->linear_operations = 0u;
        rc = yvex_program_device_run(f->execution, rows, &f->argument, 1u, outputs, 1u, NULL, NULL, &result, &err);
        if (rc != YVEX_OK) fprintf(stderr, "program execution: %s\n", yvex_error_message(&err));
        YVEX_TEST_ASSERT(rc == YVEX_OK && result.operations == 4u && f->linear_operations == 3u,
            "four lowered operations execute without a decoder or family selector");
        YVEX_TEST_ASSERT(yvex_backend_tensor_read(f->backend, &output, observed, rows * 32u * sizeof(float),
            &err) == YVEX_OK, "read exact physical output");
        for (i = 0u; i < rows * 32u; ++i) {
            float x = f->input_values[i];
            float activated = scalar_bf16(x / (1.0f + expf(-x)));
            float expected = scalar_bf16(activated * x);
            float error = fabsf(observed[i] - expected);
            if (error > maximum) maximum = error;
            if (!isfinite(observed[i]) || error != 0.0f)
                fprintf(stderr, "tensor oracle row=%llu index=%u input=%g expected=%g observed=%g abs=%g\n",
                        rows, i, (double)x, (double)expected, (double)observed[i], (double)error);
            YVEX_TEST_ASSERT(isfinite(observed[i]) && error == 0.0f,
                "CUDA program equals the independent BF16 scalar fixture exactly");
        }
    }
    YVEX_TEST_ASSERT(setenv("YVEX_TEST_CUDA_LINEAR_PLAN_FAILURE", "compile", 1) == 0,
        "make any repeated specialization fail observably");
    rc = yvex_program_kernels_prepare(f->kernels, 1u, 0u, 0u, &err);
    if (rc == YVEX_OK) rc = yvex_program_kernels_prepare(f->kernels, 3u, 0u, 0u, &err);
    YVEX_TEST_ASSERT(unsetenv("YVEX_TEST_CUDA_LINEAR_PLAN_FAILURE") == 0 && rc == YVEX_OK,
        "already admitted populations do not repeat backend specialization");
    outputs[0] = f->input;
    YVEX_TEST_ASSERT(yvex_program_device_run(f->execution, 1u, &f->argument, 1u, outputs, 1u,
        NULL, NULL, &result, &err) == YVEX_ERR_FORMAT && !result.operations, "aliasing refuses before backend work");
    outputs[0] = f->output;
    f->input->dims[1] = 16u;
    YVEX_TEST_ASSERT(yvex_program_device_run(f->execution, 1u, &f->argument, 1u, outputs, 1u,
        NULL, NULL, &result, &err) == YVEX_ERR_FORMAT && !result.operations,
        "wrong logical shape refuses despite sufficient bytes");
    f->input->dims[1] = 32u;
    f->parameters[0].weight.qtype = YVEX_GGUF_QTYPE_F32;
    YVEX_TEST_ASSERT(yvex_program_kernels_open(&refused, f->plan, f->parameters, 3u, f->backend, 0u, 0u,
        &err) == YVEX_ERR_FORMAT && !refused, "unadmitted physical representation refuses at cold binding");
    f->parameters[0].weight.qtype = YVEX_GGUF_QTYPE_BF16;
    YVEX_TEST_ASSERT(yvex_program_kernels_prepare(f->kernels, 4u, 0u, 0u, &err) == YVEX_ERR_BOUNDS,
        "population outside the compiler envelope refuses");
    YVEX_TEST_ASSERT(yvex_program_device_run(f->execution, 2u, &f->argument, 1u, outputs, 1u,
        NULL, NULL, &result, &err) == YVEX_ERR_STATE && !result.operations, "unprepared population refuses");
    printf("Tensor program CUDA: scalar BF16 oracle; rows=3,1,3,1 values=256 max_abs=%g tolerance=0; "
           "steps/run=4 linear/run=3 cached preparations=1,3; alias/shape/qtype/population negatives=5\n",
           (double)maximum);
    return 0;
}

static int program_cuda_failures(program_fixture *f)
{
    yvex_program_device *limited = NULL;
    yvex_program_device_result result;
    yvex_device_tensor output, *outputs[] = {&output};
    yvex_backend_memory_stats before, after;
    yvex_error err;
    int rc;
    YVEX_TEST_ASSERT(yvex_backend_get_memory_stats(f->backend, &before, &err) == YVEX_OK &&
        yvex_program_device_open(&limited, f->plan, f->backend, 3u, 1u, 0u,
            implementations, 2u, f, &err) == YVEX_ERR_BOUNDS &&
        !limited, "host budget refuses before allocating an execution owner");
    YVEX_TEST_ASSERT(yvex_program_device_open(&limited, f->plan, f->backend, 3u, 0u, 1u,
        implementations, 2u, f, &err) ==
        YVEX_ERR_BOUNDS && !limited &&
        yvex_backend_get_memory_stats(f->backend, &after, &err) == YVEX_OK &&
        before.allocated_bytes == after.allocated_bytes,
        "device budget refusal discharges partial execution construction");
    YVEX_TEST_ASSERT(yvex_backend_tensor_f32_subview(f->output, 0u, 64u, &output),
        "failure test binds two actual output rows");
    output.rank = 2u;
    output.dims[0] = 2u;
    output.dims[1] = 32u;
    YVEX_TEST_ASSERT(setenv("YVEX_TEST_CUDA_LINEAR_PLAN_FAILURE", "compile", 1) == 0,
        "activate the existing backend specialization failure probe");
    rc = yvex_program_kernels_prepare(f->kernels, 2u, 0u, 0u, &err);
    YVEX_TEST_ASSERT(unsetenv("YVEX_TEST_CUDA_LINEAR_PLAN_FAILURE") == 0,
        "remove specialization failure probe");
    YVEX_TEST_ASSERT(rc == YVEX_ERR_BACKEND &&
        yvex_program_device_run(f->execution, 2u, &f->argument, 1u, outputs, 1u, NULL, NULL, &result, &err) ==
        YVEX_ERR_STATE && !result.operations,
        "failed specialization never publishes a runnable population");
    YVEX_TEST_ASSERT(yvex_program_kernels_prepare(f->kernels, 2u, 0u, 0u, &err) == YVEX_OK,
        "exact population can be prepared after failure without reopening the owner");
    YVEX_TEST_ASSERT(setenv("YVEX_TEST_CUDA_LINEAR_PLAN_FAILURE", "execute", 1) == 0,
        "activate the existing backend execution failure probe");
    output.is_written = 1;
    rc = yvex_program_device_run(f->execution, 2u, &f->argument, 1u, outputs, 1u, NULL, NULL, &result, &err);
    YVEX_TEST_ASSERT(unsetenv("YVEX_TEST_CUDA_LINEAR_PLAN_FAILURE") == 0,
        "remove execution failure probe");
    YVEX_TEST_ASSERT(rc == YVEX_ERR_BACKEND && !result.operations && !output.is_written,
        "backend failure invalidates output publication and preserves exact refusal");
    YVEX_TEST_ASSERT(yvex_program_device_run(f->execution, 2u, &f->argument, 1u, outputs, 1u,
        NULL, NULL, &result, &err) == YVEX_OK && result.operations == 4u && output.is_written,
        "owner remains reusable after backend refusal");
    puts("Tensor program failures: host/device budgets refuse without leaks; compile failure -> non-runnable; "
         "execute failure -> unpublished output; retry -> 4 operations; all probes removed");
    return 0;
}

static int program_cuda_text(void)
{
    yvex_backend *backend = NULL;
    yvex_backend_options options = {.kind = YVEX_BACKEND_KIND_CUDA};
    yvex_device_tensor *resident = NULL;
    unsigned char *arena = NULL;
    yvex_program_physical *program = NULL;
    yvex_component_encoded_weight weights[23] = {0};
    unsigned long long rows[] = {13u, 1u, 32u, 16u, 16u, 32u, 1u, 1u, 1u, 64u, 64u, 32u};
    unsigned long long widths[] = {32u, 32u, 32u, 32u, 32u, 32u, 8u, 8u, 32u, 32u, 32u, 64u};
    unsigned int tokens[] = {1u, 4u, 7u};
    float expected[96], observed[96], maximum = 0.0f;
    yvex_backend_text_execution_result result = {0};
    yvex_program_stage *retained_stage = NULL;
    yvex_component_execution component = {.schema_version = YVEX_COMPONENT_EXECUTION_SCHEMA_V2,
        .program_stage = &retained_stage};
    yvex_component_text_request request = {.token_ids = tokens,
        .token_count = 3u, .output = observed, .output_capacity = 96u};
    yvex_backend_tensor_desc descriptor = {.name = "text-program-weights", .dtype = YVEX_DTYPE_I8, .rank = 1u};
    yvex_backend_memory_stats before, after;
    yvex_error err;
    int rc = yvex_backend_open(&backend, &options, &err);
    if (rc == YVEX_ERR_UNSUPPORTED) return 77;
    YVEX_TEST_ASSERT(rc == YVEX_OK && yvex_backend_get_memory_stats(backend, &before, &err) == YVEX_OK,
        "observe text program backend allocation baseline");
    for (size_t i = 0u; i < 23u; ++i) {
        size_t slot = i ? (i - 1u) % 11u + 1u : 0u;
        weights[i] = (yvex_component_encoded_weight){.row_count = rows[slot], .row_width = widths[slot],
            .row_bytes = widths[slot] * 2u, .encoded_bytes = rows[slot] * widths[slot] * 2u,
            .qtype = YVEX_GGUF_QTYPE_BF16};
        descriptor.bytes += weights[i].encoded_bytes;
    }
    descriptor.dims[0] = descriptor.bytes;
    YVEX_TEST_ASSERT(yvex_backend_resident_alloc(backend, &descriptor, &resident, &arena, &err) == YVEX_OK &&
        yvex_backend_resident_attach(backend, arena, descriptor.bytes, resident, 1u, &err) == YVEX_OK,
        "one exact BF16 arena supplies both execution paths");
    unsigned long long offset = 0u;
    for (size_t i = 0u; i < 23u; ++i) {
        weights[i].encoded = arena + offset;
        for (unsigned long long j = 0u; j < weights[i].encoded_bytes / 2u; ++j) {
            float x = weights[i].row_count == 1u ? 1.0f + (float)(j % 3u) / 8.0f :
                (float)((int)((j + i * 3u) % 17u) - 8) / 32.0f;
            uint32_t bits;
            memcpy(&bits, &x, sizeof(bits));
            arena[offset + 2u * j] = (unsigned char)(bits >> 16u);
            arena[offset + 2u * j + 1u] = (unsigned char)(bits >> 24u);
        }
        offset += weights[i].encoded_bytes;
    }
    memcpy(expected, test_text_preserved, sizeof(expected));
    YVEX_TEST_ASSERT(yvex_text_program_compile(&program, &test_text_recipe, 2u, 3u, NULL, 0u, &err) == YVEX_OK,
        "compile complete text stack before runtime");
    request.program = program;
    component.backend = backend; component.resident_encoded_bytes = descriptor.bytes;
    snprintf(component.residency_identity, sizeof(component.residency_identity), "%s", test_text_recipe.semantic_identity);
    rc = yvex_component_text_program_execute(&component, &request, weights, 23u, &result, &err);
    if (rc != YVEX_OK) fprintf(stderr, "text program CUDA: %s\n", yvex_error_message(&err));
    YVEX_TEST_ASSERT(rc == YVEX_OK && !retained_stage && result.complete && result.token_count == 3u &&
        result.hidden_width == 32u && result.layer_count == 2u, "complete component executes through physical SSA");
    for (size_t i = 0u; i < 96u; ++i) {
        float error = fabsf(observed[i] - expected[i]);
        if (error > maximum) maximum = error;
        if (!isfinite(observed[i]) || error != 0.0f)
            fprintf(stderr, "text preservation index=%zu expected=%.9g observed=%.9g abs=%.9g\n",
                i, (double)expected[i], (double)observed[i], (double)error);
        YVEX_TEST_ASSERT(isfinite(observed[i]) && error == 0.0f, "text migration preserves each BF16 result exactly");
    }
    printf("Text program CUDA: layers=2 tokens=3 values=96 old[0]=%.9g new[0]=%.9g max_abs=%g tolerance=0; "
        "retired launches=69 SSA launches=%llu; cross-implementation preservation, not upstream conformance\n",
        (double)expected[0], (double)observed[0], (double)maximum, result.kernel_launches);
    if (text_compiled_binding(&component, &request, weights, expected)) return 1;
    for (unsigned int failure = 0u; failure < 5u; ++failure) {
        memset(observed, 0x5a, sizeof(observed));
        if (failure < 2u) YVEX_TEST_ASSERT(setenv("YVEX_TEST_CUDA_LINEAR_PLAN_FAILURE",
            failure ? "execute" : "compile", 1) == 0, "arm component operation failure");
        if (failure == 2u) request.maximum_host_bytes = 1u;
        if (failure == 3u) request.maximum_device_bytes = 1u;
        if (failure == 4u) request.cancelled = test_mhc_cancel;
        rc = yvex_component_text_program_execute(&component, &request, weights, 23u, &result, &err);
        YVEX_TEST_ASSERT(rc != YVEX_OK && !retained_stage && !result.complete &&
            ((unsigned char *)observed)[0] == 0x5a,
            "compile/execute/budget/cancellation failures publish no component result and discharge resources");
        if (failure < 2u) YVEX_TEST_ASSERT(unsetenv("YVEX_TEST_CUDA_LINEAR_PLAN_FAILURE") == 0,
            "disarm component operation failure");
        request.maximum_host_bytes = request.maximum_device_bytes = 0u;
        request.cancelled = NULL;
    }
    yvex_program_kernel_parameter bindings[23];
    for (size_t i = 0u; i < 23u; ++i)
        bindings[i] = (yvex_program_kernel_parameter){.tensor_id = i, .weight = weights[i]};
    YVEX_TEST_ASSERT(yvex_program_stage_open(&retained_stage, program, bindings, 23u,
        backend, 3u, 1, 0u, 0u, &err) == YVEX_OK, "stage retains sealed program lifetime");
    yvex_program_physical_close(&program);
    unsigned int regular_positions[] = {0u, 1u, 2u};
    yvex_program_host_input arguments[] = {{.indices = tokens}, {.indices = regular_positions},
        {.indices = regular_positions}, {.indices = regular_positions}};
    yvex_backend_operation_facts facts;
    YVEX_TEST_ASSERT(yvex_program_stage_host_inputs(retained_stage, 3u, arguments, 4u,
        (float *[]){observed}, 1u, NULL, NULL, &facts, &err) == YVEX_OK &&
        !memcmp(test_text_preserved, observed, sizeof(observed)) &&
        yvex_program_stage_close(&retained_stage, &err) == YVEX_OK && !retained_stage,
        "compiled text executes exactly after importer relinquishes its program reference");
    unsigned long long positions[] = {0u, 1u, 2u, 0u, 2u, 3u, 0u, 3u, 4u};
    unsigned int visual_index = 1u;
    float visual[32], deepstack[96];
    yvex_backend_text_multimodal_input multimodal = {.position_ids = positions, .position_capacity = 9u,
        .visual_token_indices = &visual_index, .visual_token_count = 1u, .visual_embeddings = visual,
        .visual_embedding_capacity = 32u, .deepstack_embeddings = deepstack, .deepstack_layer_count = 3u,
        .deepstack_embedding_capacity = 96u, .mrope_sections = {2u, 1u, 1u},
        .vision_execution_identity = test_text_recipe.semantic_identity};
    for (size_t i = 0u; i < 32u; ++i) visual[i] = (float)((int)(i % 11u) - 5) / 16.0f;
    for (size_t i = 0u; i < 96u; ++i) deepstack[i] = (float)((int)(i % 7u) - 3) / 32.0f;
    memcpy(expected, test_multimodal_preserved, sizeof(expected));
    YVEX_TEST_ASSERT(yvex_text_program_compile(&program, &test_text_recipe, 2u, 3u,
        multimodal.mrope_sections, 3u, &err) == YVEX_OK, "visual row and layer interactions compile into SSA");
    request.program = program; request.multimodal = &multimodal;
    rc = yvex_component_text_program_execute(&component, &request, weights, 23u, &result, &err);
    if (rc != YVEX_OK) fprintf(stderr, "multimodal text program: %s\n", yvex_error_message(&err));
    YVEX_TEST_ASSERT(rc == YVEX_OK && result.complete && !memcmp(expected, observed, sizeof(expected)),
        "multimodal embedding replacement, per-axis RoPE and deepstack additions preserve every output bit");
    printf("Multimodal text CUDA: layers=2 tokens=3 visual_rows=1 results=96 expected[0]=%.9g observed[0]=%.9g "
        "max_abs=0 tolerance=0; explicit initial replacement and two layer injections\n",
        (double)expected[0], (double)observed[0]);
    if (text_compiled_binding(&component, &request, weights, expected)) return 1;
    visual_index = 3u;
    memset(observed, 0x5a, sizeof(observed));
    YVEX_TEST_ASSERT(yvex_component_text_program_execute(&component, &request, weights, 23u, &result, &err) ==
        YVEX_ERR_FORMAT && !result.complete && ((unsigned char *)observed)[0] == 0x5a,
        "visual index outside executable population refuses without publication");
    yvex_program_physical_close(&program);
    YVEX_TEST_ASSERT(yvex_backend_resident_detach(backend, &err) == YVEX_OK &&
        yvex_backend_tensor_release(backend, &resident, &err) == YVEX_OK &&
        yvex_backend_get_memory_stats(backend, &after, &err) == YVEX_OK &&
        after.allocated_bytes == before.allocated_bytes && yvex_backend_close_checked(&backend, &err) == YVEX_OK,
        "component program preparation, invocation and residency return to allocation baseline");
    return 0;
}

int yvex_cuda_test_program(void)
{
    if (test_conditioning_program(YVEX_BACKEND_KIND_CUDA)) return 1;
    if (test_joint_compiler() || test_joint_execution()) return 1;
    if (test_spatial_programs(YVEX_BACKEND_KIND_CUDA) ||
        test_signal_programs(YVEX_BACKEND_KIND_CUDA) || test_dense_program(YVEX_BACKEND_KIND_CUDA) ||
        test_vision_program() || test_program_populations(YVEX_BACKEND_KIND_CUDA, 0) ||
        test_program_populations(YVEX_BACKEND_KIND_CUDA, 1) ||
        test_program_index_values(YVEX_BACKEND_KIND_CUDA)) return 1;
    int text = program_cuda_text();
    if (text) return text;
    if (test_mhc_execute(YVEX_BACKEND_KIND_CUDA) != 0) return 1;
    if (test_shared_expert(YVEX_BACKEND_KIND_CUDA)) return 1;
    if (test_shared_target(YVEX_BACKEND_KIND_CUDA)) return 1;
    if (test_ingress_execute(YVEX_BACKEND_KIND_CUDA) != 0) return 1;
    if (test_mhc_cuda_control(YVEX_GGUF_QTYPE_F32) != 0 ||
        test_mhc_cuda_control(YVEX_GGUF_QTYPE_BF16) != 0) return 1;
    if (test_post_execute(YVEX_BACKEND_KIND_CUDA) != 0) return 1;
    if (test_stream_mean_execute(YVEX_BACKEND_KIND_CUDA) != 0) return 1;
    if (test_linear_execute(YVEX_BACKEND_KIND_CUDA) != 0) return 1;
    if (test_linear_residual_execute(YVEX_BACKEND_KIND_CUDA) != 0) return 1;
    program_fixture f = {0};
    yvex_backend_options options = {.kind = YVEX_BACKEND_KIND_CUDA};
    yvex_backend_memory_stats after;
    yvex_error err;
    int rc = yvex_backend_open(&f.backend, &options, &err);
    if (rc == YVEX_ERR_UNSUPPORTED) return 77;
    YVEX_TEST_ASSERT(rc == YVEX_OK && program_cuda_prepare(&f) == 0 && program_cuda_execute(&f) == 0 &&
        program_cuda_failures(&f) == 0,
        "physical program executes under a real CUDA backend");
    YVEX_TEST_ASSERT(yvex_program_device_close(&f.execution, &err) == YVEX_OK && !f.execution &&
        yvex_program_kernels_close(&f.kernels, &err) == YVEX_OK && !f.kernels,
        "close discharges prepared program ownership");
    yvex_program_physical_close(&f.plan);
    YVEX_TEST_ASSERT(yvex_backend_resident_detach(f.backend, &err) == YVEX_OK &&
        yvex_backend_tensor_release(f.backend, &f.weights, &err) == YVEX_OK, "parameter owner releases");
    YVEX_TEST_ASSERT(yvex_backend_tensor_release(f.backend, &f.input, &err) == YVEX_OK &&
        yvex_backend_tensor_release(f.backend, &f.output, &err) == YVEX_OK &&
        yvex_backend_get_memory_stats(f.backend, &after, &err) == YVEX_OK &&
        after.allocated_bytes == f.before.allocated_bytes,
        "physical allocations return to baseline after program cleanup");
    YVEX_TEST_ASSERT(yvex_backend_close_checked(&f.backend, &err) == YVEX_OK && !f.backend,
        "backend owner closes after every borrowed program resource");
    return 0;
}
