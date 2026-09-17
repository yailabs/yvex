/* Dense component cutover: retained CUDA values plus independent final-norm oracle. */
#ifndef TESTS_SUPPORT_DENSE_PROGRAM_H
#define TESTS_SUPPORT_DENSE_PROGRAM_H
#include "tests/test.h"
#include "tests/support/dense_reference.h"
#include <yvex/internal/component.h>
#include <yvex/internal/dense_program.h>
#include <yvex/internal/program_stage.h>
#include <yvex/qtype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
    yvex_component_encoded_weight weights[28];
    unsigned int calls, cancel_at;
    unsigned int reads, fail_read;
    unsigned long long largest_read;
} dense_test_binding;

static int dense_test_read(void *context, unsigned long long id, unsigned long long offset,
    void *output, size_t bytes, yvex_error *err)
{
    dense_test_binding *b = context;
    b->reads++;
    if (id >= 28u || offset > b->weights[id].encoded_bytes || bytes > b->weights[id].encoded_bytes - offset) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "test.parameter-source", "read exceeds exact source parameter");
        return YVEX_ERR_BOUNDS;
    }
    if (b->fail_read && b->reads == b->fail_read) {
        yvex_error_set(err, YVEX_ERR_IO, "test.parameter-source", "injected source refusal");
        return YVEX_ERR_IO;
    }
    if (bytes > b->largest_read) b->largest_read = bytes;
    memcpy(output, b->weights[id].encoded + offset, bytes);
    return YVEX_OK;
}

