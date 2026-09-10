/* Independent dense softmax oracle for native reduction, including register/streaming tails. */
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yvex/internal/backend.h>
#include "src/backend/cuda/private.h"
#include "tests/test.h"

enum { REDUCTION_WIDTH = 1280, REDUCTION_ROWS = 512, REDUCTION_HEADS = 3,
       REDUCTION_TOKENS = 3, REDUCTION_VALUES = REDUCTION_WIDTH * REDUCTION_HEADS * REDUCTION_TOKENS };
typedef struct {
    float query[REDUCTION_VALUES];
    float local[REDUCTION_ROWS * REDUCTION_WIDTH];
    float compressed[REDUCTION_ROWS * REDUCTION_WIDTH];
    float sinks[REDUCTION_HEADS];
    unsigned long long positions[REDUCTION_ROWS], compressed_positions[REDUCTION_ROWS];
    unsigned long long selected[REDUCTION_TOKENS * REDUCTION_ROWS], counts[REDUCTION_TOKENS];
    float before_canary, output[REDUCTION_VALUES], after_canary;
    int status;
} reduction_storage;

typedef struct {
    unsigned long long width, heads, tokens, initial, stride, topk, window, ratio, start;
    unsigned int attention_class, scenario;
    int block_visible;
} reduction_case;

