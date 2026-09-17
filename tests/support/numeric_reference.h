/* Retained pre-cutover arithmetic for cross-implementation preservation tests.
 * This is not an upstream numerical oracle or production computational owner. */
#ifndef TESTS_SUPPORT_NUMERIC_REFERENCE_H
#define TESTS_SUPPORT_NUMERIC_REFERENCE_H
#include <yvex/internal/core.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
static inline int test_numeric_refuse(yvex_error *err, yvex_status status, const char *reason)
{
    yvex_error_set(err, status, "test.numeric.reference", reason);
    return status;
}

/* Project source-layout [out, in] F32 weights without inventing a physical transpose. */
static inline int test_graph_linear_source_f32(
    const float *input, unsigned long long input_count, unsigned long long rows,
    unsigned long long input_width, const float *weight,
    unsigned long long weight_count, const float *bias,
    unsigned long long bias_count, unsigned long long output_width,
    float *output, unsigned long long output_count, yvex_error *err)
{
    unsigned long long expected_input, expected_weight, expected_output;
    unsigned long long row, column;

    if (!input || !rows || !input_width || !weight || !output || !output_width ||
        !yvex_core_u64_mul(rows, input_width, &expected_input) ||
        !yvex_core_u64_mul(output_width, input_width, &expected_weight) ||
        !yvex_core_u64_mul(rows, output_width, &expected_output) ||
        expected_input > (unsigned long long)SIZE_MAX / sizeof(float) ||
        expected_weight > (unsigned long long)SIZE_MAX / sizeof(float) ||
        expected_output > (unsigned long long)SIZE_MAX / sizeof(float) ||
        expected_input != input_count || expected_weight != weight_count ||
        expected_output != output_count || (bias && bias_count != output_width) ||
        (!bias && bias_count))
        return test_numeric_refuse(err, YVEX_ERR_BOUNDS,
                                    "linear F32 extents differ from source geometry");
    for (row = 0ull; row < rows; ++row) {
        const float *input_row = input + row * input_width;
        for (column = 0ull; column < output_width; ++column) {
            const float *weight_row = weight + column * input_width;
            float sum = bias ? bias[column] : 0.0f;
            unsigned long long inner;

            for (inner = 0ull; inner < input_width; ++inner)
                sum += input_row[inner] * weight_row[inner];
            if (!isfinite(sum))
                return test_numeric_refuse(err, YVEX_ERR_FORMAT,
                                            "linear F32 produced a non-finite value");
            output[row * output_width + column] = sum;
        }
    }
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Apply the source LayerNorm contract in place over independently normalized rows. */
static inline int test_graph_layer_norm_f32(float *values, unsigned long long rows,
                              unsigned long long width, const float *weight,
                              const float *bias, double epsilon, yvex_error *err)
{
    unsigned long long row;

    if (!values || !rows || !width || !weight || !bias || !isfinite(epsilon) ||
        epsilon <= 0.0 || rows > ULLONG_MAX / width ||
        rows * width > (unsigned long long)SIZE_MAX / sizeof(float))
        return test_numeric_refuse(err, YVEX_ERR_INVALID_ARG,
                                    "LayerNorm requires bounded rows, affine values, and epsilon");
    for (row = 0ull; row < rows; ++row) {
        float *current = values + row * width;
        double mean = 0.0, variance = 0.0, inverse;
        unsigned long long index;

        for (index = 0ull; index < width; ++index) {
            if (!isfinite(current[index]) || !isfinite(weight[index]) ||
                !isfinite(bias[index]))
                return test_numeric_refuse(err, YVEX_ERR_FORMAT,
                                            "LayerNorm input or affine value is non-finite");
            mean += current[index];
        }
        mean /= (double)width;
        for (index = 0ull; index < width; ++index) {
            double centered = (double)current[index] - mean;
            variance += centered * centered;
        }
        inverse = 1.0 / sqrt(variance / (double)width + epsilon);
        if (!isfinite(inverse))
            return test_numeric_refuse(err, YVEX_ERR_FORMAT,
                                        "LayerNorm variance is not finite");
        for (index = 0ull; index < width; ++index) {
            double normalized = ((double)current[index] - mean) * inverse;
            double result = normalized * weight[index] + bias[index];
            if (!isfinite(result))
                return test_numeric_refuse(err, YVEX_ERR_FORMAT,
                                            "LayerNorm produced a non-finite value");
            current[index] = (float)result;
        }
    }
    yvex_error_clear(err);
    return YVEX_OK;
}

static inline int test_graph_silu_gate_f32(const float *fused, unsigned long long rows,
                             unsigned long long width, float *output,
                             yvex_error *err)
{
    unsigned long long row, index;

    if (!fused || !rows || !width || !output || width > ULLONG_MAX / 2ull ||
        rows > ULLONG_MAX / (width * 2ull) ||
        rows * width > (unsigned long long)SIZE_MAX / sizeof(float))
        return test_numeric_refuse(err, YVEX_ERR_INVALID_ARG,
                                    "gated SiLU requires bounded fused rows and output");
    for (row = 0ull; row < rows; ++row) {
        const float *gate = fused + row * width * 2ull;
        const float *value = gate + width;
        for (index = 0ull; index < width; ++index) {
            float result = gate[index] / (1.0f + expf(-gate[index])) * value[index];
            if (!isfinite(result))
                return test_numeric_refuse(err, YVEX_ERR_FORMAT,
                                            "gated SiLU produced a non-finite value");
            output[row * width + index] = result;
        }
    }
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Execute full noncausal attention over interleaved [head, Q/K/V, width] rows. */
static inline int test_graph_full_attention_f32(
    const float *qkv, unsigned long long rows, unsigned long long heads,
    unsigned long long head_width, float *output, float *scratch,
    unsigned long long scratch_count, yvex_error *err)
{
    unsigned long long hidden, qkv_width, output_elements, qkv_elements;
    unsigned long long query, head;
    float scale;

    if (!qkv || !rows || !heads || !head_width || !output || !scratch ||
        !yvex_core_u64_mul(heads, head_width, &hidden) ||
        !yvex_core_u64_mul(hidden, 3ull, &qkv_width) ||
        !yvex_core_u64_mul(rows, hidden, &output_elements) ||
        !yvex_core_u64_mul(rows, qkv_width, &qkv_elements) ||
        output_elements > (unsigned long long)SIZE_MAX / sizeof(float) ||
        qkv_elements > (unsigned long long)SIZE_MAX / sizeof(float) ||
        scratch_count < rows)
        return test_numeric_refuse(err, YVEX_ERR_INVALID_ARG,
                                    "full attention requires bounded QKV geometry and scratch");
    scale = 1.0f / sqrtf((float)head_width);
    memset(output, 0, (size_t)output_elements * sizeof(*output));
    for (query = 0ull; query < rows; ++query) {
        for (head = 0ull; head < heads; ++head) {
            const float *q = qkv + query * qkv_width + head * head_width * 3ull;
            float maximum = -INFINITY, sum = 0.0f;
            unsigned long long key, coordinate;

            for (key = 0ull; key < rows; ++key) {
                const float *k = qkv + key * qkv_width +
                                 head * head_width * 3ull + head_width;
                float score = 0.0f;
                for (coordinate = 0ull; coordinate < head_width; ++coordinate)
                    score += q[coordinate] * k[coordinate];
                scratch[key] = score * scale;
                if (scratch[key] > maximum) maximum = scratch[key];
            }
            for (key = 0ull; key < rows; ++key) {
                scratch[key] = expf(scratch[key] - maximum);
                sum += scratch[key];
            }
            if (!isfinite(sum) || sum <= 0.0f)
                return test_numeric_refuse(err, YVEX_ERR_FORMAT,
                                            "full attention softmax is not finite");
            for (key = 0ull; key < rows; ++key) {
                const float *value = qkv + key * qkv_width +
                                     head * head_width * 3ull + head_width * 2ull;
                float probability = scratch[key] / sum;
                float *destination = output + query * hidden + head * head_width;
                for (coordinate = 0ull; coordinate < head_width; ++coordinate)
                    destination[coordinate] += probability * value[coordinate];
            }
        }
    }
    yvex_error_clear(err);
    return YVEX_OK;
}
#endif
