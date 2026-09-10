/* Independent decoded F64 oracle; nonfinite operands must fail before nonlinear clamping. */
#include <float.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yvex/internal/backend.h>
#include <yvex/internal/quant_numeric.h>
#include "src/backend/cuda/private.h"
#include "tests/test.h"

enum { DOT_WIDTH = 128, DOT_ROWS = 3, DOT_TOKENS = 2 };
typedef struct {
    unsigned char weights[DOT_ROWS * DOT_WIDTH * sizeof(float)];
    float input[DOT_TOKENS * DOT_WIDTH], route;
    unsigned long long selected;
    float before_canary, output[DOT_ROWS * DOT_TOKENS], after_canary;
    int status;
} dot_storage;

static int dot_fixture(dot_storage *s, unsigned int qtype, unsigned int scenario,
                       double expected[DOT_ROWS * DOT_TOKENS], unsigned long long *row_bytes)
{
    const yvex_gguf_qtype_geometry *g = yvex_gguf_qtype_geometry_find(qtype);
    float values[DOT_WIDTH], decoded[DOT_WIDTH];
    yvex_quant_failure failure;
    yvex_error err;
    *row_bytes = DOT_WIDTH / g->block_size * g->bytes_per_block;
    for (unsigned int i = 0u; i < DOT_WIDTH; ++i) values[i] = 1.0f;
    for (unsigned int row = 0u; row < DOT_ROWS; ++row)
        for (unsigned int i = 0u; i < DOT_WIDTH; i += g->block_size) {
            size_t written = 0u;
            unsigned char *p = s->weights + row * *row_bytes + i / g->block_size * g->bytes_per_block;
            YVEX_TEST_ASSERT(yvex_quant_encode_block(qtype, values + i, g->block_size,
                p, g->bytes_per_block, &written, &failure, &err) == YVEX_OK &&
                written == g->bytes_per_block, "encode finite dot fixture");
        }
    for (unsigned int token = 0u; token < DOT_TOKENS; ++token)
        for (unsigned int i = 0u; i < DOT_WIDTH; ++i)
            s->input[token * DOT_WIDTH + i] = scenario == 3u ?
                (i == 0u || i == 32u ? FLT_MAX : i == 64u || i == 96u ? -FLT_MAX : 0.0f)
                : (float)((int)((i * 7u + token) % 31u) - 15) / 16.0f;
    for (unsigned int row = 0u; row < DOT_ROWS; ++row) {
        for (unsigned int i = 0u; i < DOT_WIDTH; i += g->block_size)
            YVEX_TEST_ASSERT(yvex_quant_decode_block(qtype,
                s->weights + row * *row_bytes + i / g->block_size * g->bytes_per_block,
                g->bytes_per_block, decoded + i, g->block_size, &failure, &err) == YVEX_OK,
                "independent CPU decode for F64 dot oracle");
        for (unsigned int token = 0u; token < DOT_TOKENS; ++token) {
            double sum = 0.0;
            for (unsigned int i = 0u; i < DOT_WIDTH; ++i)
                sum += (double)decoded[i] * s->input[token * DOT_WIDTH + i];
            expected[token * DOT_ROWS + row] = sum;
        }
    }
    if (scenario == 1u) s->input[37] = NAN;
    if (scenario == 2u) s->input[37] = INFINITY;
    if (scenario == 4u) {
        if (qtype == YVEX_GGUF_QTYPE_F32) { uint32_t bits = 0x7fc00000u; memcpy(s->weights + 37u * 4u, &bits, 4u); }
        else if (qtype == YVEX_GGUF_QTYPE_MXFP4) s->weights[17] = 255u;
        else { uint16_t bits = qtype == YVEX_GGUF_QTYPE_F16 ? 0x7e00u : 0x7fc0u; memcpy(s->weights + 37u * 2u, &bits, 2u); }
    }
    if (scenario == 5u) s->status = 1;
    s->route = 1.0f;
    s->before_canary = s->after_canary = 12345.0f;
    for (unsigned int i = 0u; i < DOT_ROWS * DOT_TOKENS; ++i) s->output[i] = 12345.0f;
    return 0;
}