static float reduction_bf16(float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    bits += 0x7fffu + ((bits >> 16u) & 1u);
    bits &= 0xffff0000u;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

/* Full materialized softmax in scalar F64, not the device's online recurrence
 * or its lane reduction. Input/source rows are selected independently here. */
static double reduction_oracle(const reduction_storage *s, const reduction_case *c,
                               unsigned long long token, unsigned long long head,
                               double expected[REDUCTION_WIDTH])
{
    const float *rows[REDUCTION_ROWS * 2];
    double scores[REDUCTION_ROWS * 2], maximum = s->sinks[head], denominator;
    unsigned long long count = 0ull, position = c->start + token;
    unsigned long long first = position + 1ull > c->window ? position + 1ull - c->window : 0ull;
    for (unsigned long long i = 0ull; i < c->initial + c->tokens; ++i) {
        if (!c->block_visible && (s->positions[i] < first || s->positions[i] > position)) continue;
        rows[count++] = s->local + i * REDUCTION_WIDTH;
    }
    if (c->attention_class) {
        unsigned long long n = c->attention_class == 1u ? s->counts[token]
            : position / c->ratio + ((position + 1ull) % c->ratio == 0ull);
        for (unsigned long long j = 0ull; j < n; ++j) {
            unsigned long long i = c->attention_class == 1u ? s->selected[token * REDUCTION_ROWS + j] : j;
            if (s->compressed_positions[i] + c->ratio - 1ull > position) continue;
            rows[count++] = s->compressed + i * REDUCTION_WIDTH;
        }
    }
    for (unsigned long long row = 0ull; row < count; ++row) {
        double dot = 0.0;
        for (unsigned long long i = 0ull; i < c->width; ++i)
            dot += (double)s->query[(token * c->heads + head) * c->width + i] * rows[row][i];
        scores[row] = dot / sqrt((double)c->width);
        if (scores[row] > maximum) maximum = scores[row];
    }
    denominator = exp((double)s->sinks[head] - maximum);
    memset(expected, 0, REDUCTION_WIDTH * sizeof(*expected));
    for (unsigned long long row = 0ull; row < count; ++row) {
        double weight = exp(scores[row] - maximum);
        denominator += weight;
        for (unsigned long long i = 0ull; i < c->width; ++i) expected[i] += weight * rows[row][i];
    }
    for (unsigned long long i = 0ull; i < c->width; ++i) expected[i] /= denominator;
    return denominator;
}

static void reduction_inputs(reduction_storage *s, reduction_case *c)
{
    int analytic = c->scenario == 0u;
    c->heads = REDUCTION_HEADS; c->tokens = REDUCTION_TOKENS;
    c->initial = analytic ? 4ull : 128ull;
    c->stride = REDUCTION_WIDTH; c->topk = REDUCTION_ROWS;
    c->window = 128ull; c->start = 8192ull;
    c->ratio = c->attention_class == 1u ? 4ull : c->attention_class == 2u ? 128ull : 0ull;
    c->block_visible = c->scenario != 2u;
    s->before_canary = s->after_canary = 12345.0f;
    for (unsigned long long i = 0ull; i < REDUCTION_VALUES; ++i) {
        s->query[i] = analytic ? 0.0f : (float)((int)((i * 19ull + 3ull) % 127ull) - 63) / 128.0f;
        s->output[i] = 12345.0f;
    }
    for (unsigned long long row = 0ull; row < REDUCTION_ROWS; ++row) {
        s->positions[row] = c->start - c->initial + row;
        s->compressed_positions[row] = row * c->ratio;
        for (unsigned long long i = 0ull; i < REDUCTION_WIDTH; ++i) {
            float value = (float)((int)(i % 15ull) - 7) / 8.0f;
            s->local[row * REDUCTION_WIDTH + i] = analytic ? value
                : (float)((int)((row * 17ull + i * 7ull) % 251ull) - 125) / 64.0f;
            s->compressed[row * REDUCTION_WIDTH + i] = analytic ? value
                : (float)((int)((row * 31ull + i * 5ull) % 241ull) - 120) / 64.0f;
        }
    }
    for (unsigned long long token = 0ull; token < c->tokens; ++token) {
        s->counts[token] = analytic ? 8ull : REDUCTION_ROWS;
        for (unsigned long long i = 0ull; i < REDUCTION_ROWS; ++i)
            s->selected[token * REDUCTION_ROWS + i] = s->counts[token] > i ? s->counts[token] - 1ull - i : i;
    }
    for (unsigned long long head = 0ull; head < c->heads; ++head)
        s->sinks[head] = analytic ? 0.0f : (float)((int)head - 1) / 4.0f;
    if (c->scenario == 3u) s->query[0] = NAN;
    if (c->scenario == 4u) s->status = 1;
    if (c->scenario == 5u) c->stride = c->width - 1ull;
    if (c->scenario == 6u) c->ratio = 7ull;
    if (c->scenario == 7u) s->sinks[0] = NAN;
}

static int reduction_check(yvex_backend *backend, unsigned long long width,
                           unsigned int attention_class, unsigned int scenario)
{
    reduction_storage *host = calloc(1u, sizeof(*host)), *observed = calloc(1u, sizeof(*observed));
    reduction_case c = {.width = width, .attention_class = attention_class, .scenario = scenario};
    yvex_backend_tensor_desc descriptor = {0};
    yvex_device_tensor *arena = NULL;
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    yvex_error err;
    CUdeviceptr base, query, local, positions, compressed, compressed_positions, selected, counts, sinks, output, status;
    double maximum_error = 0.0, worst_ratio = 0.0;
    int rc, device_wide = 0;
    YVEX_TEST_ASSERT(host && observed, "allocate native reduction oracle");
    reduction_inputs(host, &c);
    descriptor.name = "attention-reduction"; descriptor.dtype = YVEX_DTYPE_I8;
    descriptor.rank = 1u; descriptor.dims[0] = sizeof(*host); descriptor.bytes = sizeof(*host);
    YVEX_TEST_ASSERT(yvex_backend_tensor_alloc(backend, &descriptor, &arena, &err) == YVEX_OK &&
        yvex_backend_tensor_write(backend, arena, host, sizeof(*host), &err) == YVEX_OK, "upload reduction fixture");
    base = yvex_cuda_activation_pointer(backend, arena);
    query = base + offsetof(reduction_storage, query); local = base + offsetof(reduction_storage, local);
    positions = base + offsetof(reduction_storage, positions); compressed = base + offsetof(reduction_storage, compressed);
    compressed_positions = base + offsetof(reduction_storage, compressed_positions);
    selected = base + offsetof(reduction_storage, selected); counts = base + offsetof(reduction_storage, counts);
    sinks = base + offsetof(reduction_storage, sinks); output = base + offsetof(reduction_storage, output);
    status = base + offsetof(reduction_storage, status);
    void *params[] = {&query, &local, &positions, &c.initial, &c.stride, &compressed, &compressed_positions,
        &c.stride, &selected, &counts, &c.topk, &sinks, &c.heads, &c.width, &c.window, &c.ratio,
        &c.attention_class, &c.start, &c.tokens, &c.block_visible, &output, &status};
    rc = yvex_cuda_launch(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
        state->attention_reduce_native_function, (unsigned int)(c.heads * c.tokens), 256u, 0u,
        params, "cuda.test.attention-reduction", &err);
    if (rc == YVEX_OK) rc = yvex_cuda_launch_synchronize(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
        &device_wide, "cuda.test.attention-reduction", &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK && yvex_backend_tensor_read(
        backend, arena, observed, sizeof(*observed), &err) == YVEX_OK, "read native reduction");
    if (scenario >= 3u) {
        YVEX_TEST_ASSERT(observed->status != 0, "nonfinite/prior error/invalid geometry refuses publication");
    } else {
        YVEX_TEST_ASSERT(observed->status == 0, "valid reduction succeeds");
        for (unsigned long long token = 0ull; token < c.tokens; ++token)
            for (unsigned long long head = 0ull; head < c.heads; ++head) {
                double expected[REDUCTION_WIDTH];
                YVEX_TEST_ASSERT(isfinite(reduction_oracle(host, &c, token, head, expected)), "finite softmax oracle");
                for (unsigned long long i = 0ull; i < width; ++i) {
                    float value = observed->output[(token * c.heads + head) * width + i];
                    double difference = fabs((double)value - expected[i]);
                    /* Half of a BF16 relative spacing, plus bounded F32 arithmetic error.
                     * Analytic equal scores additionally require exact BF16 identity. */
                    double tolerance = fabs(expected[i]) / 256.0 + 2e-5;
                    if (difference > maximum_error) maximum_error = difference;
                    if (difference / tolerance > worst_ratio) worst_ratio = difference / tolerance;
                    YVEX_TEST_ASSERT(isfinite(value) && difference <= tolerance, "native output agrees with independent dense F64 softmax");
                    if (scenario == 0u) YVEX_TEST_ASSERT(value == reduction_bf16((float)expected[i]), "analytic BF16 output is exact");
                }
            }
    }
    YVEX_TEST_ASSERT(!memcmp(host, observed, offsetof(reduction_storage, before_canary)),
        "reduction does not modify query, sinks, state, positions or selected candidates");
    YVEX_TEST_ASSERT(observed->before_canary == 12345.0f && observed->after_canary == 12345.0f,
        "output allocation canaries retained");
    for (unsigned long long i = c.heads * c.tokens * width; i < REDUCTION_VALUES; ++i)
        YVEX_TEST_ASSERT(observed->output[i] == 12345.0f, "output stays within admitted shape");
    printf("attention reduction: class=%u width=%llu scenario=%u values=%llu status=%d max_abs=%.12g "
           "worst_error_over_tolerance=%.9g analytic_exact=%s\n", attention_class, width, scenario,
           c.heads * c.tokens * width, observed->status, maximum_error, worst_ratio, scenario ? "n/a" : "true");
    YVEX_TEST_ASSERT(yvex_backend_tensor_release(backend, &arena, &err) == YVEX_OK, "release reduction fixture");
    free(host); free(observed);
    return 0;
}

int yvex_cuda_test_attention_reduction(void)
{
    const unsigned long long widths[] = {1ull, 127ull, 128ull, 255ull, 256ull, 257ull,
        511ull, 512ull, 768ull, 1023ull, 1024ull, 1025ull, 1280ull};
    yvex_backend_options options = {.kind = YVEX_BACKEND_KIND_CUDA};
    yvex_backend *backend = NULL;
    yvex_error err;
    int rc = yvex_backend_open(&backend, &options, &err);
    if (rc == YVEX_ERR_UNSUPPORTED) return 77;
    YVEX_TEST_ASSERT(rc == YVEX_OK, "open reduction CUDA backend");
    for (size_t i = 0u; i < sizeof(widths) / sizeof(widths[0]); ++i)
        for (unsigned int attention_class = 0u; attention_class < 3u; ++attention_class)
            for (unsigned int scenario = 0u; scenario < 3u; ++scenario)
                if (reduction_check(backend, widths[i], attention_class, scenario)) return 1;
    for (unsigned int scenario = 3u; scenario <= 7u; ++scenario)
        if (reduction_check(backend, 512ull, 1u, scenario)) return 1;
    yvex_backend_close(backend);
    return 0;
}
