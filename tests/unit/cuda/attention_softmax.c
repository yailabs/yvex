/* Bounded block-softmax oracle and shared-reduction lifetime regression. */
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yvex/internal/backend.h>
#include "src/backend/cuda/private.h"
#include "tests/test.h"

enum { SOFTMAX_ROWS = 3, SOFTMAX_TOKENS = 21741 };
typedef struct {
    float scores[SOFTMAX_ROWS * SOFTMAX_TOKENS];
    float before, probabilities[SOFTMAX_ROWS * SOFTMAX_TOKENS], after;
    int status;
} softmax_fixture;

static int softmax_case(yvex_backend *backend, unsigned long long tokens, unsigned int scenario)
{
    softmax_fixture *input = calloc(1u, sizeof(*input));
    softmax_fixture *observed = calloc(1u, sizeof(*observed));
    float *previous = malloc(sizeof(input->probabilities));
    yvex_backend_tensor_desc desc = {.name = "softmax-reduction", .dtype = YVEX_DTYPE_I8,
        .rank = 1u, .dims = {sizeof(*input)}, .bytes = sizeof(*input)};
    yvex_device_tensor *arena = NULL;
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    unsigned long long rows = SOFTMAX_ROWS, start = 5ull;
    int causal = scenario == 1u || scenario == 2u, device_wide = 0, rc;
    CUdeviceptr base, scores, probabilities, status;
    yvex_error err;
    double maximum_error = 0.0, maximum_relative = 0.0;
    YVEX_TEST_ASSERT(input && observed && previous, "allocate bounded softmax oracle");
    for (size_t i = 0u; i < SOFTMAX_ROWS * SOFTMAX_TOKENS; ++i) {
        input->scores[i] = (float)((int)((i * 17u) % 107u) - 53) / 8.0f;
        input->probabilities[i] = -12345.0f;
    }
    input->before = input->after = 12345.0f;
    if (scenario == 2u) input->scores[tokens - 1u] = NAN; /* Masked, not a visible operand. */
    if (scenario == 3u) input->scores[0] = NAN;
    if (scenario == 4u) input->status = 1;
    if (scenario == 5u) input->scores[0] = INFINITY;
    YVEX_TEST_ASSERT(yvex_backend_tensor_alloc(backend, &desc, &arena, &err) == YVEX_OK &&
        yvex_backend_tensor_write(backend, arena, input, sizeof(*input), &err) == YVEX_OK,
        "admit softmax fixture");
    base = yvex_cuda_activation_pointer(backend, arena);
    scores = base + offsetof(softmax_fixture, scores);
    probabilities = base + offsetof(softmax_fixture, probabilities);
    status = base + offsetof(softmax_fixture, status);
    void *params[] = {&scores, &probabilities, &rows, &tokens, &start, &causal, &status};
    for (unsigned int repeat = 0u; repeat < 4u; ++repeat) {
        rc = yvex_cuda_launch(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
            state->gqa_softmax_function, SOFTMAX_ROWS, 256u, 256u * sizeof(float), params,
            "cuda.test.block-softmax", &err);
        if (rc == YVEX_OK) rc = yvex_cuda_launch_synchronize(backend,
            YVEX_BACKEND_VARIANT_ATTENTION_ENCODED, &device_wide, "cuda.test.block-softmax", &err);
        YVEX_TEST_ASSERT(rc == YVEX_OK && yvex_backend_tensor_read(
            backend, arena, observed, sizeof(*observed), &err) == YVEX_OK, "read softmax result");
        YVEX_TEST_ASSERT(!memcmp(input->scores, observed->scores, sizeof(input->scores)) &&
            observed->before == 12345.0f && observed->after == 12345.0f,
            "softmax preserves input and output canaries");
        if (scenario >= 3u) {
            YVEX_TEST_ASSERT(observed->status != 0, "invalid visible operand or prior status refuses publication");
            if (scenario == 4u) YVEX_TEST_ASSERT(!memcmp(input->probabilities,
                observed->probabilities, sizeof(input->probabilities)), "prior error leaves output untouched");
            break;
        }
        YVEX_TEST_ASSERT(observed->status == 0, "finite visible softmax succeeds");
        for (unsigned long long row = 0ull; row < rows; ++row) {
            unsigned long long visible = causal ? start + row + 1ull : tokens;
            double maximum = -INFINITY, denominator = 0.0;
            for (unsigned long long i = 0ull; i < visible; ++i)
                maximum = fmax(maximum, input->scores[row * tokens + i]);
            for (unsigned long long i = 0ull; i < visible; ++i)
                denominator += exp((double)input->scores[row * tokens + i] - maximum);
            for (unsigned long long i = 0ull; i < tokens; ++i) {
                double expected = i < visible
                    ? exp((double)input->scores[row * tokens + i] - maximum) / denominator : 0.0;
                double value = observed->probabilities[row * tokens + i];
                double difference = fabs(value - expected), tolerance = 1e-8 + 2e-6 * fabs(expected);
                if (difference > maximum_error) maximum_error = difference;
                if (expected > 0.0 && difference / expected > maximum_relative)
                    maximum_relative = difference / expected;
                YVEX_TEST_ASSERT(isfinite(value) && difference <= tolerance &&
                    (i < visible || value == 0.0), "block softmax matches independent scalar F64 normalization");
            }
        }
        if (repeat) YVEX_TEST_ASSERT(!memcmp(previous, observed->probabilities,
            sizeof(input->probabilities)), "softmax reuses reduction storage byte-identically");
        memcpy(previous, observed->probabilities, sizeof(input->probabilities));
        for (size_t i = (size_t)(rows * tokens); i < SOFTMAX_ROWS * SOFTMAX_TOKENS; ++i)
            YVEX_TEST_ASSERT(observed->probabilities[i] == -12345.0f, "softmax preserves unused output extent");
    }
    printf("block softmax: rows=%llu tokens=%llu scenario=%u status=%d max_abs=%.12g "
           "max_rel=%.12g tolerance=1e-8+2e-6*abs(expected) exact_repeats=%u\n",
        rows, tokens, scenario, observed->status, maximum_error, maximum_relative, scenario < 3u ? 4u : 0u);
    YVEX_TEST_ASSERT(yvex_backend_tensor_release(backend, &arena, &err) == YVEX_OK, "release softmax fixture");
    free(previous); free(observed); free(input);
    return 0;
}

int yvex_cuda_test_attention_softmax(void)
{
    const unsigned long long tokens[] = {257ull, 1025ull, 5757ull, 21741ull};
    yvex_backend_options options = {.kind = YVEX_BACKEND_KIND_CUDA};
    yvex_backend *backend = NULL;
    yvex_error err;
    int rc = yvex_backend_open(&backend, &options, &err);
    if (rc == YVEX_ERR_UNSUPPORTED) return 77;
    YVEX_TEST_ASSERT(rc == YVEX_OK, "open softmax CUDA backend");
    for (size_t i = 0u; i < sizeof(tokens) / sizeof(tokens[0]); ++i)
        for (unsigned int scenario = 0u; scenario < 6u; ++scenario)
            if (softmax_case(backend, tokens[i], scenario)) return 1;
    yvex_backend_close(backend);
    return 0;
}