static int dot_case(yvex_backend *backend, unsigned int qtype, unsigned int scenario, unsigned int mode)
{
    dot_storage before = {0}, after;
    double expected[DOT_ROWS * DOT_TOKENS], maximum_error = 0.0, limit = 10.0;
    unsigned long long bytes, width = DOT_WIDTH, rows = DOT_ROWS, tokens = DOT_TOKENS;
    unsigned long long zero = 0ull, one = 1ull, expert_bytes;
    int no = 0, device_wide = 0, rc, failure = scenario == 1u || scenario == 2u || scenario >= 4u;
    yvex_backend_tensor_desc d = {.name = "dot-finiteness", .dtype = YVEX_DTYPE_I8, .rank = 1u};
    yvex_device_tensor *arena = NULL;
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    yvex_error err;
    CUdeviceptr base, weight, input, output, status, selected, route, absent = 0ull;
    if (dot_fixture(&before, qtype, scenario, expected, &bytes)) return 1;
    expert_bytes = bytes * rows;
    d.bytes = d.dims[0] = sizeof(before);
    YVEX_TEST_ASSERT(yvex_backend_tensor_alloc(backend, &d, &arena, &err) == YVEX_OK &&
        yvex_backend_tensor_write(backend, arena, &before, sizeof(before), &err) == YVEX_OK,
        "upload finite/invalid dot inputs");
    base = yvex_cuda_activation_pointer(backend, arena);
    weight = base + offsetof(dot_storage, weights); input = base + offsetof(dot_storage, input);
    output = base + offsetof(dot_storage, output); status = base + offsetof(dot_storage, status);
    selected = base + offsetof(dot_storage, selected); route = base + offsetof(dot_storage, route);
    void *ordinary[] = {&weight, &bytes, &width, &zero, &rows, &tokens, &qtype,
        &input, &width, &no, &no, &no, &absent, &output, &rows, &no, &status};
    void *grouped[] = {&weight, &bytes, &width, &one, &rows, &one, &tokens, &qtype,
        &input, &width, &output, &rows, &no, &status};
    void *expert[] = {&weight, &bytes, &expert_bytes, &qtype, &weight, &bytes, &expert_bytes, &qtype,
        &selected, &route, &one, &one, &input, &width, &no, &rows, &limit, &output, &status};
    CUfunction function = mode == 0u ? state->qtype_matvec_function :
        mode == 1u ? state->qtype_grouped_rows_function : state->moe_grouped_up_function;
    rc = yvex_cuda_launch(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED, function, 1u, 256u, 0u,
        mode == 0u ? ordinary : mode == 1u ? grouped : expert, "cuda.test.dot-finiteness", &err);
    if (rc == YVEX_OK) rc = yvex_cuda_launch_synchronize(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
        &device_wide, "cuda.test.dot-finiteness", &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK && yvex_backend_tensor_read(backend, arena, &after, sizeof(after), &err) == YVEX_OK,
        "read dot completion/status");
    YVEX_TEST_ASSERT((after.status != 0) == failure, "nonfinite input refuses before clamp; finite overflow remains recoverable");
    if (!failure) for (unsigned int i = 0u; i < (mode == 2u ? DOT_ROWS : DOT_ROWS * DOT_TOKENS); ++i) {
        double value = expected[i];
        if (mode == 2u) {
            float g = scenario == 3u ? 10.0f : fminf((float)value, 10.0f);
            float u = scenario == 3u ? 10.0f : fmaxf(-10.0f, fminf((float)value, 10.0f));
            float silu = g >= 0.0f ? g / (1.0f + expf(-g)) : g * expf(g) / (1.0f + expf(g));
            float result = silu * u;
            uint32_t bits; memcpy(&bits, &result, 4u);
            bits = (bits + 0x7fffu + ((bits >> 16u) & 1u)) & 0xffff0000u;
            memcpy(&result, &bits, 4u); value = result;
        }
        double difference = fabs((double)after.output[i] - value);
        if (difference > maximum_error) maximum_error = difference;
        YVEX_TEST_ASSERT(isfinite(after.output[i]) && difference == 0.0, "exact finite dot/recovery/BF16 clamp oracle");
    }
    YVEX_TEST_ASSERT(!memcmp(before.weights, after.weights, sizeof(before.weights)) &&
        !memcmp(before.input, after.input, sizeof(before.input)) && after.before_canary == 12345.0f &&
        after.after_canary == 12345.0f, "inputs and bounded output canaries are preserved");
    if (scenario == 5u) YVEX_TEST_ASSERT(!memcmp(before.output, after.output, sizeof(before.output)),
        "prior refusal writes no dot result");
    printf("dot finiteness: qtype=%u mode=%u scenario=%u status=%d max_abs=%.12g tolerance=0 result=pass\n",
        qtype, mode, scenario, after.status, maximum_error);
    YVEX_TEST_ASSERT(yvex_backend_tensor_release(backend, &arena, &err) == YVEX_OK, "release dot fixture");
    return 0;
}

int yvex_cuda_test_dot_finiteness(void)
{
    const unsigned int qtypes[] = {YVEX_GGUF_QTYPE_F32, YVEX_GGUF_QTYPE_F16,
        YVEX_GGUF_QTYPE_BF16, YVEX_GGUF_QTYPE_MXFP4};
    yvex_backend_options options = {.kind = YVEX_BACKEND_KIND_CUDA};
    yvex_backend *backend = NULL;
    yvex_error err;
    int rc = yvex_backend_open(&backend, &options, &err);
    if (rc == YVEX_ERR_UNSUPPORTED) return 77;
    YVEX_TEST_ASSERT(rc == YVEX_OK, "open dot CUDA backend");
    for (unsigned int q = 0u; q < 4u; ++q)
        for (unsigned int mode = 0u; mode < 3u; ++mode)
            for (unsigned int scenario = 0u; scenario < 6u; ++scenario)
                if (dot_case(backend, qtypes[q], scenario, mode)) return 1;
    yvex_backend_close(backend);
    return 0;
}