static int dense_test_stream(yvex_backend *backend, const yvex_program_physical *program,
    dense_test_binding *binding, const float *const inputs[3], const float expected[12])
{
    yvex_program_kernel_parameter parameters[28];
    yvex_program_stage *stage = NULL;
    yvex_error err;
    yvex_backend_operation_facts facts;
    float output[12];
    float *outputs[] = {output};
    for (size_t i = 0u; i < 28u; ++i) {
        parameters[i] = (yvex_program_kernel_parameter){.tensor_id = i, .weight = binding->weights[i],
            .read = dense_test_read, .read_context = binding};
        parameters[i].weight.encoded = NULL;
    }
    YVEX_TEST_ASSERT(yvex_program_stage_open(&stage, program, parameters, 28u, backend,
        4u, 1, 1024u * 1024u, 1024u * 1024u, &err) == YVEX_OK, "bounded CPU parameter source prepares");
    for (unsigned int run = 0u; run < 3u; ++run) {
        for (size_t i = 0u; i < 12u; ++i) output[i] = -77.0f;
        binding->fail_read = run == 1u ? binding->reads + 5u : 0u;
        int rc = yvex_program_stage_host(stage, 4u, inputs, 3u, outputs, 1u, NULL, NULL, &facts, &err);
        YVEX_TEST_ASSERT(rc == (run == 1u ? YVEX_ERR_IO : YVEX_OK), "source failure is propagated, then retry is exact");
        for (size_t i = 0u; i < 12u; ++i)
            YVEX_TEST_ASSERT(output[i] == (run == 1u ? -77.0f : expected[i]), "no partial source result escapes");
    }
    YVEX_TEST_ASSERT(binding->largest_read == 1024u && yvex_program_stage_close(&stage, &err) == YVEX_OK,
        "one bounded source parameter at a time, no complete weight mirror");
    printf("Dense CPU parameter source: reads=%u largest_read=%llu bytes; resident==streamed max_abs=0 tolerance=0; "
        "mid-execution IO refusal unpublished, retry exact\n", binding->reads, binding->largest_read);
    return 0;
}
static int dense_test_prefix(void)
{
    const char *identity = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    yvex_dense_prefix_recipe recipe = {.semantic_identity = identity, .rows = 2u, .input_width = 2u,
        .intermediate_width = 2u, .output_width = 3u, .learned_rows = 2u, .padding_rows = 1u};
    float w0[] = {1, 0, 0, 1}, b0[] = {0.25f, -0.5f};
    float w1[] = {1, 0, 0, 1, 1, -1}, b1[] = {0, 0, 0.5f}, learned[] = {1, -0.0f, -1, 2, 3, 4};
    const float *weights[] = {w0, b0, w1, b1, learned};
    const unsigned long long widths[] = {2, 2, 2, 3, 3}, rows[] = {2, 1, 3, 1, 2};
    float input[] = {1, 2, 3, 4}, output[15];
    const float expected[] = {1.25f, 1.5f, 0.25f, 3.25f, 3.5f, 0.25f, 1, -0.0f, -1, 2, 3, 4, 0, 0, 0};
    dense_test_binding source = {0};
    yvex_program_kernel_parameter parameters[5];
    yvex_program_physical *program = NULL, *decoded = NULL;
    yvex_program_stage *stage = NULL;
    yvex_backend *backend = NULL;
    yvex_backend_operation_facts facts;
    yvex_core_bytes wire = {.maximum = 1024u * 1024u};
    yvex_error err;
    int rc = yvex_dense_prefix_compile(&program, &recipe, &err);
    if (rc == YVEX_OK) rc = yvex_program_physical_encode(program, &wire, &err);
    if (rc == YVEX_OK) rc = yvex_program_physical_decode(&decoded, wire.data, wire.count, &err);
    if (rc != YVEX_OK) fprintf(stderr, "prefix lowering: %s: %s\n", yvex_error_where(&err), yvex_error_message(&err));
    YVEX_TEST_ASSERT(rc == YVEX_OK && !strcmp(yvex_program_physical_summary_get(program)->identity,
        yvex_program_physical_summary_get(decoded)->identity), "typed prefix survives deterministic physical import");
    for (size_t i = 0u; i < 5u; ++i) {
        source.weights[i] = (yvex_component_encoded_weight){.encoded = (const unsigned char *)weights[i],
            .encoded_bytes = rows[i] * widths[i] * sizeof(float), .row_count = rows[i], .row_width = widths[i],
            .row_bytes = widths[i] * sizeof(float), .qtype = YVEX_GGUF_QTYPE_F32};
        parameters[i] = (yvex_program_kernel_parameter){.tensor_id = i, .weight = source.weights[i],
            .read = dense_test_read, .read_context = &source};
        parameters[i].weight.encoded = NULL;
    }
    rc = yvex_backend_open_cpu(&backend, &err);
    if (rc == YVEX_OK) rc = yvex_program_stage_open(&stage, decoded, parameters, 5u, backend,
        2u, 1, 1024u * 1024u, 1024u * 1024u, &err);
    if (rc != YVEX_OK) fprintf(stderr, "prefix prepare: %s: %s\n", yvex_error_where(&err), yvex_error_message(&err));
    YVEX_TEST_ASSERT(rc == YVEX_OK, "prefix binds exact source parameters with explicit independent populations");
    const float *inputs[] = {input}; float *outputs[] = {output};
    for (size_t run = 0u; run < 3u; ++run) {
        for (size_t i = 0u; i < 15u; ++i) output[i] = -77.0f;
        source.fail_read = run == 1u ? source.reads + 3u : 0u;
        rc = yvex_program_stage_host(stage, 2u, inputs, 1u, outputs, 1u, NULL, NULL, &facts, &err);
        if (rc != (run == 1u ? YVEX_ERR_IO : YVEX_OK))
            fprintf(stderr, "prefix execution: %s: %s\n", yvex_error_where(&err), yvex_error_message(&err));
        YVEX_TEST_ASSERT(rc == (run == 1u ? YVEX_ERR_IO : YVEX_OK), "prefix source failure is transactional");
        for (size_t i = 0u; i < 15u; ++i)
            YVEX_TEST_ASSERT(output[i] == (run == 1u ? -77.0f : expected[i]), "independent affine/learned/zero row oracle");
        if (run != 1u) YVEX_TEST_ASSERT(!memcmp(output, expected, sizeof(output)), "learned negative zero is preserved");
    }
    YVEX_TEST_ASSERT(yvex_program_stage_close(&stage, &err) == YVEX_OK &&
        yvex_backend_close_checked(&backend, &err) == YVEX_OK, "prefix cleanup");
    printf("Dense prefix CPU: five source parameters, rows 2->4->5, 15 values exact including signed zero; "
        "mid-source refusal unpublished, repeat exact; binary identity exact\n");
    free(wire.data);
    yvex_program_physical_close(&decoded); yvex_program_physical_close(&program);
    return 0;
}

