/* Encoded expert rows: independent CPU decode + F64 dot, including exact exceptional recovery. */
#include <float.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yvex/internal/backend.h>
#include <yvex/internal/quant_numeric.h>
#include "src/backend/cuda/private.h"
#include "tests/test.h"

enum { MOE_ROWS_WIDTH = 9, MOE_ROWS_EXPERTS = 3, MOE_ROWS_PAIRS = 4,
       MOE_ROWS_BLOCKS = 17, MOE_ROWS_Q8_BYTES = 292 };
typedef struct {
    unsigned char weights[MOE_ROWS_WIDTH * MOE_ROWS_EXPERTS * MOE_ROWS_BLOCKS * 84];
    unsigned char inputs[MOE_ROWS_PAIRS * MOE_ROWS_BLOCKS * MOE_ROWS_Q8_BYTES];
    unsigned long long selected[MOE_ROWS_PAIRS], order[MOE_ROWS_PAIRS];
    float before_canary, outputs[MOE_ROWS_PAIRS * MOE_ROWS_WIDTH], after_canary;
    int status;
} moe_rows_storage;

static void moe_rows_input(moe_rows_storage *s, unsigned int qtype,
                           unsigned long long blocks, unsigned int bytes, unsigned int scenario)
{
    for (size_t i = 0u; i < sizeof(s->weights); ++i) s->weights[i] = (unsigned char)(i * 31u + 7u);
    for (unsigned long long i = 0ull; i < MOE_ROWS_WIDTH * MOE_ROWS_EXPERTS * blocks; ++i) {
        unsigned char *p = s->weights + i * bytes;
        unsigned int scale = qtype == YVEX_GGUF_QTYPE_Q2_K ? 80u : 0u;
        p[scale] = scenario == 4u ? 0xffu : 0u;
        p[scale + 1u] = scenario == 4u ? 0x7bu : scenario == 6u ? 0x7cu : scenario == 7u ? 0x7eu : 0x20u;
        if (qtype == YVEX_GGUF_QTYPE_Q2_K) { p[82] = 0u; p[83] = 0x18u; }
    }
    for (unsigned long long i = 0ull; i < MOE_ROWS_PAIRS * blocks; ++i) {
        unsigned char *p = s->inputs + i * MOE_ROWS_Q8_BYTES;
        float scale = scenario == 3u ? NAN : scenario == 4u ? FLT_MAX :
                      scenario == 5u ? INFINITY : 1.0f / 4096.0f;
        memcpy(p, &scale, sizeof(scale));
        for (unsigned int j = 0u; j < 256u; ++j)
            p[4u + j] = scenario == 4u ? 0u : (unsigned char)((int)((i * 13ull + j * 7ull) % 255ull) - 127);
        for (unsigned int j = 0u; j < 16u; ++j) {
            short sum = 0;
            for (unsigned int k = 0u; k < 16u; ++k) sum += (signed char)p[4u + j * 16u + k];
            memcpy(p + 260u + j * 2u, &sum, sizeof(sum));
        }
    }
    for (unsigned int pair = 0u; pair < MOE_ROWS_PAIRS; ++pair) {
        s->selected[pair] = pair % MOE_ROWS_EXPERTS;
        s->order[pair] = MOE_ROWS_PAIRS - 1u - pair;
    }
    if (scenario == 1u) s->status = 1;
    if (scenario == 2u) s->selected[0] = MOE_ROWS_EXPERTS;
    s->before_canary = s->after_canary = 12345.0f;
}

