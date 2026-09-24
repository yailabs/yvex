/* Encoded expert rows: independent CPU decode + F64 dot, including exact exceptional recovery. */
#include <float.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yvex/internal/backend.h>
#include <yvex/internal/neural_operations.h>
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

typedef struct {
    float gate, up, input, route_weight, output;
    unsigned long long selected;
    int status;
} moe_grouped_scalar_storage;

static int moe_grouped_swiglu_precision(yvex_backend *backend)
{
    moe_grouped_scalar_storage host = {
        .gate = 0.4303571283817291f, .up = 1.6795454025268555f,
        .input = 1.0f, .route_weight = 0.9476000070571899f,
        .output = -123.0f, .selected = 0u};
    moe_grouped_scalar_storage observed;
    yvex_backend_tensor_desc desc = {
        .name = "moe-grouped-swiglu-precision", .dtype = YVEX_DTYPE_I8,
        .rank = 1u, .dims = {sizeof(host)}, .bytes = sizeof(host)};
    yvex_device_tensor *arena = NULL;
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    yvex_error err = {0};
    float reference = 0.0f;
    unsigned long long row_bytes = sizeof(float), expert_bytes = sizeof(float);
    unsigned long long topk = 1u, experts = 1u, input_extent = 1u, intermediate_width = 1u;
    unsigned int qtype = YVEX_GGUF_QTYPE_F32;
    int q8_input = 0, device_wide = 0;
    double limit = 7.0;
    YVEX_TEST_ASSERT(yvex_clamped_swiglu_bf16(&host.gate, &host.up, 1u, limit,
        host.route_weight, &reference, &err) == YVEX_OK,
        "canonical scalar F64/BF16 grouped expert oracle is available");
    YVEX_TEST_ASSERT(yvex_backend_tensor_alloc(backend, &desc, &arena, &err) == YVEX_OK &&
        yvex_backend_tensor_write(backend, arena, &host, sizeof(host), &err) == YVEX_OK,
        "upload grouped expert precision fixture");
    CUdeviceptr base = yvex_cuda_activation_pointer(backend, arena);
    CUdeviceptr gate = base + offsetof(moe_grouped_scalar_storage, gate);
    CUdeviceptr up = base + offsetof(moe_grouped_scalar_storage, up);
    CUdeviceptr input = base + offsetof(moe_grouped_scalar_storage, input);
    CUdeviceptr weight = base + offsetof(moe_grouped_scalar_storage, route_weight);
    CUdeviceptr output = base + offsetof(moe_grouped_scalar_storage, output);
    CUdeviceptr selected = base + offsetof(moe_grouped_scalar_storage, selected);
    CUdeviceptr status = base + offsetof(moe_grouped_scalar_storage, status);
    void *params[] = {&gate, &row_bytes, &expert_bytes, &qtype,
        &up, &row_bytes, &expert_bytes, &qtype, &selected, &weight,
        &topk, &experts, &input, &input_extent, &q8_input,
        &intermediate_width, &limit, &output, &status};
    int rc = yvex_cuda_launch(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
        state->moe_grouped_up_function, 1u, 256u, 0u, params,
        "cuda.test.moe-grouped-swiglu", &err);
    if (rc == YVEX_OK) rc = yvex_cuda_launch_synchronize(backend,
        YVEX_BACKEND_VARIANT_ATTENTION_ENCODED, &device_wide,
        "cuda.test.moe-grouped-swiglu", &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK && yvex_backend_tensor_read(backend, arena,
        &observed, sizeof(observed), &err) == YVEX_OK,
        "read grouped expert precision result");
    YVEX_TEST_ASSERT(observed.status == 0 && observed.output == reference,
        "grouped expert uses canonical F64 math before one BF16 publication");
    printf("moe grouped SiLU precision: expected=%.9g observed=%.9g tolerance=0 status=%d\n",
        reference, observed.output, observed.status);
    host.gate = NAN;
    host.output = -123.0f;
    YVEX_TEST_ASSERT(yvex_backend_tensor_write(backend, arena, &host, sizeof(host), &err) == YVEX_OK,
        "reset grouped expert nonfinite fixture");
    rc = yvex_cuda_launch(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
        state->moe_grouped_up_function, 1u, 256u, 0u, params,
        "cuda.test.moe-grouped-swiglu-nonfinite", &err);
    if (rc == YVEX_OK) rc = yvex_cuda_launch_synchronize(backend,
        YVEX_BACKEND_VARIANT_ATTENTION_ENCODED, &device_wide,
        "cuda.test.moe-grouped-swiglu-nonfinite", &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK && yvex_backend_tensor_read(backend, arena,
        &observed, sizeof(observed), &err) == YVEX_OK &&
        observed.status != 0 && observed.output == -123.0f,
        "nonfinite grouped expert input refuses without publishing output");
    YVEX_TEST_ASSERT(yvex_backend_tensor_release(backend, &arena, &err) == YVEX_OK,
        "release grouped expert precision fixture");
    return 0;
}

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

typedef struct {
    float logits[6], bias[2], scores[6], weights[6];
    unsigned long long selected[6], hash_selected[2];
    int32_t table[6];
    unsigned int tokens[3];
    int status;
} moe_route_storage;

/* The source Gate ranks F32 scores + F32 correction bias. A sub-ULP bias
 * must not become an FP64 ranking discriminator in the single-row lowering.
 * Tie order is YVEX's declared low-ordinal rule, not a PyTorch topk tie claim. */
