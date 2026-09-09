/* Compiler-owned tensor work versus an independent scalar BF16 oracle. */
#include "tests/test.h"
#include "tests/support/tensor_program.h"
#include <yvex/internal/tensor_execution.h>
#include <yvex/qtype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    yvex_backend *backend;
    yvex_program_tensor_plan *plan;
    yvex_tensor_execution *execution;
    yvex_device_tensor *weights, *input, *output;
    yvex_component_encoded_weight encoded[3];
    yvex_tensor_execution_argument arguments[4];
    float input_values[96];
    yvex_backend_memory_stats before;
} program_fixture;

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
        f->encoded[i] = (yvex_component_encoded_weight){.encoded = mapped, .encoded_bytes = bytes,
            .row_count = rows, .row_width = width, .row_bytes = width * 2u, .qtype = YVEX_GGUF_QTYPE_BF16};
        f->arguments[i + 1u].parameter = &f->encoded[i];
    }
    desc = (yvex_backend_tensor_desc){.name = "program-fixture-activation", .dtype = YVEX_DTYPE_F32,
        .rank = 2u, .dims = {3u, 32u}, .bytes = sizeof(f->input_values)};
    for (i = 0u; i < 96u; ++i) f->input_values[i] = (float)((int)(i % 17u) - 8) / 8.0f;
    YVEX_TEST_ASSERT(yvex_backend_tensor_alloc(f->backend, &desc, &f->input, &err) == YVEX_OK &&
        yvex_backend_tensor_alloc(f->backend, &desc, &f->output, &err) == YVEX_OK &&
        yvex_backend_tensor_write(f->backend, f->input, f->input_values, desc.bytes, &err) == YVEX_OK,
        "bind rank-2 input and disjoint output");
    f->arguments[0].tensor = f->input;
    YVEX_TEST_ASSERT(test_tensor_program(&f->plan, 0, &err) == YVEX_OK &&
        yvex_tensor_execution_open(&f->execution, f->plan, f->backend, 3u, 0ull, 0ull, &err) == YVEX_OK &&
        yvex_tensor_execution_prepare(f->execution, 1u, &err) == YVEX_OK &&
        yvex_tensor_execution_prepare(f->execution, 3u, &err) == YVEX_OK,
        "lowered program prepares exact single and multi-row populations");
    return 0;
}

