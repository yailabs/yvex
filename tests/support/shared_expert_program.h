/* Shared FFN program: independent diagonal oracle plus retained row-dot
 * preservation. Neither is upstream whole-model conformance. */
#ifndef TESTS_SUPPORT_SHARED_EXPERT_PROGRAM_H
#define TESTS_SUPPORT_SHARED_EXPERT_PROGRAM_H
#include "tests/test.h"
#include <yvex/internal/moe.h>
#include <yvex/internal/program_stage.h>
#include <yvex/internal/quant_numeric.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static float test_shared_round(float x)
{
    uint32_t bits;
    memcpy(&bits, &x, sizeof(bits));
    bits += 0x7fffu + ((bits >> 16u) & 1u);
    bits &= 0xffff0000u;
    memcpy(&x, &bits, sizeof(x));
    return x;
}

static int test_shared_cancel(void *context) { (void)context; return 1; }

static int test_shared_expert(yvex_backend_kind kind)
{
    enum { WIDTH = 32, ROWS = 3, COUNT = WIDTH * ROWS };
    const char *identity = "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789";
    float weights[3][WIDTH * WIDTH] = {{0}}, x[COUNT], actual[COUNT], expected[COUNT], single[COUNT];
    const float probes[] = {-80.0f, -8.0f, -7.0f, -1.0f, -0.0f, 0.0f, 0.125f, 1.0f, 7.0f, 8.0f, 80.0f};
    yvex_moe_layer_plan layer = {.hidden_width = WIDTH, .shared_intermediate_width = WIDTH,
        .shared_experts = 1u, .activation_limit = 7.0};
    yvex_program_kernel_parameter parameters[3] = {0};
    for (unsigned int w = 0u; w < 3u; ++w) {
        for (unsigned int i = 0u; i < WIDTH; ++i) weights[w][i * WIDTH + i] = w == 1u ? -0.5f : 1.0f;
        layer.tensor_ids[YVEX_MOE_WEIGHT_SHARED_GATE + w] = 401u + w;
        layer.qtypes[YVEX_MOE_WEIGHT_SHARED_GATE + w] = YVEX_GGUF_QTYPE_F32;
        parameters[w] = (yvex_program_kernel_parameter){.tensor_id = 401u + w,
            .weight = {(const unsigned char *)weights[w], sizeof(weights[w]), WIDTH, WIDTH,
                WIDTH * sizeof(float), YVEX_GGUF_QTYPE_F32}};
    }
    for (unsigned int i = 0u; i < COUNT; ++i) {
        x[i] = probes[i % (sizeof(probes) / sizeof(probes[0]))];
        long double g = fminl(x[i], 7.0L), u = fmaxl(-7.0L, fminl(-0.5L * x[i], 7.0L));
        expected[i] = test_shared_round((float)(g / (1.0L + expl(-g)) * u));
    }
    yvex_error err = {0};
    yvex_program_physical *program = NULL, *copy = NULL, *bad = NULL, *changed = NULL;
    yvex_program_stage *stage = NULL, *refused = NULL;
    yvex_core_bytes bytes = {.maximum = 65536u}, again = {.maximum = 65536u};
    int imported = yvex_moe_shared_program_import(&program, &layer, identity, identity, ROWS, &err);
    if (imported != YVEX_OK) fprintf(stderr, "shared import: %s: %s\n", yvex_error_where(&err), yvex_error_message(&err));
    YVEX_TEST_ASSERT(imported == YVEX_OK,
        "shared FFN imports as a typed function");
    const yvex_program_physical_summary *s = yvex_program_physical_summary_get(program);
    YVEX_TEST_ASSERT(s->input_count == 1u && s->result_count == 1u, "one normalized input and BF16 result");
    for (unsigned int invalid = 0u; invalid < 5u; ++invalid) {
        yvex_moe_layer_plan l = layer;
        if (invalid == 0u) l.activation_limit = 0.0;
        if (invalid == 1u) l.activation_limit = NAN;
        if (invalid == 2u) l.shared_experts = 2u;
        if (invalid == 3u) l.hidden_width = 0u;
        if (invalid == 4u) l.qtypes[YVEX_MOE_WEIGHT_SHARED_GATE] = UINT32_MAX;
        YVEX_TEST_ASSERT(yvex_moe_shared_program_import(&bad, &l, identity, identity, ROWS, &err) != YVEX_OK && !bad,
            "invalid limit, topology, shape or representation refuses at compilation");
    }
    layer.activation_limit = 8.0;
    YVEX_TEST_ASSERT(yvex_moe_shared_program_import(&changed, &layer, identity, identity, ROWS, &err) == YVEX_OK &&
        strcmp(s->identity, yvex_program_physical_summary_get(changed)->identity), "clamp semantics change identity");
    layer.activation_limit = 7.0;
    YVEX_TEST_ASSERT(yvex_program_physical_encode(program, &bytes, &err) == YVEX_OK &&
        yvex_program_physical_decode(&copy, bytes.data, bytes.count, &err) == YVEX_OK &&
        yvex_program_physical_encode(copy, &again, &err) == YVEX_OK && bytes.count == again.count &&
        !memcmp(bytes.data, again.data, bytes.count), "shared program reopens deterministically");
    yvex_backend *backend = NULL;
    yvex_backend_options options = {.kind = kind};
    yvex_backend_memory_stats before, after;
    yvex_backend_operation_facts facts;
    YVEX_TEST_ASSERT(yvex_backend_open(&backend, &options, &err) == YVEX_OK &&
        yvex_backend_get_memory_stats(backend, &before, &err) == YVEX_OK,
        "shared program backend opens");
    yvex_device_tensor *resident = NULL;
    if (kind == YVEX_BACKEND_KIND_CUDA) {
        unsigned char *mapped = NULL;
        yvex_backend_tensor_desc d = {.name = "shared-parameters", .dtype = YVEX_DTYPE_I8,
            .rank = 1u, .dims = {sizeof(weights)}, .bytes = sizeof(weights)};
        YVEX_TEST_ASSERT(yvex_backend_resident_alloc(backend, &d, &resident, &mapped, &err) == YVEX_OK &&
            yvex_backend_resident_attach(backend, mapped, d.bytes, resident, 1u, &err) == YVEX_OK,
            "shared physical parameters have exact device residency");
        memcpy(mapped, weights, sizeof(weights));
        for (unsigned int i = 0u; i < 3u; ++i) parameters[i].weight.encoded = mapped + i * sizeof(weights[0]);
    }
    YVEX_TEST_ASSERT(yvex_program_stage_open(&refused, copy, parameters, 2u, backend, ROWS, 1, 0u, 0u, &err) !=
        YVEX_OK && !refused, "missing shared parameter refuses before invocation");
    YVEX_TEST_ASSERT(yvex_program_stage_open(&stage, copy, parameters, 3u, backend, ROWS, 1, 0u, 0u, &err) ==
        YVEX_OK, "shared parameters bind once");
    int rc = yvex_program_stage_host(stage, ROWS, (const float *[]){x}, 1u,
        (float *[]){actual}, 1u, NULL, NULL, &facts, &err);
    if (rc != YVEX_OK) fprintf(stderr, "shared execution: %s: %s\n", yvex_error_where(&err), yvex_error_message(&err));
    YVEX_TEST_ASSERT(rc == YVEX_OK, "shared compiled population executes");
    double maximum = 0.0;
    for (unsigned int i = 0u; i < COUNT; ++i) {
        double error = fabs((double)actual[i] - expected[i]);
        if (error > maximum) maximum = error;
    }
    YVEX_TEST_ASSERT(maximum == 0.0, "shared compiled FFN equals independent long-double diagonal oracle exactly");
    for (unsigned int row = 0u; row < ROWS; ++row)
        YVEX_TEST_ASSERT(yvex_program_stage_host(stage, 1u, (const float *[]){x + row * WIDTH}, 1u,
            (float *[]){single + row * WIDTH}, 1u, NULL, NULL, &facts, &err) == YVEX_OK,
            "shared singleton invocation executes");
    YVEX_TEST_ASSERT(!memcmp(actual, single, sizeof(actual)), "shared population equals ordered singleton calls");
    for (unsigned int i = 0u; i < COUNT; ++i) actual[i] = 123.0f;
    YVEX_TEST_ASSERT(yvex_program_stage_host(stage, ROWS, (const float *[]){x}, 1u, (float *[]){actual}, 1u,
        test_shared_cancel, NULL, &facts, &err) == YVEX_ERR_CANCELLED, "shared cancellation fails closed");
    for (unsigned int i = 0u; i < COUNT; ++i)
        YVEX_TEST_ASSERT(actual[i] == 123.0f, "shared cancellation publishes no partial result");
    YVEX_TEST_ASSERT(yvex_program_stage_close(&stage, &err) == YVEX_OK, "shared stage closes");
    if (resident) YVEX_TEST_ASSERT(yvex_backend_resident_detach(backend, &err) == YVEX_OK &&
        yvex_backend_tensor_release(backend, &resident, &err) == YVEX_OK, "shared residency closes");
    YVEX_TEST_ASSERT(yvex_backend_get_memory_stats(backend, &after, &err) == YVEX_OK &&
        before.allocated_bytes == after.allocated_bytes && yvex_backend_close_checked(&backend, &err) == YVEX_OK,
        "shared resources close without leaked allocation");
    printf("shared FFN %s: values=%u long-double-diagonal max_abs=%.12g tolerance=0 "
        "chunk=single negatives=5 missing_parameter=refused cancel=unpublished allocation_delta=0\n",
        kind == YVEX_BACKEND_KIND_CPU ? "CPU" : "CUDA", COUNT, maximum);
    yvex_program_physical_close(&program); yvex_program_physical_close(&copy);
    yvex_program_physical_close(&changed); free(bytes.data); free(again.data);
    return 0;
}
static void test_shared_q8_reference(const float *x, float *out)
{
    unsigned int maximum = 0u;
    for (unsigned int i = 1u; i < 256u; ++i)
        if (fabsf(x[i]) > fabsf(x[maximum])) maximum = i;
    float inverse = x[maximum] == 0.0f ? 0.0f : -127.0f / x[maximum];
    float scale = inverse == 0.0f ? 0.0f : 1.0f / inverse;
    for (unsigned int i = 0u; i < 256u; ++i) {
        float q = nearbyintf(x[i] * inverse);
        out[i] = fmaxf(-128.0f, fminf(127.0f, q)) * scale;
    }
}