static int moe_route_precision(yvex_backend *backend, unsigned int scenario)
{
    moe_route_storage host = {0}, observed[2];
    yvex_backend_tensor_desc desc = {.name = "routing-precision", .dtype = YVEX_DTYPE_I8,
        .rank = 1u, .dims = {sizeof(host)}, .bytes = sizeof(host)};
    yvex_device_tensor *arena = NULL;
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    yvex_error err = {0};
    unsigned long long rows = 3u, experts = 2u, topk = 2u, table_rows = 3u, columns = 2u;
    unsigned long long row_bytes = 2u * sizeof(int32_t);
    unsigned int router = scenario >= 3u && scenario <= 5u ? 0u : 1u;
    int normalize = 1, device_wide = 0;
    double scaling = 1.0;
    memset(observed, 0, sizeof(observed));
    host.bias[1] = scenario == 0u ? 0x1p-26f : scenario == 1u ? 0x1p-22f : 0.0f;
    for (size_t i = 0u; i < 3u; ++i) {
        host.tokens[i] = (unsigned int)i;
        host.table[2u * i] = scenario == 5u ? 2 : 1;
        host.table[2u * i + 1u] = scenario == 4u ? 1 : 0;
    }
    host.hash_selected[0] = (unsigned long long)host.table[0];
    host.hash_selected[1] = (unsigned long long)host.table[1];
    if (scenario == 6u) host.logits[0] = NAN;
    YVEX_TEST_ASSERT(yvex_backend_tensor_alloc(backend, &desc, &arena, &err) == YVEX_OK,
        "allocate routing precision fixture");
    CUdeviceptr base = yvex_cuda_activation_pointer(backend, arena);
    CUdeviceptr logits = base + offsetof(moe_route_storage, logits), bias = base + offsetof(moe_route_storage, bias);
    CUdeviceptr hash = base + offsetof(moe_route_storage, hash_selected), table = base + offsetof(moe_route_storage, table);
    CUdeviceptr tokens = base + offsetof(moe_route_storage, tokens), scores = base + offsetof(moe_route_storage, scores);
    CUdeviceptr selected = base + offsetof(moe_route_storage, selected), weights = base + offsetof(moe_route_storage, weights);
    CUdeviceptr status = base + offsetof(moe_route_storage, status);
    void *single[] = {&logits, &bias, &hash, &router, &experts, &topk, &normalize, &scaling,
        &scores, &selected, &weights, &status};
    void *multiple[] = {&logits, &bias, &table, &tokens, &router, &rows, &experts, &topk,
        &table_rows, &columns, &row_bytes, &normalize, &scaling, &scores, &selected, &weights, &status};
    for (size_t mode = 0u; mode < 2u; ++mode) {
        YVEX_TEST_ASSERT(yvex_backend_tensor_write(backend, arena, &host, sizeof(host), &err) == YVEX_OK,
            "reset exact routing inputs and status");
        int rc = yvex_cuda_launch(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
            mode ? state->moe_route_rows_function : state->moe_route_function,
            mode ? 3u : 1u, mode ? 256u : 1u, 0u, mode ? multiple : single, "cuda.test.routing-precision", &err);
        if (rc == YVEX_OK) rc = yvex_cuda_launch_synchronize(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
            &device_wide, "cuda.test.routing-precision", &err);
        YVEX_TEST_ASSERT(rc == YVEX_OK && yvex_backend_tensor_read(backend, arena,
            &observed[mode], sizeof(host), &err) == YVEX_OK, "observe both routing implementations");
    }
    YVEX_TEST_ASSERT(yvex_backend_tensor_release(backend, &arena, &err) == YVEX_OK,
        "release routing fixture before comparison");
    unsigned long long first = scenario == 1u || scenario == 3u ? 1u : 0u;
    printf("MoE routing precision: scenario=%u expected=%llu,%llu single=%llu,%llu rows=%llu,%llu "
        "status=%d/%d score=%.9g/%.9g weight=%.9g/%.9g tolerance=0\n", scenario, first, 1u - first,
        observed[0].selected[0], observed[0].selected[1], observed[1].selected[0], observed[1].selected[1],
        observed[0].status, observed[1].status, observed[0].scores[0], observed[1].scores[0],
        observed[0].weights[0], observed[1].weights[0]);
    for (size_t mode = 0u; mode < 2u; ++mode) {
        if (scenario >= 4u) {
            YVEX_TEST_ASSERT(observed[mode].status != 0, "invalid expert/nonfinite routing fails closed");
            continue;
        }
        YVEX_TEST_ASSERT(observed[mode].status == 0, "valid routing succeeds");
        for (size_t row = 0u; row < (mode ? 3u : 1u); ++row) {
            YVEX_TEST_ASSERT(observed[mode].selected[2u * row] == first &&
                observed[mode].selected[2u * row + 1u] == 1u - first,
                "F32 ranking and explicit low-ordinal ties are independent of row lowering");
            for (size_t rank = 0u; rank < 2u; ++rank)
                YVEX_TEST_ASSERT(observed[mode].scores[2u * row + rank] == (float)sqrt(log(2.0)) &&
                    observed[mode].weights[2u * row + rank] == 0.5f,
                    "zero-logit analytic score and normalized weight are exact and unaffected by bias");
        }
    }
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
    if (moe_grouped_swiglu_precision(backend)) return 1;
    for (unsigned int scenario = 0u; scenario < 7u; ++scenario)
        if (moe_route_precision(backend, scenario)) return 1;
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