static int moe_rows_check(yvex_backend *backend, unsigned int qtype,
                          unsigned long long blocks, unsigned int scenario, int up_stage)
{
    const unsigned int bytes = qtype == YVEX_GGUF_QTYPE_Q2_K ? 84u : 66u;
    unsigned long long row_bytes = blocks * bytes, expert_bytes = row_bytes * MOE_ROWS_WIDTH;
    unsigned long long pairs = MOE_ROWS_PAIRS, topk = 2ull, experts = MOE_ROWS_EXPERTS;
    unsigned long long width = MOE_ROWS_WIDTH, minimum = 0ull;
    int q8_input = 1, device_wide = 0, rc;
    moe_rows_storage *host = calloc(1u, sizeof(*host)), *observed = calloc(1u, sizeof(*observed));
    yvex_backend_tensor_desc descriptor = {.name = "moe-encoded-rows", .dtype = YVEX_DTYPE_I8, .rank = 1u};
    yvex_device_tensor *arena = NULL;
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    yvex_error err;
    CUdeviceptr base, weights, selected, order, input, output, status, absent = 0;
    double maximum_error = 0.0, maximum_ratio = 0.0;
    double limit = 7.0;
    YVEX_TEST_ASSERT(host && observed, "allocate encoded expert oracle");
    moe_rows_input(host, qtype, blocks, bytes, scenario);
    descriptor.bytes = descriptor.dims[0] = sizeof(*host);
    YVEX_TEST_ASSERT(yvex_backend_tensor_alloc(backend, &descriptor, &arena, &err) == YVEX_OK &&
        yvex_backend_tensor_write(backend, arena, host, sizeof(*host), &err) == YVEX_OK, "upload encoded expert fixture");
    base = yvex_cuda_activation_pointer(backend, arena);
    weights = base + offsetof(moe_rows_storage, weights); selected = base + offsetof(moe_rows_storage, selected);
    order = base + offsetof(moe_rows_storage, order); input = base + offsetof(moe_rows_storage, inputs);
    output = base + offsetof(moe_rows_storage, outputs); status = base + offsetof(moe_rows_storage, status);
    void *params[] = {&weights, &row_bytes, &expert_bytes, &qtype, &selected, &order,
        &absent, &absent, &absent, &minimum, &pairs, &topk, &experts, &input, &blocks, &q8_input, &width, &output, &status};
    void *up_params[] = {&weights, &row_bytes, &expert_bytes, &qtype,
        &weights, &row_bytes, &expert_bytes, &qtype, &selected, &absent, &order,
        &absent, &absent, &absent, &minimum, &pairs, &topk, &experts, &input, &blocks, &q8_input,
        &width, &limit, &output, &status};
    rc = yvex_cuda_launch(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
        up_stage ? state->moe_grouped_up_rows_function : state->moe_grouped_down_rows_function,
        (unsigned int)(pairs * 2ull), 256u, 0u, up_stage ? up_params : params,
        "cuda.test.moe-encoded-rows", &err);
    if (rc == YVEX_OK) rc = yvex_cuda_launch_synchronize(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
        &device_wide, "cuda.test.moe-encoded-rows", &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK && yvex_backend_tensor_read(
        backend, arena, observed, sizeof(*observed), &err) == YVEX_OK, "read expert row result");
    if ((scenario >= 1u && scenario <= 3u) || scenario >= 5u) {
        YVEX_TEST_ASSERT(observed->status != 0, "prior error/invalid expert/nonfinite input prevents publication");
    } else {
        YVEX_TEST_ASSERT(observed->status == 0, "valid expert row or exceptional exact recovery succeeds");
        for (unsigned long long pair = 0ull; pair < pairs; ++pair)
            for (unsigned long long row = 0ull; row < width; ++row) {
                unsigned long long source = host->order[pair], expert = host->selected[source];
                double expected = 0.0;
                for (unsigned long long block = 0ull; block < blocks; ++block) {
                    float decoded[256], scale;
                    yvex_quant_failure failure;
                    const unsigned char *w = host->weights + expert * expert_bytes + row * row_bytes + block * bytes;
                    unsigned long long input_row = up_stage ? source / topk : pair;
                    const unsigned char *x = host->inputs + (input_row * blocks + block) * MOE_ROWS_Q8_BYTES;
                    YVEX_TEST_ASSERT(yvex_quant_decode_block(qtype, w, bytes, decoded, 256u,
                        &failure, &err) == YVEX_OK, "independent CPU weight decoder");
                    memcpy(&scale, x, sizeof(scale));
                    for (unsigned int i = 0u; i < 256u; ++i)
                        expected += (double)decoded[i] * scale * (signed char)x[4u + i];
                }
                if (up_stage) {
                    double g = fmin(expected, limit), u = fmax(-limit, fmin(expected, limit));
                    double silu = g >= 0.0 ? g / (1.0 + exp(-g)) : g * exp(g) / (1.0 + exp(g));
                    expected = silu * u;
                }
                unsigned long long result_row = up_stage ? pair : source;
                double difference = fabs((double)observed->outputs[result_row * width + row] - expected);
                double tolerance = fabs(expected) / 256.0 + 2e-5;
                if (difference > maximum_error) maximum_error = difference;
                if (difference / tolerance > maximum_ratio) maximum_ratio = difference / tolerance;
                YVEX_TEST_ASSERT(isfinite(observed->outputs[result_row * width + row]) && difference <= tolerance,
                    "expert output matches decoded-weight F64 dot plus BF16 rounding");
                if (scenario == 4u) YVEX_TEST_ASSERT(observed->outputs[result_row * width + row] == 0.0f,
                    "overflowed fast path recovers exact zero through F64");
            }
    }
    YVEX_TEST_ASSERT(!memcmp(host, observed, offsetof(moe_rows_storage, before_canary)),
        "expert execution preserves weights, inputs and worklist");
    YVEX_TEST_ASSERT(observed->before_canary == 12345.0f && observed->after_canary == 12345.0f,
        "partial row groups preserve output canaries");
    printf("moe encoded rows: stage=%s qtype=%u blocks=%llu scenario=%u values=%llu status=%d max_abs=%.12g "
           "worst_error_over_tolerance=%.9g recovery_exact=%s\n", up_stage ? "up" : "down", qtype, blocks, scenario,
           pairs * width, observed->status, maximum_error, maximum_ratio, scenario == 4u ? "true" : "n/a");
    YVEX_TEST_ASSERT(yvex_backend_tensor_release(backend, &arena, &err) == YVEX_OK, "release expert fixture");
    free(host); free(observed);
    return 0;
}

int yvex_cuda_test_moe_rows(void)
{
    const unsigned long long blocks[] = {1ull, 7ull, 8ull, 15ull, 16ull, 17ull};
    const unsigned int qtypes[] = {YVEX_GGUF_QTYPE_IQ2_XXS, YVEX_GGUF_QTYPE_Q2_K};
    yvex_backend_options options = {.kind = YVEX_BACKEND_KIND_CUDA};
    yvex_backend *backend = NULL;
    yvex_error err;
    int rc = yvex_backend_open(&backend, &options, &err);
    if (rc == YVEX_ERR_UNSUPPORTED) return 77;
    YVEX_TEST_ASSERT(rc == YVEX_OK, "open expert CUDA backend");
    for (int up_stage = 0; up_stage <= 1; ++up_stage)
    for (size_t q = 0u; q < sizeof(qtypes) / sizeof(qtypes[0]); ++q) {
        for (size_t i = 0u; i < sizeof(blocks) / sizeof(blocks[0]); ++i)
            if (moe_rows_check(backend, qtypes[q], blocks[i], 0u, up_stage)) return 1;
        for (unsigned int scenario = 1u; scenario <= 7u; ++scenario)
            if (moe_rows_check(backend, qtypes[q], q ? 8ull : 16ull, scenario, up_stage)) return 1;
    }
    yvex_backend_close(backend);
    return 0;
}
