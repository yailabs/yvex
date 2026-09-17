/* Test-owned replay of the retired ingress composition using retained kernels.
 * This is migration preservation, not an upstream numerical oracle. */
#ifndef TESTS_SUPPORT_MHC_CUDA_CONTROL_H
#define TESTS_SUPPORT_MHC_CUDA_CONTROL_H
#include "src/backend/cuda/private.h"
#include <yvex/internal/program_stage.h>
#include <yvex/internal/moe.h>
#include <yvex/internal/quant_numeric.h>
#include "tests/test.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static int test_mhc_cuda_control(unsigned int router_qtype)
{
    enum { WIDTH = 4096, STREAMS = 4, EXPANDED = WIDTH * STREAMS,
        MIXES = (STREAMS + 2) * STREAMS, ROWS = 3, ROUTES = 4,
        RESULT = WIDTH + STREAMS + STREAMS * STREAMS + ROUTES };
    const char *identity = "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789";
    const unsigned int slots[] = {YVEX_MOE_WEIGHT_MHC_FUNCTION, YVEX_MOE_WEIGHT_MHC_SCALE,
        YVEX_MOE_WEIGHT_MHC_BASE, YVEX_MOE_WEIGHT_FFN_NORM, YVEX_MOE_WEIGHT_ROUTER};
    const unsigned long long widths[] = {EXPANDED, 3u, MIXES, WIDTH, WIDTH}, counts[] = {MIXES, 1u, 1u, 1u, ROUTES};
    yvex_backend *backend = NULL;
    yvex_backend_options options = {.kind = YVEX_BACKEND_KIND_CUDA};
    yvex_device_tensor *resident = NULL;
    yvex_program_physical *program = NULL;
    yvex_program_stage *stage = NULL;
    yvex_program_kernel_parameter parameters[5] = {0};
    yvex_backend_attention_weight weights[5] = {0};
    CUdeviceptr addresses[5] = {0}, x, mixes, normalized, gates, matrix, logits, status;
    yvex_moe_layer_plan layer = {.hidden_width = WIDTH, .residual_streams = STREAMS,
        .expanded_width = EXPANDED, .mhc_mixing_rows = MIXES, .mhc_sinkhorn_iterations = 20u,
        .rms_epsilon = 1e-6, .mhc_epsilon = 1e-6, .mhc_post_multiplier = 2.0, .routed_experts = ROUTES};
    yvex_error err = {0};
    yvex_backend_operation_facts facts;
    yvex_backend_attention_failure failure;
    unsigned long long router_bytes = router_qtype == YVEX_GGUF_QTYPE_BF16 ? 2u : sizeof(float);
    unsigned long long total = (EXPANDED * MIXES + 3u + MIXES + WIDTH) * sizeof(float) +
        WIDTH * ROUTES * router_bytes, offset = 0u;
    float *data = calloc(ROWS * (EXPANDED + RESULT * 2u), sizeof(float));
    YVEX_TEST_ASSERT(data, "CUDA ingress control host storage");
    float *input = data, *expected = input + ROWS * EXPANDED, *actual = expected + ROWS * RESULT;
    for (size_t i = 0u; i < ROWS * EXPANDED; ++i)
        input[i] = (float)((int)(i * 29u % 1021u) - 510) / 317.0f;
    YVEX_TEST_ASSERT(yvex_backend_open(&backend, &options, &err) == YVEX_OK, "CUDA ingress control backend");
    unsigned char *mapped = NULL;
    yvex_backend_tensor_desc desc = {.name = "ingress-control-weights", .dtype = YVEX_DTYPE_I8,
        .rank = 1u, .dims = {total}, .bytes = total};
    YVEX_TEST_ASSERT(yvex_backend_resident_alloc(backend, &desc, &resident, &mapped, &err) == YVEX_OK &&
        yvex_backend_resident_attach(backend, mapped, total, resident, 1u, &err) == YVEX_OK,
        "CUDA ingress control exact resident parameters");
    for (size_t i = 0u; i < 5u; ++i) {
        unsigned long long n = widths[i] * counts[i], element_bytes = i == 4u ? router_bytes : sizeof(float);
        unsigned long long bytes = n * element_bytes, address;
        float *values = (float *)(mapped + offset);
        for (unsigned long long j = 0u; j < n; ++j) {
            float value = i == 0u ? (float)((int)(j * 17u % 127u) - 63) / 1024.0f :
                i == 1u ? (float)(j + 1u) / 4.0f :
                i == 2u ? (float)((int)(j % 7u) - 3) / 8.0f : 0.5f + (float)(j % 11u) / 16.0f;
            if (i == 4u && router_qtype == YVEX_GGUF_QTYPE_BF16)
                ((unsigned short *)values)[j] = yvex_quant_bf16_encode(value);
            else values[j] = value;
        }
        unsigned int qtype = i == 4u ? router_qtype : YVEX_GGUF_QTYPE_F32;
        layer.tensor_ids[slots[i]] = 32u + i; layer.qtypes[slots[i]] = qtype;
        parameters[i] = (yvex_program_kernel_parameter){.tensor_id = 32u + i,
            .weight = {mapped + offset, bytes, counts[i], widths[i], widths[i] * element_bytes, qtype}};
        weights[i] = (yvex_backend_attention_weight){.present = 1, .qtype = qtype,
            .row_width = widths[i], .row_count = counts[i], .row_bytes = widths[i] * element_bytes,
            .encoded_bytes = bytes};
        YVEX_TEST_ASSERT(yvex_backend_resident_resolve(backend, mapped + offset, bytes, &address) ==
            YVEX_BACKEND_RESIDENT_HIT, "CUDA ingress control resident address");
        addresses[i] = address; offset += bytes;
    }
    YVEX_TEST_ASSERT(yvex_moe_ingress_program_import(&program, &layer, identity, identity, ROWS, &err) == YVEX_OK &&
        yvex_program_stage_open(&stage, program, parameters, 5u, backend, ROWS, 1, 0u, 0u, &err) == YVEX_OK,
        "CUDA ingress control compiled stage");
    const yvex_cuda_attention_operations *ops = yvex_cuda_attention_operations_get();
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    yvex_cuda_work work = {.backend = backend, .state = state, .variant = YVEX_BACKEND_VARIANT_ATTENTION_ENCODED};
    YVEX_TEST_ASSERT(ops->allocate(&work, &x, ROWS * EXPANDED * sizeof(float), input, 0, "test.ingress.x", &failure, &err) == YVEX_OK &&
        ops->allocate(&work, &mixes, ROWS * MIXES * sizeof(float), NULL, 0, "test.ingress.mixes", &failure, &err) == YVEX_OK &&
        ops->allocate(&work, &normalized, ROWS * WIDTH * sizeof(float), NULL, 0, "test.ingress.norm", &failure, &err) == YVEX_OK &&
        ops->allocate(&work, &gates, ROWS * STREAMS * sizeof(float), NULL, 0, "test.ingress.gates", &failure, &err) == YVEX_OK &&
        ops->allocate(&work, &matrix, ROWS * STREAMS * STREAMS * sizeof(float), NULL, 0, "test.ingress.matrix", &failure, &err) == YVEX_OK &&
        ops->allocate(&work, &logits, ROWS * ROUTES * sizeof(float), NULL, 0, "test.ingress.logits", &failure, &err) == YVEX_OK &&
        ops->allocate(&work, &status, sizeof(int), NULL, 1, "test.ingress.status", &failure, &err) == YVEX_OK,
        "CUDA ingress control private numerical buffers");
    unsigned long long streams = STREAMS, width = WIDTH, mix_count = MIXES, rows = ROWS;
    void *args[] = {&x, &mixes, &addresses[1], &addresses[2], &streams, &width, &mix_count,
        &layer.mhc_sinkhorn_iterations, &layer.rms_epsilon, &layer.mhc_epsilon, &layer.mhc_post_multiplier,
        &normalized, &gates, &matrix, &rows, &status};
    int rc = ops->matvec(&work, &weights[0], addresses[0], 0u, MIXES, ROWS, x, mixes, 0,
        status, "test.ingress.projection", &failure, &err);
    if (rc == YVEX_OK) rc = ops->launch(&work, state->residual_mhc_pre_function, ROWS, 256u,
        (STREAMS + 1u + 256u) * sizeof(double), args, "test.ingress.pre", &failure, &err);
    if (rc == YVEX_OK) rc = ops->weighted_norm(&work, normalized, WIDTH, ROWS, &weights[3], addresses[3],
        layer.rms_epsilon, status, "test.ingress.norm", &failure, &err);
    if (rc == YVEX_OK) rc = ops->matvec(&work, &weights[4], addresses[4], 0u, ROUTES, ROWS,
        normalized, logits, 0, status, "test.ingress.router", &failure, &err);
    int observed_status = -1;
    if (rc == YVEX_OK) rc = ops->download(&work, expected, normalized, ROWS * WIDTH * sizeof(float), "test.ingress.norm", &failure, &err);
    if (rc == YVEX_OK) rc = ops->download(&work, expected + ROWS * WIDTH, gates, ROWS * STREAMS * sizeof(float), "test.ingress.gates", &failure, &err);
    if (rc == YVEX_OK) rc = ops->download(&work, expected + ROWS * (WIDTH + STREAMS), matrix,
        ROWS * STREAMS * STREAMS * sizeof(float), "test.ingress.matrix", &failure, &err);
    if (rc == YVEX_OK) rc = ops->download(&work, expected + ROWS * (RESULT - ROUTES), logits,
        ROWS * ROUTES * sizeof(float), "test.ingress.logits", &failure, &err);
    if (rc == YVEX_OK) rc = ops->download(&work, &observed_status, status, sizeof(int), "test.ingress.status", &failure, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK && observed_status == 0, "retained ingress control completes normally");
    rc = yvex_program_stage_host(stage, ROWS, (const float *[]){input}, 1u,
        (float *[]){actual, actual + ROWS * WIDTH, actual + ROWS * (WIDTH + STREAMS),
            actual + ROWS * (RESULT - ROUTES)}, 4u, NULL, NULL, &facts, &err);
    if (rc != YVEX_OK) fprintf(stderr, "CUDA ingress control: %s\n", yvex_error_message(&err));
    YVEX_TEST_ASSERT(rc == YVEX_OK, "compiled ingress control executes");
    double maximum = 0.0;
    size_t worst = 0u, different = 0u;
    for (size_t i = 0u; i < ROWS * RESULT; ++i) {
        double error = fabs((double)expected[i] - actual[i]);
        if (error > maximum) { maximum = error; worst = i; }
        different += expected[i] != actual[i];
    }
    printf("CUDA ingress retained-kernel preservation: values=%u different=%zu max_abs=%.12g worst=%zu expected=%.9g observed=%.9g tolerance=0\n",
        ROWS * RESULT, different, maximum, worst, (double)expected[worst], (double)actual[worst]);
    YVEX_TEST_ASSERT(different == 0u, "compiled ingress preserves retained wide multi-row numerical composition");
    yvex_device_tensor *device_input = NULL, *device_outputs[4] = {0};
    unsigned long long output_widths[] = {WIDTH, STREAMS, STREAMS * STREAMS, ROUTES};
    desc = (yvex_backend_tensor_desc){.name = "ingress-control-input", .dtype = YVEX_DTYPE_F32,
        .rank = 3u, .dims = {ROWS, STREAMS, WIDTH}, .bytes = ROWS * EXPANDED * sizeof(float)};
    YVEX_TEST_ASSERT(yvex_backend_tensor_alloc(backend, &desc, &device_input, &err) == YVEX_OK &&
        yvex_backend_tensor_write(backend, device_input, input, desc.bytes, &err) == YVEX_OK,
        "independent device input prepares");
    for (size_t i = 0u; i < 4u; ++i) {
        desc = (yvex_backend_tensor_desc){.name = "ingress-control-output", .dtype = YVEX_DTYPE_F32,
            .rank = i == 2u ? 3u : 2u, .dims = {ROWS, i == 0u ? WIDTH : i == 3u ? ROUTES : STREAMS, STREAMS},
            .bytes = ROWS * output_widths[i] * sizeof(float)};
        YVEX_TEST_ASSERT(yvex_backend_tensor_alloc(backend, &desc, &device_outputs[i], &err) == YVEX_OK,
            "independent device output prepares");
    }
    const yvex_device_tensor *borrowed_input = device_input;
    YVEX_TEST_ASSERT(yvex_program_stage_device(stage, ROWS, &borrowed_input, 1u,
        device_outputs, 4u, NULL, NULL, &facts, &err) == YVEX_OK, "compiled device ingress executes");
    offset = 0u;
    for (size_t i = 0u; i < 4u; ++i) {
        YVEX_TEST_ASSERT(yvex_backend_tensor_read(backend, device_outputs[i], actual + offset,
            device_outputs[i]->bytes, &err) == YVEX_OK, "device ingress output is published");
        offset += ROWS * output_widths[i];
    }
    YVEX_TEST_ASSERT(!memcmp(expected, actual, ROWS * RESULT * sizeof(float)),
        "device ingress preserves all retained output bytes");
    printf("CUDA ingress device preservation: values=%u exact=1 tolerance=0\n", ROWS * RESULT);
    printf("CUDA router projection representation: qtype=%u rows=%u logits=%u exact=1\n",
        router_qtype, ROWS, ROWS * ROUTES);
    for (size_t i = 0u; i < 4u; ++i)
        YVEX_TEST_ASSERT(yvex_backend_tensor_release(backend, &device_outputs[i], &err) == YVEX_OK,
            "device output owner releases");
    YVEX_TEST_ASSERT(yvex_backend_tensor_release(backend, &device_input, &err) == YVEX_OK,
        "device input owner releases");
    YVEX_TEST_ASSERT(yvex_cuda_work_cleanup(&work, &err) == YVEX_OK && yvex_program_stage_close(&stage, &err) == YVEX_OK,
        "CUDA ingress control stages and private work release");
    yvex_program_physical_close(&program);
    YVEX_TEST_ASSERT(yvex_backend_resident_detach(backend, &err) == YVEX_OK &&
        yvex_backend_tensor_release(backend, &resident, &err) == YVEX_OK, "CUDA ingress control resident release");
    yvex_backend_close(backend); free(data);
    return 0;
}
#endif