static int dense_test_cancel(void *context)
{
    dense_test_binding *b = context;
    b->calls++;
    return b->cancel_at && b->calls >= b->cancel_at;
}
static int dense_test_name(void *context, unsigned long long ordinal, char name[256], yvex_error *err)
{
    (void)context; (void)err;
    if (ordinal >= 28u) return YVEX_ERR_BOUNDS;
    snprintf(name, 256u, "weight_%llu", ordinal);
    return YVEX_OK;
}
static int dense_test_weight(void *context, const char *name, yvex_component_encoded_weight *out, yvex_error *err)
{
    dense_test_binding *b = context;
    unsigned long long ordinal;
    char tail;
    (void)err;
    if (sscanf(name, "weight_%llu%c", &ordinal, &tail) != 1 || ordinal >= 28u) return YVEX_ERR_FORMAT;
    *out = b->weights[ordinal];
    return YVEX_OK;
}
static int dense_test_workspace(void *context, unsigned long long bytes, yvex_error *err)
{
    (void)context; (void)bytes; (void)err;
    return YVEX_OK;
}
static int test_dense_program(yvex_backend_kind kind)
{
    if (kind == YVEX_BACKEND_KIND_CPU && dense_test_prefix()) return 1;
    /* Recorded before retirement of CUDA dense_decoder_run at ac8a34a9:
     * two non-zero blocks, two heads, partial rotary and a three-row prefix.
     * Cross-implementation preservation is not upstream conformance. */
    const float retained[] = {0x1.1bc16cp-3f, -0x1.a8b218p-2f, 0x1.43c5ap-3f, -0x1.5dde32p-1f,
        -0x1.52a308p-3f, 0x1.1cfab4p-2f, 0x1.2b2948p-1f, -0x1.815a04p-2f,
        0x1.28583ep-3f, -0x1.ada55p-2f, 0x1.52617cp-3f, -0x1.59685ep-1f};
    const char *identity = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    dense_test_binding binding = {0};
    yvex_backend *backend = NULL;
    yvex_device_tensor *resident = NULL;
    yvex_program_stage *stage = NULL;
    yvex_program_physical *program = NULL, *imported = NULL;
    unsigned char *arena = NULL;
    yvex_error err;
    yvex_backend_options options = {.kind = kind};
    yvex_backend_tensor_desc desc = {.name = "dense-program-weights", .dtype = YVEX_DTYPE_I8, .rank = 1u};
    yvex_dense_program_recipe recipe = {.semantic_identity = identity, .rows = 4u, .output_rows = 3u,
        .width = 8u, .heads = 2u, .head_dimension = 4u, .rotary_dimension = 2u,
        .ffn_width = 16u, .block_count = 2u, .output_width = 4u, .epsilon = 1.0e-5};
    float hidden[32], cosine[8], sine[8], output[12], reference[12];
    const float *inputs[] = {hidden, cosine, sine};
    float *outputs[] = {output};
    unsigned long long input_capacity[] = {32u, 8u, 8u}, output_capacity = 12u;
    yvex_component_execution component = {.schema_version = YVEX_COMPONENT_EXECUTION_SCHEMA_V2,
        .program_stage = &stage, .owner_context = &binding, .weight_view = dense_test_weight,
        .workspace_reserve = dense_test_workspace};
    yvex_component_program_request request = {.parameter_name = dense_test_name, .parameter_context = &binding,
        .inputs = inputs, .input_capacity = input_capacity, .input_count = 3u,
        .outputs = outputs, .output_capacity = &output_capacity, .output_count = 1u, .rows = 4u,
        .cancel_requested = dense_test_cancel, .cancel_context = &binding};
    yvex_component_program_result result;
    yvex_backend_memory_stats before, after;
    for (size_t i = 0u; i < 28u; ++i) {
        unsigned long long rows = 1u, width = 8u;
        if (i < 24u) {
            size_t slot = i % 12u;
            if (slot == 1u) rows = 24u;
            if (slot == 2u) width = 24u;
            if (slot == 3u) rows = 8u;
            if (slot == 7u) rows = 32u;
            if (slot == 8u) width = 32u;
            if (slot == 9u) { rows = 8u; width = 16u; }
        } else if (i == 26u) rows = 4u;
        else if (i == 27u) width = 4u;
        binding.weights[i] = (yvex_component_encoded_weight){.qtype = YVEX_GGUF_QTYPE_F32,
            .row_count = rows, .row_width = width, .row_bytes = width * 4u, .encoded_bytes = rows * width * 4u};
        desc.bytes += binding.weights[i].encoded_bytes;
    }
    desc.dims[0] = desc.bytes;
    YVEX_TEST_ASSERT(yvex_backend_open(&backend, &options, &err) == YVEX_OK, "dense fixture backend");
    if (kind == YVEX_BACKEND_KIND_CPU) {
        YVEX_TEST_ASSERT(yvex_backend_tensor_alloc(backend, &desc, &resident, &err) == YVEX_OK, "CPU source weights");
        arena = resident->data;
    } else YVEX_TEST_ASSERT(yvex_backend_resident_alloc(backend, &desc, &resident, &arena, &err) == YVEX_OK,
        "CUDA source weights residency");
    component.backend = backend;
    component.materialization = (yvex_materialization_session *)&component;
    snprintf(component.residency_identity, sizeof(component.residency_identity), "%s", identity);
    unsigned long long offset = 0u;
    for (size_t i = 0u; i < 28u; ++i) {
        binding.weights[i].encoded = arena + offset;
        for (size_t j = 0u; j < binding.weights[i].encoded_bytes / 4u; ++j) {
            float value = (float)((int)((j * 3u + i) % 17u) - 8) / 32.0f;
            if (i < 24u && (i % 12u == 0u || i % 12u == 6u)) value += 1.0f;
            if (i == 24u) value += 1.0f;
            memcpy(arena + offset + j * 4u, &value, 4u);
        }
        offset += binding.weights[i].encoded_bytes;
    }
    for (size_t i = 0u; i < 32u; ++i) hidden[i] = (float)((int)(i % 13u) - 6) / 8.0f;
    for (size_t i = 0u; i < 8u; ++i) {
        float angle = (float)(i / 2u) * 0.25f;
        cosine[i] = cosf(angle); sine[i] = sinf(angle);
    }
    if (kind == YVEX_BACKEND_KIND_CPU) {
        const float *weights[28];
        for (size_t i = 0u; i < 28u; ++i) weights[i] = (const float *)binding.weights[i].encoded;
        YVEX_TEST_ASSERT(dense_reference(weights, hidden, cosine, sine, recipe.epsilon, reference, &err) == YVEX_OK,
            "retained CPU source-order composition computes preservation reference");
    } else memcpy(reference, retained, sizeof(reference));
    YVEX_TEST_ASSERT((kind == YVEX_BACKEND_KIND_CPU ||
        yvex_backend_resident_attach(backend, arena, desc.bytes, resident, 1u, &err) == YVEX_OK) &&
        yvex_backend_get_memory_stats(backend, &before, &err) == YVEX_OK &&
        yvex_dense_program_compile(&program, &recipe, &err) == YVEX_OK, "compile source dense program");
    request.program = program;
    yvex_core_bytes wire = {.maximum = 16u * 1024u * 1024u};
    int imported_rc = yvex_program_physical_encode(program, &wire, &err);
    if (imported_rc == YVEX_OK) imported_rc = yvex_program_physical_decode(&imported, wire.data, wire.count, &err);
    if (imported_rc != YVEX_OK) fprintf(stderr, "dense import: %s: %s\n",
        yvex_error_where(&err), yvex_error_message(&err));
    YVEX_TEST_ASSERT(imported_rc == YVEX_OK &&
        !strcmp(yvex_program_physical_summary_get(program)->identity,
            yvex_program_physical_summary_get(imported)->identity), "F32 lowering reimports with exact identity");
    free(wire.data);
    request.program = imported;
    int rc = yvex_component_tensor_program_execute(&component, &request, &result, &err);
    if (rc != YVEX_OK) fprintf(stderr, "dense SSA: %s: %s\n", yvex_error_where(&err), yvex_error_message(&err));
    YVEX_TEST_ASSERT(rc == YVEX_OK && result.complete && !stage, "real component binding executes SSA and releases");
    unsigned int completed_calls = binding.calls;
    for (size_t i = 0u; i < 12u; ++i)
        YVEX_TEST_ASSERT(output[i] == reference[i], "all nonzero dense outputs preserve same-backend computation exactly");
    printf("Dense SSA %s: two blocks, four input/three output rows, 12 values; first=%.9g last=%.9g max_abs=0 tolerance=0\n",
        yvex_backend_kind_name(kind), (double)output[0], (double)output[11]);
    if (kind == YVEX_BACKEND_KIND_CPU && dense_test_stream(backend, imported, &binding, inputs, reference)) return 1;
    for (unsigned int negative = 0u; negative < 6u; ++negative) {
        for (size_t i = 0u; i < 12u; ++i) output[i] = -77.0f;
        binding.calls = 0u; binding.cancel_at = negative == 4u ? 1u : negative == 5u ? completed_calls : 0u;
        input_capacity[0] = negative == 0u ? 31u : 32u;
        binding.weights[26].row_width = negative == 1u ? 9u : 8u;
        request.device_limit = negative == 2u ? 1u : 0u;
        request.host_limit = negative == 3u ? 1u : 0u;
        rc = yvex_component_tensor_program_execute(&component, &request, &result, &err);
        YVEX_TEST_ASSERT(rc == (negative < 2u ? YVEX_ERR_FORMAT : negative < 4u ? YVEX_ERR_BOUNDS : YVEX_ERR_CANCELLED) &&
            !result.complete && !stage, "capacity/binding/resources/early and final cancellation fail closed");
        for (size_t i = 0u; i < 12u; ++i) YVEX_TEST_ASSERT(output[i] == -77.0f, "failed component publishes no values");
        YVEX_TEST_ASSERT(yvex_backend_get_memory_stats(backend, &after, &err) == YVEX_OK &&
            after.allocated_bytes == before.allocated_bytes, "failed component discharges all temporary resources");
    }
    binding.calls = binding.cancel_at = 0u;
    binding.weights[26].row_width = 8u;
    input_capacity[0] = 32u; request.host_limit = request.device_limit = 0u;
    /* Zero both updates, retaining the independent LayerNorm/projection case
     * formerly checked by cuda.quant's procedural decoder test. */
    for (size_t i = 0u; i < 24u; ++i) if (i % 12u == 5u || i % 12u == 11u)
        memset((void *)binding.weights[i].encoded, 0, (size_t)binding.weights[i].encoded_bytes);
    YVEX_TEST_ASSERT(yvex_component_tensor_program_execute(&component, &request, &result, &err) == YVEX_OK,
        "zero-update independent reference executes through the same program");
    double maximum = 0.0;
    for (size_t row = 0u; row < 3u; ++row) {
        double mean = 0.0, variance = 0.0, normalized[8];
        for (size_t j = 0u; j < 8u; ++j) mean += hidden[row * 8u + j] / 8.0;
        for (size_t j = 0u; j < 8u; ++j) variance += (hidden[row * 8u + j] - mean) * (hidden[row * 8u + j] - mean) / 8.0;
        for (size_t j = 0u; j < 8u; ++j) normalized[j] = (hidden[row * 8u + j] - mean) /
            sqrt(variance + recipe.epsilon) * ((const float *)binding.weights[24].encoded)[j] +
            ((const float *)binding.weights[25].encoded)[j];
        for (size_t column = 0u; column < 4u; ++column) {
            double expected = ((const float *)binding.weights[27].encoded)[column];
            for (size_t j = 0u; j < 8u; ++j) expected += normalized[j] * ((const float *)binding.weights[26].encoded)[column * 8u + j];
            double error = fabs(expected - output[row * 4u + column]);
            if (error > maximum) maximum = error;
            YVEX_TEST_ASSERT(error < 1.0e-5, "independent F64 LayerNorm/projection reference retains original tolerance");
        }
    }
    printf("Dense independent final norm: 12 values max_abs=%.12g tolerance=1e-5; lifecycle negatives=6 unpublished allocation_delta=0\n", maximum);
    yvex_program_physical_close(&imported);
    yvex_program_physical_close(&program);
    YVEX_TEST_ASSERT(yvex_backend_get_memory_stats(backend, &after, &err) == YVEX_OK &&
        after.allocated_bytes == before.allocated_bytes && yvex_backend_resident_detach(backend, &err) == YVEX_OK &&
        yvex_backend_tensor_release(backend, &resident, &err) == YVEX_OK &&
        yvex_backend_close_checked(&backend, &err) == YVEX_OK, "dense component leaves no temporary owners");
    return 0;
}
#endif