static int program_cuda_execute(program_fixture *f)
{
    yvex_error err;
    yvex_tensor_execution_result result;
    yvex_device_tensor *outputs[] = {f->output};
    float observed[96];
    const unsigned long long population[] = {3u, 1u, 3u, 1u};
    unsigned long long prepared = yvex_tensor_execution_resources_get(f->execution)->preparation_count;
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
        rc = yvex_tensor_execution_run(f->execution, rows, f->arguments, 4u, outputs, 1u, &result, &err);
        if (rc != YVEX_OK) fprintf(stderr, "program execution: %s\n", yvex_error_message(&err));
        YVEX_TEST_ASSERT(rc == YVEX_OK && result.operations == 4u && result.linear_operations == 3u,
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
    YVEX_TEST_ASSERT(yvex_tensor_execution_resources_get(f->execution)->preparation_count == prepared,
        "repeated admitted populations do not repeat backend specialization");
    outputs[0] = f->input;
    YVEX_TEST_ASSERT(yvex_tensor_execution_run(f->execution, 1u, f->arguments, 4u, outputs, 1u,
        &result, &err) == YVEX_ERR_FORMAT && !result.operations, "aliasing refuses before backend work");
    outputs[0] = f->output;
    f->input->dims[1] = 16u;
    YVEX_TEST_ASSERT(yvex_tensor_execution_run(f->execution, 1u, f->arguments, 4u, outputs, 1u,
        &result, &err) == YVEX_ERR_FORMAT && !result.operations, "wrong logical shape refuses despite sufficient bytes");
    f->input->dims[1] = 32u;
    f->encoded[0].qtype = YVEX_GGUF_QTYPE_F32;
    YVEX_TEST_ASSERT(yvex_tensor_execution_run(f->execution, 1u, f->arguments, 4u, outputs, 1u,
        &result, &err) == YVEX_ERR_FORMAT && !result.operations, "unadmitted physical representation refuses");
    f->encoded[0].qtype = YVEX_GGUF_QTYPE_BF16;
    YVEX_TEST_ASSERT(yvex_tensor_execution_prepare(f->execution, 4u, &err) == YVEX_ERR_BOUNDS,
        "population outside the compiler envelope refuses");
    YVEX_TEST_ASSERT(yvex_tensor_execution_run(f->execution, 2u, f->arguments, 4u, outputs, 1u,
        &result, &err) == YVEX_ERR_STATE && !result.operations, "unprepared population refuses");
    printf("Tensor program CUDA: scalar BF16 oracle; rows=3,1,3,1 values=256 max_abs=%g tolerance=0; "
           "steps/run=4 linear/run=3 preparations=%llu; alias/shape/qtype/population negatives=5\n",
           (double)maximum, prepared);
    return 0;
}

static int program_cuda_failures(program_fixture *f)
{
    yvex_tensor_execution *limited = NULL;
    yvex_tensor_execution_result result;
    yvex_device_tensor output, *outputs[] = {&output};
    yvex_backend_memory_stats before, after;
    yvex_error err;
    int rc;
    YVEX_TEST_ASSERT(yvex_backend_get_memory_stats(f->backend, &before, &err) == YVEX_OK &&
        yvex_tensor_execution_open(&limited, f->plan, f->backend, 3u, 1u, 0u, &err) == YVEX_ERR_BOUNDS &&
        !limited, "host budget refuses before allocating an execution owner");
    YVEX_TEST_ASSERT(yvex_tensor_execution_open(&limited, f->plan, f->backend, 3u, 0u, 1u, &err) ==
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
    rc = yvex_tensor_execution_prepare(f->execution, 2u, &err);
    YVEX_TEST_ASSERT(unsetenv("YVEX_TEST_CUDA_LINEAR_PLAN_FAILURE") == 0,
        "remove specialization failure probe");
    YVEX_TEST_ASSERT(rc == YVEX_ERR_BACKEND &&
        yvex_tensor_execution_run(f->execution, 2u, f->arguments, 4u, outputs, 1u, &result, &err) ==
        YVEX_ERR_STATE && !result.operations,
        "failed specialization never publishes a runnable population");
    YVEX_TEST_ASSERT(yvex_tensor_execution_prepare(f->execution, 2u, &err) == YVEX_OK,
        "exact population can be prepared after failure without reopening the owner");
    YVEX_TEST_ASSERT(setenv("YVEX_TEST_CUDA_LINEAR_PLAN_FAILURE", "execute", 1) == 0,
        "activate the existing backend execution failure probe");
    output.is_written = 1;
    rc = yvex_tensor_execution_run(f->execution, 2u, f->arguments, 4u, outputs, 1u, &result, &err);
    YVEX_TEST_ASSERT(unsetenv("YVEX_TEST_CUDA_LINEAR_PLAN_FAILURE") == 0,
        "remove execution failure probe");
    YVEX_TEST_ASSERT(rc == YVEX_ERR_BACKEND && !result.operations && !output.is_written,
        "backend failure invalidates output publication and preserves exact refusal");
    YVEX_TEST_ASSERT(yvex_tensor_execution_run(f->execution, 2u, f->arguments, 4u, outputs, 1u,
        &result, &err) == YVEX_OK && result.operations == 4u && output.is_written,
        "owner remains reusable after backend refusal");
    puts("Tensor program failures: host/device budgets refuse without leaks; compile failure -> non-runnable; "
         "execute failure -> unpublished output; retry -> 4 operations; all probes removed");
    return 0;
}

int yvex_cuda_test_program(void)
{
    program_fixture f = {0};
    yvex_backend_options options = {.kind = YVEX_BACKEND_KIND_CUDA};
    yvex_backend_memory_stats after;
    yvex_error err;
    int rc = yvex_backend_open(&f.backend, &options, &err);
    if (rc == YVEX_ERR_UNSUPPORTED) return 77;
    YVEX_TEST_ASSERT(rc == YVEX_OK && program_cuda_prepare(&f) == 0 && program_cuda_execute(&f) == 0 &&
        program_cuda_failures(&f) == 0,
        "physical program executes under a real CUDA backend");
    YVEX_TEST_ASSERT(yvex_tensor_execution_close(&f.execution, &err) == YVEX_OK && !f.execution,
        "close discharges prepared program ownership");
    yvex_program_tensor_close(&f.plan);
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