static int test_shared_target(yvex_backend_kind kind)
{
    enum { WIDTH = 256, ROWS = 3, COUNT = WIDTH * ROWS, ROW_BYTES = 8 * 34,
        WEIGHT_BYTES = WIDTH * ROW_BYTES, BYTES = 3 * WEIGHT_BYTES };
    const char *identity = "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789";
    yvex_moe_layer_plan layer = {.hidden_width = WIDTH, .shared_intermediate_width = WIDTH,
        .shared_experts = 1u, .activation_limit = 7.0};
    yvex_program_physical *source = NULL, *target = NULL, *bad = NULL;
    yvex_program_stage *stage = NULL;
    yvex_error err = {0};
    for (unsigned int i = 0u; i < 3u; ++i) {
        layer.tensor_ids[YVEX_MOE_WEIGHT_SHARED_GATE + i] = 901u + i;
        layer.qtypes[YVEX_MOE_WEIGHT_SHARED_GATE + i] = YVEX_GGUF_QTYPE_Q8_0;
    }
    YVEX_TEST_ASSERT(yvex_moe_shared_program_import(&source, &layer, identity, identity, ROWS, &err) == YVEX_OK,
        "encoded shared program imports without inferring activation precision");
    yvex_program_target_choice choices[3];
    size_t count = 0u;
    const yvex_program_physical_summary *s = yvex_program_physical_summary_get(source);
    for (size_t i = 0u; i < s->step_count; ++i)
        if (!strcmp(yvex_program_physical_step_at(source, i)->implementation, "linear.row_dot.f32.v1"))
            choices[count++] = (yvex_program_target_choice){i, "linear.row_dot.q8.v1"};
    YVEX_TEST_ASSERT(count == 3u && yvex_program_physical_target_compile(&target, source, choices, count, &err) ==
        YVEX_OK, "all three linear operations accept explicit Q8 target choices");
    const yvex_program_physical_summary *t = yvex_program_physical_summary_get(target);
    YVEX_TEST_ASSERT(!strcmp(s->semantic_identity, t->semantic_identity) &&
        !strcmp(s->execution_identity, t->execution_identity) && strcmp(s->identity, t->identity),
        "target activation changes only physical identity");
    yvex_program_target_choice duplicate[] = {choices[0], choices[0]};
    YVEX_TEST_ASSERT(yvex_program_physical_target_compile(&bad, source, duplicate, 2u, &err) != YVEX_OK && !bad,
        "duplicate target choice refuses");
    yvex_backend *backend = NULL;
    yvex_backend_options options = {.kind = kind};
    YVEX_TEST_ASSERT(yvex_backend_open(&backend, &options, &err) == YVEX_OK, "target backend opens");
    if (kind == YVEX_BACKEND_KIND_CPU) {
        YVEX_TEST_ASSERT(yvex_program_stage_open(&stage, target, NULL, 0u, backend, ROWS, 1, 0u, 0u, &err) !=
            YVEX_OK && !stage, "CUDA Q8 target cannot silently execute as CPU F32");
    } else {
        unsigned char *mapped = NULL;
        yvex_device_tensor *resident = NULL;
        yvex_backend_tensor_desc d = {.name = "shared-q8-parameters", .dtype = YVEX_DTYPE_I8,
            .rank = 1u, .dims = {BYTES}, .bytes = BYTES};
        YVEX_TEST_ASSERT(yvex_backend_resident_alloc(backend, &d, &resident, &mapped, &err) == YVEX_OK &&
            yvex_backend_resident_attach(backend, mapped, BYTES, resident, 1u, &err) == YVEX_OK,
            "Q8 target parameters have exact residency");
        memset(mapped, 0, BYTES);
        yvex_program_kernel_parameter params[3];
        for (unsigned int w = 0u; w < 3u; ++w) {
            for (unsigned int row = 0u; row < WIDTH; ++row) {
                unsigned char *block = mapped + w * WEIGHT_BYTES + row * ROW_BYTES + (row / 32u) * 34u;
                block[0] = 0u; block[1] = 0x3cu; /* binary16 scale = 1 */
                block[2u + row % 32u] = w == 1u ? 255u : 1u;
            }
            params[w] = (yvex_program_kernel_parameter){.tensor_id = 901u + w,
                .weight = {mapped + w * WEIGHT_BYTES, WEIGHT_BYTES, WIDTH, WIDTH, ROW_BYTES,
                    YVEX_GGUF_QTYPE_Q8_0}};
        }
        float x[COUNT], quantized[WIDTH], hidden[WIDTH], expected[COUNT], actual[COUNT], single[COUNT];
        for (unsigned int r = 0u; r < ROWS; ++r) {
            for (unsigned int i = 0u; i < WIDTH; ++i)
                x[r * WIDTH + i] = test_shared_round(((float)(i % 47u) - 23.0f) * (0.3125f + r * 0.125f));
            test_shared_q8_reference(x + r * WIDTH, quantized);
            for (unsigned int i = 0u; i < WIDTH; ++i) {
                long double g = fminl(quantized[i], 7.0L), u = fmaxl(-7.0L, fminl(-quantized[i], 7.0L));
                hidden[i] = test_shared_round((float)(g / (1.0L + expl(-g)) * u));
            }
            test_shared_q8_reference(hidden, quantized);
            for (unsigned int i = 0u; i < WIDTH; ++i) expected[r * WIDTH + i] = test_shared_round(quantized[i]);
        }
        YVEX_TEST_ASSERT(yvex_program_stage_open(&stage, target, params, 3u, backend, ROWS, 1, 0u, 0u, &err) ==
            YVEX_OK, "explicit target program binds");
        yvex_backend_operation_facts facts;
        YVEX_TEST_ASSERT(yvex_program_stage_host(stage, ROWS, (const float *[]){x}, 1u,
            (float *[]){actual}, 1u, NULL, NULL, &facts, &err) == YVEX_OK,
            "Q8 target executes full batch");
        for (unsigned int r = 0u; r < ROWS; ++r)
            YVEX_TEST_ASSERT(yvex_program_stage_host(stage, 1u, (const float *[]){x + r * WIDTH}, 1u,
                (float *[]){single + r * WIDTH}, 1u, NULL, NULL, &facts, &err) == YVEX_OK,
                "one-row batch retains the same target precision");
        float maximum = 0.0f;
        for (unsigned int i = 0u; i < COUNT; ++i) maximum = fmaxf(maximum, fabsf(expected[i] - actual[i]));
        printf("shared Q8 target: values=%u independent-codec max_abs=%.12g tolerance=0 chunk=single=%s\n",
            COUNT, maximum, !memcmp(actual, single, sizeof(actual)) ? "true" : "false");
        YVEX_TEST_ASSERT(maximum == 0.0f && !memcmp(actual, single, sizeof(actual)),
            "target precision matches independent codec and preserves chunk equivalence");
        YVEX_TEST_ASSERT(yvex_program_stage_close(&stage, &err) == YVEX_OK &&
            yvex_backend_resident_detach(backend, &err) == YVEX_OK &&
            yvex_backend_tensor_release(backend, &resident, &err) == YVEX_OK, "Q8 target resources close");
    }
    YVEX_TEST_ASSERT(yvex_backend_close_checked(&backend, &err) == YVEX_OK, "target backend closes");
    yvex_program_physical_close(&target); yvex_program_physical_close(&source);
    return 0;
}
#endif
