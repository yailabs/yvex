/*
 * Provide the independently compiled CPU implementation selected by the generic backend vtable.
 *
 * Every tensor is tied to one backend instance; checked memory accounting precedes allocation;
 * failed operations preserve output state. These primitives prove bounded CPU execution, not
 * transformer or generation support.
 */

#include <yvex/internal/backend.h>
#include "src/backend/private.h"
#include <yvex/internal/quant_numeric.h>
#include <yvex/internal/neural_operations.h>

#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int cpu_tensor_alloc(yvex_backend *, const yvex_backend_tensor_desc *,
                                 yvex_device_tensor **, yvex_error *);
static int cpu_tensor_free(yvex_backend *, yvex_device_tensor *, yvex_error *);
static int cpu_tensor_write(yvex_backend *, yvex_device_tensor *, const void *,
                                 unsigned long long, yvex_error *);
static int cpu_tensor_read(yvex_backend *, const yvex_device_tensor *, void *,
                                unsigned long long, yvex_error *);
static int cpu_tensor_zero(yvex_backend *, yvex_device_tensor *, yvex_error *);
static int cpu_tensor_copy(yvex_backend *, yvex_device_tensor *,
                                const yvex_device_tensor *, yvex_error *);
static int cpu_op_embed(yvex_backend *, const yvex_device_tensor *, const unsigned int *,
                             unsigned long long, yvex_device_tensor *, yvex_error *);
static int cpu_op_rms_norm(yvex_backend *, const yvex_device_tensor *,
                                const yvex_device_tensor *, float, yvex_device_tensor *,
                                yvex_error *);
static int cpu_op_rope(yvex_backend *, const yvex_device_tensor *, unsigned long long,
                            float, yvex_device_tensor *, yvex_error *);
static int cpu_op_matmul(yvex_backend *, const yvex_device_tensor *,
                              const yvex_device_tensor *, yvex_device_tensor *, yvex_error *);
static int cpu_op_mlp(yvex_backend *, const yvex_device_tensor *,
                           const yvex_device_tensor *, const yvex_device_tensor *,
                           const yvex_device_tensor *, const yvex_mlp_options *,
                           yvex_device_tensor *, yvex_device_tensor *, yvex_error *);
static int cpu_op_attention(yvex_backend *, const yvex_device_tensor *,
                                 const yvex_device_tensor *, const yvex_device_tensor *,
                                 unsigned long long, unsigned long long, float, int,
                                 yvex_device_tensor *, yvex_device_tensor *,
                                 yvex_device_tensor *, yvex_error *);

static int cpu_memory_stats(const yvex_backend *backend,
                            yvex_backend_memory_stats *out,
                            yvex_error *err)
{
    *out = backend->stats;
    yvex_error_clear(err);
    return YVEX_OK;
}

static int cpu_device_info(const yvex_backend *backend,
                           yvex_backend_device_info *out,
                           yvex_error *err)
{
    *out = backend->device_info;
    yvex_error_clear(err);
    return YVEX_OK;
}

static int cpu_sync(yvex_backend *backend, yvex_error *err)
{
    (void)backend;
    yvex_error_clear(err);
    return YVEX_OK;
}

static int cpu_query_capability(const yvex_backend *backend,
                                yvex_backend_operation_variant variant,
                                yvex_backend_capability_result *out,
                                yvex_error *err)
{
    if (!backend || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "cpu.query_capability",
                       "backend and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (variant < 0 || variant >= YVEX_BACKEND_VARIANT_COUNT) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "cpu.query_capability",
                       "operation variant is out of range");
        return YVEX_ERR_INVALID_ARG;
    }
    out->state = YVEX_BACKEND_CAPABILITY_SUPPORTED;
    out->reason = YVEX_BACKEND_CAPABILITY_REASON_NONE;
    out->context_available = 1;
    out->function_available = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}

/* One admitted numerical operation, not a model/decoder plan. Stream geometry
 * and both rounding points are supplied by the verified computational program. */
static int cpu_mhc_head(yvex_backend *backend, const yvex_device_tensor *expanded,
    const yvex_device_tensor *function, const yvex_device_tensor *base,
    const yvex_device_tensor *scale, const yvex_device_tensor *norm,
    unsigned long long rows, unsigned long long width, unsigned long long streams,
    double epsilon, double mhc_epsilon, yvex_device_tensor *pre, yvex_device_tensor *output,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    const yvex_device_tensor *tensors[] = {expanded, function, base, scale, norm, pre, output};
    unsigned long long sizes[7], expanded_width, row, stream, lane, i;
    const float *fn, *bias, *gain, *weights;
    if (facts) memset(facts, 0, sizeof(*facts));
    if (output) output->is_written = 0;
    if (pre) pre->is_written = 0;
    if (!facts || !rows || !width || !streams || !isfinite(epsilon) || epsilon <= 0.0 ||
        !isfinite(mhc_epsilon) || mhc_epsilon <= 0.0 ||
        !yvex_core_u64_mul(width, streams, &expanded_width) ||
        !yvex_core_u64_mul(rows, expanded_width, &sizes[0]) ||
        !yvex_core_u64_mul(streams, expanded_width, &sizes[1]) ||
        !yvex_core_u64_mul(rows, width, &sizes[5])) goto invalid;
    sizes[2] = streams; sizes[3] = 1u; sizes[4] = width; sizes[6] = sizes[5];
    for (i = 0u; i < 7u; ++i) {
        if (i == 5u && !pre) continue;
        if (!backend_tensor_owner_is(backend, tensors[i]) || !backend_tensor_f32_elements(tensors[i], sizes[i]) ||
            (i < 5u && !tensors[i]->is_written)) goto invalid;
    }
    if (pre && pre->data == output->data) goto invalid;
    fn = (const float *)function->data; bias = (const float *)base->data;
    gain = (const float *)scale->data; weights = (const float *)norm->data;
    for (row = 0u; row < rows; ++row) {
        const float *x = (const float *)expanded->data + row * expanded_width;
        float *y = (float *)output->data + row * width;
        double squares = 0.0, inverse;
        for (i = 0u; i < expanded_width; ++i) squares += (double)x[i] * (double)x[i];
        inverse = 1.0 / sqrt(squares / (double)expanded_width + epsilon);
        if (!isfinite(inverse)) goto numeric;
        memset(y, 0, (size_t)width * sizeof(float));
        for (stream = 0u; stream < streams; ++stream) {
            double mix = 0.0, coefficient;
            for (i = 0u; i < expanded_width; ++i) mix += (double)fn[stream * expanded_width + i] * (double)x[i];
            coefficient = 1.0 / (1.0 + exp(-(mix * inverse * (double)gain[0] + (double)bias[stream])));
            coefficient += mhc_epsilon;
            for (lane = 0u; lane < width; ++lane)
                y[lane] += (float)(coefficient * (double)x[stream * width + lane]);
        }
        squares = 0.0;
        for (lane = 0u; lane < width; ++lane) {
            if (!isfinite(y[lane]) || !isfinite(weights[lane])) goto numeric;
            y[lane] = yvex_quant_bf16_decode(yvex_quant_bf16_encode(y[lane]));
            squares += (double)y[lane] * (double)y[lane];
        }
        if (pre) memcpy((float *)pre->data + row * width, y, (size_t)width * sizeof(float));
        inverse = 1.0 / sqrt(squares / (double)width + epsilon);
        for (lane = 0u; lane < width; ++lane) {
            double value = (double)y[lane] * inverse * (double)weights[lane];
            if (!isfinite(value) || !isfinite((float)value)) goto numeric;
            y[lane] = yvex_quant_bf16_decode(yvex_quant_bf16_encode((float)value));
        }
    }
    output->is_written = 1;
    if (pre) pre->is_written = 1;
    facts->active_weight_bytes = function->bytes + base->bytes + scale->bytes + norm->bytes;
    facts->activation_bytes = expanded->bytes + output->bytes + (pre ? pre->bytes : 0u);
    facts->compulsory_memory_facts_available = 1;
    yvex_error_clear(err);
    return YVEX_OK;
invalid:
    yvex_error_set(err, YVEX_ERR_FORMAT, "cpu.mhc-head", "mHC head tensors or geometry are incompatible");
    return YVEX_ERR_FORMAT;
numeric:
    yvex_error_set(err, YVEX_ERR_FORMAT, "cpu.mhc-head", "mHC head produced non-finite values");
    return YVEX_ERR_FORMAT;
}

static int cpu_stream_mean(yvex_backend *backend, const yvex_device_tensor *input,
    unsigned long long rows, unsigned long long width, unsigned long long streams,
    yvex_device_tensor *output, yvex_device_tensor *resident, unsigned long long row_offset,
    unsigned long long row_stride, unsigned long long column_offset, float *host_output,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    unsigned long long count, input_count, row, lane, stream;
    if (facts) memset(facts, 0, sizeof(*facts));
    if (output) output->is_written = 0;
    if (!facts || !rows || !width || !streams || resident || row_offset || row_stride || column_offset ||
        host_output || !yvex_core_u64_mul(rows, width, &count) ||
        !yvex_core_u64_mul(count, streams, &input_count) || !backend_tensor_owner_is(backend, input) ||
        !backend_tensor_f32_elements(input, input_count) || !input->is_written ||
        !backend_tensor_owner_is(backend, output) || !backend_tensor_f32_elements(output, count) ||
        input->data == output->data) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "cpu.stream-mean", "incompatible stream reduction operands");
        return YVEX_ERR_FORMAT;
    }
    for (row = 0u; row < rows; ++row)
        for (lane = 0u; lane < width; ++lane) {
            double sum = 0.0;
            for (stream = 0u; stream < streams; ++stream)
                sum += ((const float *)input->data)[(row * streams + stream) * width + lane];
            float value = (float)(sum / (double)streams);
            if (!isfinite(value)) {
                yvex_error_set(err, YVEX_ERR_FORMAT, "cpu.stream-mean", "stream mean produced a non-finite value");
                return YVEX_ERR_FORMAT;
            }
            ((float *)output->data)[row * width + lane] = value;
        }
    output->is_written = 1;
    facts->activation_bytes = input->bytes + output->bytes;
    facts->compulsory_memory_facts_available = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}

static int cpu_residual_post(yvex_backend *backend, const yvex_device_tensor *residual,
    const yvex_device_tensor *core, const yvex_device_tensor *post, const yvex_device_tensor *mix,
    unsigned long long rows, unsigned long long streams, unsigned long long width,
    yvex_device_tensor *output, yvex_backend_operation_facts *facts, yvex_error *err)
{
    const yvex_device_tensor *inputs[] = {residual, core, post, mix};
    unsigned long long sizes[4], total, row, target, lane, source;
    if (facts) memset(facts, 0, sizeof(*facts));
    if (output) output->is_written = 0;
    if (!facts || !rows || !streams || !width ||
        !yvex_core_u64_mul(rows, width, &sizes[1]) || !yvex_core_u64_mul(rows, streams, &sizes[2]) ||
        !yvex_core_u64_mul(sizes[1], streams, &sizes[0]) || !yvex_core_u64_mul(sizes[2], streams, &sizes[3]) ||
        !backend_tensor_owner_is(backend, output) || !backend_tensor_f32_elements(output, sizes[0])) goto invalid;
    total = sizes[0];
    for (size_t i = 0u; i < 4u; ++i)
        if (!backend_tensor_owner_is(backend, inputs[i]) || !backend_tensor_f32_elements(inputs[i], sizes[i]) ||
            !inputs[i]->is_written || inputs[i]->data == output->data ||
            !yvex_core_u64_add(total, sizes[i], &total)) goto invalid;
    if (!yvex_core_u64_mul(total, sizeof(float), &total)) goto invalid;
    for (row = 0u; row < rows; ++row)
        for (target = 0u; target < streams; ++target)
            for (lane = 0u; lane < width; ++lane) {
                double value = (double)((const float *)post->data)[row * streams + target] *
                    ((const float *)core->data)[row * width + lane];
                for (source = 0u; source < streams; ++source)
                    value += (double)((const float *)mix->data)[(row * streams + source) * streams + target] *
                        ((const float *)residual->data)[(row * streams + source) * width + lane];
                if (!isfinite((float)value)) goto invalid;
                ((float *)output->data)[(row * streams + target) * width + lane] =
                    yvex_quant_bf16_decode(yvex_quant_bf16_encode((float)value));
            }
    output->is_written = 1;
    facts->activation_bytes = total;
    facts->compulsory_memory_facts_available = 1;
    yvex_error_clear(err);
    return YVEX_OK;
invalid:
    yvex_error_set(err, YVEX_ERR_FORMAT, "cpu.mhc-post", "invalid mHC post operands or non-finite result");
    return YVEX_ERR_FORMAT;
}

static int cpu_storage_disjoint(const yvex_device_tensor *a, const yvex_device_tensor *b)
{
    uintptr_t start = (uintptr_t)a->data, other = (uintptr_t)b->data;
    return a->bytes <= UINTPTR_MAX - start && b->bytes <= UINTPTR_MAX - other &&
        (start + a->bytes <= other || other + b->bytes <= start);
}

/* Checked numerical operands, independent of model topology and source names. */
static int cpu_neural_admit(yvex_backend *backend, const yvex_device_tensor *const *inputs,
    const unsigned long long *input_sizes, size_t input_count, yvex_device_tensor *const *outputs,
    const unsigned long long *output_sizes, size_t output_count,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    if (!facts) goto invalid;
    memset(facts, 0, sizeof(*facts));
    for (size_t i = 0u; i < input_count; ++i)
        if (!backend_tensor_owner_is(backend, inputs[i]) || !inputs[i]->is_written ||
            !input_sizes[i] || !backend_tensor_f32_elements(inputs[i], input_sizes[i]) ||
            !yvex_core_u64_add(facts->activation_bytes, inputs[i]->bytes, &facts->activation_bytes)) goto invalid;
    for (size_t i = 0u; i < output_count; ++i) {
        if (!backend_tensor_owner_is(backend, outputs[i]) || !output_sizes[i] ||
            !backend_tensor_f32_elements(outputs[i], output_sizes[i]) || outputs[i]->borrowed_host ||
            !yvex_core_u64_add(facts->activation_bytes, outputs[i]->bytes, &facts->activation_bytes)) goto invalid;
        uintptr_t start = (uintptr_t)outputs[i]->data;
        if (outputs[i]->bytes > UINTPTR_MAX - start) goto invalid;
        for (size_t j = 0u; j < input_count + i; ++j) {
            const yvex_device_tensor *other = j < input_count ? inputs[j] : outputs[j - input_count];
            if (!cpu_storage_disjoint(outputs[i], other)) goto invalid;
        }
    }
    for (size_t i = 0u; i < output_count; ++i) outputs[i]->is_written = 0;
    facts->compulsory_memory_facts_available = 1;
    return YVEX_OK;
invalid:
    yvex_error_set(err, YVEX_ERR_FORMAT, "cpu.neural", "incompatible numerical operands, population or alias");
    return YVEX_ERR_FORMAT;
}

static int cpu_neural_publish(yvex_device_tensor *const *outputs, size_t count, yvex_error *err)
{
    for (size_t i = 0u; i < count; ++i)
        for (unsigned long long j = 0u; j < outputs[i]->bytes / sizeof(float); ++j)
            if (!isfinite(((const float *)outputs[i]->data)[j])) {
                yvex_error_set(err, YVEX_ERR_FORMAT, "cpu.neural", "non-finite numerical result is unpublished");
                return YVEX_ERR_FORMAT;
            }
    for (size_t i = 0u; i < count; ++i) outputs[i]->is_written = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}

static int cpu_neural_bounds(yvex_error *err)
{
    yvex_error_set(err, YVEX_ERR_BOUNDS, "cpu.neural", "numerical population or parameter extent overflowed");
    return YVEX_ERR_BOUNDS;
}

static int cpu_sinusoidal_embedding(yvex_backend *backend, const yvex_device_tensor *input,
    yvex_device_tensor *output, unsigned long long rows, unsigned long long half,
    float period, yvex_backend_operation_facts *facts, yvex_error *err)
{
    unsigned long long count;
    if (!rows || !half || !isfinite(period) || period <= 1.0f ||
        !yvex_core_u64_mul(rows, half, &count) || !yvex_core_u64_mul(count, 2u, &count))
        return cpu_neural_bounds(err);
    int rc = cpu_neural_admit(backend, &input, &rows, 1u, &output, &count, 1u, facts, err);
    if (rc != YVEX_OK) return rc;
    const float *x = (const float *)input->data;
    float *y = (float *)output->data;
    for (unsigned long long row = 0u; row < rows; ++row)
        for (unsigned long long lane = 0u; lane < half; ++lane) {
            float exponent = -logf(period) * (float)lane / (float)half;
            float angle = x[row] * expf(exponent);
            y[row * half * 2u + lane] = cosf(angle);
            y[row * half * 2u + half + lane] = sinf(angle);
        }
    return cpu_neural_publish(&output, 1u, err);
}

static int cpu_linear_bias_f32(yvex_backend *backend, const unsigned char *encoded,
    unsigned long long bytes, const yvex_device_tensor *bias, unsigned long long rows,
    unsigned long long input_width, unsigned long long output_width, const yvex_device_tensor *input,
    yvex_device_tensor *output, yvex_backend_operation_facts *facts, yvex_error *err)
{
    unsigned long long sizes[2], result, expected;
    if (!encoded || !rows || !input_width || !output_width ||
        !yvex_core_u64_mul(rows, input_width, sizes) || !yvex_core_u64_mul(rows, output_width, &result) ||
        !yvex_core_u64_mul(input_width, output_width, &expected) ||
        !yvex_core_u64_mul(expected, sizeof(float), &expected) || expected != bytes || bytes > SIZE_MAX) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "cpu.linear-bias", "bounded exact F32 weight geometry required");
        return YVEX_ERR_FORMAT;
    }
    sizes[1] = output_width;
    int rc = cpu_neural_admit(backend, (const yvex_device_tensor *[]){input, bias}, sizes, 2u,
        &output, &result, 1u, facts, err);
    if (rc != YVEX_OK) return rc;
    const float *x = (const float *)input->data, *w = (const float *)encoded, *b = (const float *)bias->data;
    float *y = (float *)output->data;
    /* Source CPU contract: bias initializes the sequential F32 accumulator. */
    for (unsigned long long row = 0u; row < rows; ++row)
        for (unsigned long long column = 0u; column < output_width; ++column) {
            float sum = b[column];
            for (unsigned long long k = 0u; k < input_width; ++k)
                sum += x[row * input_width + k] * w[column * input_width + k];
            y[row * output_width + column] = sum;
        }
    facts->active_weight_bytes = bytes + bias->bytes;
    return cpu_neural_publish(&output, 1u, err);
}

static int cpu_normalization_f32(yvex_backend *backend, const yvex_device_tensor *input,
    const yvex_device_tensor *weight, const yvex_device_tensor *bias, yvex_device_tensor *output,
    unsigned long long rows, unsigned long long width, double epsilon,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    unsigned long long sizes[3];
    if (!rows || !width || !isfinite(epsilon) || epsilon <= 0.0 || !yvex_core_u64_mul(rows, width, sizes)) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "cpu.normalization", "bounded geometry and positive F64 epsilon required");
        return YVEX_ERR_FORMAT;
    }
    sizes[1] = sizes[2] = width;
    int rc = cpu_neural_admit(backend, (const yvex_device_tensor *[]){input, weight, bias}, sizes, bias ? 3u : 2u,
        &output, sizes, 1u, facts, err);
    if (rc != YVEX_OK) return rc;
    const float *x = (const float *)input->data, *w = (const float *)weight->data;
    const float *b = bias ? (const float *)bias->data : NULL;
    float *y = (float *)output->data;
    for (unsigned long long row = 0u; row < rows; ++row) {
        double mean = 0.0, variance = 0.0;
        if (bias) {
            for (unsigned long long k = 0u; k < width; ++k) mean += x[row * width + k];
            mean /= (double)width;
        }
        for (unsigned long long k = 0u; k < width; ++k) {
            double v = (double)x[row * width + k] - mean;
            variance += v * v;
        }
        double inverse = 1.0 / sqrt(variance / (double)width + epsilon);
        for (unsigned long long k = 0u; k < width; ++k) {
            double v = ((double)x[row * width + k] - mean) * inverse * (double)w[k];
            if (bias) v += (double)b[k];
            y[row * width + k] = (float)v;
        }
    }
    facts->active_weight_bytes = weight->bytes + (bias ? bias->bytes : 0u);
    return cpu_neural_publish(&output, 1u, err);
}

static int cpu_weighted_rms_bf16(yvex_backend *backend, const yvex_device_tensor *input,
    const yvex_device_tensor *weight, yvex_device_tensor *output,
    unsigned long long rows, unsigned long long width, double epsilon,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    int rc = cpu_normalization_f32(backend, input, weight, NULL, output, rows, width, epsilon, facts, err);
    if (rc != YVEX_OK) return rc;
    output->is_written = 0;
    float *y = (float *)output->data;
    for (unsigned long long i = 0u; i < rows * width; ++i)
        y[i] = yvex_quant_bf16_decode(yvex_quant_bf16_encode(y[i]));
    return cpu_neural_publish(&output, 1u, err);
}

static int cpu_scaled_residual_f32(yvex_backend *backend, const yvex_device_tensor *input,
    const yvex_device_tensor *update, const yvex_device_tensor *scale, yvex_device_tensor *output,
    unsigned long long rows, unsigned long long width, yvex_backend_operation_facts *facts, yvex_error *err)
{
    unsigned long long sizes[3];
    if (!rows || !width || !yvex_core_u64_mul(rows, width, sizes)) return cpu_neural_bounds(err);
    sizes[1] = sizes[0]; sizes[2] = width;
    int rc = cpu_neural_admit(backend, (const yvex_device_tensor *[]){input, update, scale}, sizes, 3u,
        &output, sizes, 1u, facts, err);
    if (rc != YVEX_OK) return rc;
    for (unsigned long long i = 0u; i < sizes[0]; ++i)
        ((float *)output->data)[i] = ((const float *)input->data)[i] +
            ((const float *)update->data)[i] * ((const float *)scale->data)[i % width];
    return cpu_neural_publish(&output, 1u, err);
}

static int cpu_split_interleaved_three(yvex_backend *backend, const yvex_device_tensor *input,
    yvex_device_tensor *q, yvex_device_tensor *k, yvex_device_tensor *v, unsigned long long rows,
    unsigned long long heads, unsigned long long head, yvex_backend_operation_facts *facts, yvex_error *err)
{
    unsigned long long width, size, sizes[3], full;
    if (!rows || !heads || !head || !yvex_core_u64_mul(heads, head, &width) ||
        !yvex_core_u64_mul(rows, width, &size) || !yvex_core_u64_mul(size, 3u, &full)) return cpu_neural_bounds(err);
    sizes[0] = sizes[1] = sizes[2] = size;
    yvex_device_tensor *outputs[] = {q, k, v};
    int rc = cpu_neural_admit(backend, &input, &full, 1u, outputs, sizes, 3u, facts, err);
    if (rc != YVEX_OK) return rc;
    for (unsigned long long i = 0u; i < size; ++i)
        for (unsigned long long part = 0u; part < 3u; ++part)
            ((float *)outputs[part]->data)[i] =
                ((const float *)input->data)[i / head * head * 3u + part * head + i % head];
    return cpu_neural_publish(outputs, 3u, err);
}

static int cpu_swiglu_split_f32(yvex_backend *backend, const yvex_device_tensor *input,
    yvex_device_tensor *output, unsigned long long rows, unsigned long long width, int gate_first,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    unsigned long long size, full;
    if (!rows || !width || (gate_first != 0 && gate_first != 1) ||
        !yvex_core_u64_mul(rows, width, &size) || !yvex_core_u64_mul(size, 2u, &full)) return cpu_neural_bounds(err);
    int rc = cpu_neural_admit(backend, &input, &full, 1u, &output, &size, 1u, facts, err);
    if (rc != YVEX_OK) return rc;
    for (unsigned long long i = 0u; i < size; ++i) {
        const float *row = (const float *)input->data + i / width * width * 2u;
        float gate = row[(gate_first ? 0u : width) + i % width];
        float value = row[(gate_first ? width : 0u) + i % width];
        ((float *)output->data)[i] = gate / (1.0f + expf(-gate)) * value;
    }
    return cpu_neural_publish(&output, 1u, err);
}

static int cpu_rotary_half_f32(yvex_backend *backend, yvex_device_tensor *value,
    const yvex_device_tensor *cosine, const yvex_device_tensor *sine, unsigned long long rows,
    unsigned long long heads, unsigned long long head, unsigned long long rotary,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    unsigned long long width, count, table;
    if (!facts || !rows || !heads || !head || !rotary || rotary > head || (rotary & 1u) ||
        !yvex_core_u64_mul(heads, head, &width) || !yvex_core_u64_mul(rows, width, &count) ||
        !yvex_core_u64_mul(rows, rotary, &table) || !backend_tensor_owner_is(backend, value) ||
        !backend_tensor_f32_elements(value, count) || !value->is_written || value->borrowed_host ||
        !backend_tensor_owner_is(backend, cosine) || !backend_tensor_f32_elements(cosine, table) ||
        !cosine->is_written || !backend_tensor_owner_is(backend, sine) ||
        !backend_tensor_f32_elements(sine, table) || !sine->is_written) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "cpu.rotary", "bounded written tensors and even rotary geometry required");
        return YVEX_ERR_FORMAT;
    }
    const yvex_device_tensor *tables[] = {cosine, sine};
    for (size_t i = 0u; i < 2u; ++i) {
        if (!cpu_storage_disjoint(value, tables[i])) {
            yvex_error_set(err, YVEX_ERR_FORMAT, "cpu.rotary", "rotary tables must not alias mutable values");
            return YVEX_ERR_FORMAT;
        }
    }
    memset(facts, 0, sizeof(*facts));
    value->is_written = 0;
    for (unsigned long long row = 0u; row < rows; ++row)
        for (unsigned long long h = 0u; h < heads; ++h)
            for (unsigned long long lane = 0u; lane < rotary / 2u; ++lane) {
                float *x = (float *)value->data + row * width + h * head;
                float a = x[lane], b = x[rotary / 2u + lane];
                float c = ((const float *)cosine->data)[row * rotary + lane];
                float s = ((const float *)sine->data)[row * rotary + lane];
                x[lane] = a * c - b * s;
                x[rotary / 2u + lane] = b * c + a * s;
            }
    facts->activation_bytes = value->bytes + cosine->bytes + sine->bytes;
    facts->compulsory_memory_facts_available = 1;
    return cpu_neural_publish(&value, 1u, err);
}

static int cpu_attention_workspace(const yvex_transformer_attention_requirement *r,
    unsigned long long *bytes, yvex_error *err)
{
    unsigned long long end, qwidth, kwidth;
    if (bytes) *bytes = 0u;
    if (!r || !bytes || !r->query_tokens || !r->key_value_tokens || !r->query_heads ||
        !r->key_value_heads || r->query_heads % r->key_value_heads || !r->head_dimension ||
        !yvex_core_u64_add(r->query_start, r->query_tokens, &end) || end > r->key_value_tokens ||
        !yvex_core_u64_mul(r->query_heads, r->head_dimension, &qwidth) ||
        !yvex_core_u64_mul(r->key_value_heads, r->head_dimension, &kwidth) ||
        (r->query_token_stride && r->query_token_stride < qwidth) ||
        (r->key_token_stride && r->key_token_stride < kwidth) ||
        (r->value_token_stride && r->value_token_stride < kwidth) ||
        !yvex_core_u64_mul(r->key_value_tokens, sizeof(float), bytes) || *bytes > SIZE_MAX) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "cpu.attention", "bounded exact attention geometry required");
        return YVEX_ERR_FORMAT;
    }
    if (!r->deterministic || r->layout != YVEX_TRANSFORMER_ATTENTION_LAYOUT_TOKEN_HEAD_DIM ||
        (r->mask != YVEX_TRANSFORMER_ATTENTION_MASK_FULL && r->mask != YVEX_TRANSFORMER_ATTENTION_MASK_CAUSAL) ||
        r->numeric_contract != YVEX_TRANSFORMER_ATTENTION_NUMERIC_EXACT_F32 ||
        r->query_dtype != YVEX_DTYPE_F32 || r->key_dtype != YVEX_DTYPE_F32 ||
        r->value_dtype != YVEX_DTYPE_F32 || r->output_dtype != YVEX_DTYPE_F32) {
        *bytes = 0u;
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "cpu.attention", "only deterministic F32 attention is admitted");
        return YVEX_ERR_UNSUPPORTED;
    }
    return YVEX_OK;
}

static int cpu_full_attention(yvex_backend *backend, const yvex_transformer_attention_request *request,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    const yvex_transformer_attention_requirement *r = request ? &request->requirement : NULL;
    unsigned long long bytes, sizes[3], result, widths[3], strides[3];
    int rc = cpu_attention_workspace(r, &bytes, err);
    if (rc != YVEX_OK) return rc;
    widths[0] = r->query_heads * r->head_dimension;
    widths[1] = widths[2] = r->key_value_heads * r->head_dimension;
    strides[0] = r->query_token_stride ? r->query_token_stride : widths[0];
    strides[1] = r->key_token_stride ? r->key_token_stride : widths[1];
    strides[2] = r->value_token_stride ? r->value_token_stride : widths[2];
    for (size_t i = 0u; i < 3u; ++i)
        if (!yvex_core_u64_mul((i ? r->key_value_tokens : r->query_tokens) - 1u, strides[i], sizes + i) ||
            !yvex_core_u64_add(sizes[i], widths[i], sizes + i)) {
            yvex_error_set(err, YVEX_ERR_BOUNDS, "cpu.attention", "strided attention extent overflowed");
            return YVEX_ERR_BOUNDS;
        }
    if (!yvex_core_u64_mul(r->query_tokens, widths[0], &result)) return cpu_neural_bounds(err);
    yvex_device_tensor *output = request->output, *scratch = request->workspace, *owned = NULL;
    rc = cpu_neural_admit(backend, (const yvex_device_tensor *[]){request->query, request->key, request->value},
        sizes, 3u, &output, &result, 1u, facts, err);
    if (rc != YVEX_OK) return rc;
    yvex_backend_tensor_desc d = {.name = "attention-scores", .dtype = YVEX_DTYPE_F32,
        .rank = 1u, .dims = {r->key_value_tokens}, .bytes = bytes};
    if (scratch && (!backend_tensor_owner_is(backend, scratch) || scratch->bytes < bytes ||
        scratch->dtype != YVEX_DTYPE_F32 || scratch->borrowed_host ||
        !cpu_storage_disjoint(scratch, output) || !cpu_storage_disjoint(scratch, request->query) ||
        !cpu_storage_disjoint(scratch, request->key) || !cpu_storage_disjoint(scratch, request->value))) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "cpu.attention", "attention workspace is not independently owned");
        return YVEX_ERR_FORMAT;
    }
    if (!scratch) {
        rc = yvex_backend_tensor_alloc(backend, &d, &owned, err);
        scratch = owned;
    }
    if (rc != YVEX_OK) return rc;
    const float *q = (const float *)request->query->data, *k = (const float *)request->key->data;
    const float *v = (const float *)request->value->data;
    float *scores = (float *)scratch->data, *y = (float *)output->data;
    float scale = 1.0f / sqrtf((float)r->head_dimension);
    memset(y, 0, (size_t)result * sizeof(float));
    for (unsigned long long row = 0u; row < r->query_tokens && rc == YVEX_OK; ++row) {
        unsigned long long keys = r->mask == YVEX_TRANSFORMER_ATTENTION_MASK_CAUSAL ?
            r->query_start + row + 1u : r->key_value_tokens;
        for (unsigned long long head = 0u; head < r->query_heads; ++head) {
            unsigned long long kvhead = head / (r->query_heads / r->key_value_heads);
            float maximum = -INFINITY, sum = 0.0f;
            for (unsigned long long key = 0u; key < keys; ++key) {
                float score = 0.0f;
                for (unsigned long long lane = 0u; lane < r->head_dimension; ++lane)
                    score += q[row * strides[0] + head * r->head_dimension + lane] *
                        k[key * strides[1] + kvhead * r->head_dimension + lane];
                scores[key] = score * scale;
                if (scores[key] > maximum) maximum = scores[key];
            }
            for (unsigned long long key = 0u; key < keys; ++key) {
                scores[key] = expf(scores[key] - maximum);
                sum += scores[key];
            }
            if (!isfinite(sum) || sum <= 0.0f) { rc = YVEX_ERR_FORMAT; break; }
            for (unsigned long long key = 0u; key < keys; ++key) {
                float probability = scores[key] / sum;
                for (unsigned long long lane = 0u; lane < r->head_dimension; ++lane)
                    y[row * widths[0] + head * r->head_dimension + lane] +=
                        probability * v[key * strides[2] + kvhead * r->head_dimension + lane];
            }
        }
    }
    yvex_error cleanup;
    int cleanup_rc = owned ? yvex_backend_tensor_release(backend, &owned, &cleanup) : YVEX_OK;
    if (cleanup_rc != YVEX_OK) { if (err) *err = cleanup; return cleanup_rc; }
    facts->temporary_bytes = bytes;
    if (rc != YVEX_OK) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "cpu.attention", "attention softmax is not finite");
        return rc;
    }
    return cpu_neural_publish(&output, 1u, err);
}

static int cpu_residual_pre(yvex_backend *backend, const yvex_mhc_device_request *r,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    int rc = yvex_mhc_pre_admit(backend, r, facts, err);
    if (rc != YVEX_OK) return rc;
    yvex_mhc_pre_request request = {
        .geometry = r->geometry, .residual = (const float *)r->inputs[0]->data,
        .linear_mixes = (const float *)r->inputs[1]->data,
        .scale = (const float *)r->inputs[2]->data, .base = (const float *)r->inputs[3]->data,
        .rows = r->rows, .residual_stride = r->geometry.streams * r->geometry.width,
        .mix_stride = (r->geometry.streams + 2u) * r->geometry.streams,
        .collapsed = (float *)r->outputs[0]->data, .post = (float *)r->outputs[1]->data,
        .combination = (float *)r->outputs[2]->data, .collapsed_stride = r->geometry.width,
        .post_stride = r->geometry.streams, .combination_stride = r->geometry.streams * r->geometry.streams};
    rc = yvex_mhc_pre_f32(&request, err);
    return rc == YVEX_OK ? cpu_neural_publish(r->outputs, 3u, err) : rc;
}

static int cpu_combine_f32(yvex_backend *backend, const yvex_device_tensor *const *inputs,
    size_t count, int mean, yvex_device_tensor *output, yvex_backend_operation_facts *facts, yvex_error *err)
{
    int rc = yvex_neural_elementwise_admit(backend, inputs, count, output, facts, err);
    if (rc != YVEX_OK) return rc;
    float *y = (float *)output->data;
    for (size_t i = 0u; i < output->bytes / sizeof(float); ++i) {
        float sum = mean ? 0.0f : ((const float *)inputs[0]->data)[i];
        for (size_t j = mean ? 0u : 1u; j < count; ++j) sum += ((const float *)inputs[j]->data)[i];
        y[i] = mean ? sum / (float)count : sum;
    }
    return cpu_neural_publish(&output, 1u, err);
}

static int cpu_clamp_f32(yvex_backend *backend, const yvex_device_tensor *input,
    yvex_device_tensor *output, float lower, float upper, yvex_backend_operation_facts *facts, yvex_error *err)
{
    if (!isfinite(lower) || !isfinite(upper) || lower > upper) return cpu_neural_bounds(err);
    int rc = yvex_neural_elementwise_admit(backend, &input, 1u, output, facts, err);
    if (rc != YVEX_OK) return rc;
    for (size_t i = 0u; i < output->bytes / sizeof(float); ++i)
        ((float *)output->data)[i] = fmaxf(lower, fminf(upper, ((const float *)input->data)[i]));
    return cpu_neural_publish(&output, 1u, err);
}

static int cpu_convolution_1d(yvex_backend *backend, const yvex_convolution_1d_request *r,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    int rc = yvex_convolution_1d_admit(backend, r, facts, err);
    if (rc != YVEX_OK) return rc;
    rc = yvex_convolution_1d_f32(&r->geometry, (const float *)r->input->data,
        r->input->bytes / sizeof(float), (const float *)r->weight->encoded,
        r->weight->encoded_bytes / sizeof(float), r->bias ? (const float *)r->bias->encoded : NULL,
        r->bias ? r->bias->encoded_bytes / sizeof(float) : 0u,
        r->gain ? (const float *)r->gain->encoded : NULL,
        r->gain ? r->gain->encoded_bytes / sizeof(float) : 0u,
        (float *)r->output->data, r->output->bytes / sizeof(float), err);
    if (rc == YVEX_OK) r->output->is_written = 1;
    return rc;
}

static int cpu_clamped_swiglu_bf16(yvex_backend *backend, const yvex_device_tensor *gate,
    const yvex_device_tensor *up, yvex_device_tensor *output, double limit,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    if (output) output->is_written = 0;
    if (facts) memset(facts, 0, sizeof(*facts));
    if (!isfinite(limit) || limit <= 0.0) return cpu_neural_bounds(err);
    const yvex_device_tensor *inputs[] = {gate, up};
    int rc = yvex_neural_elementwise_admit(backend, inputs, 2u, output, facts, err);
    if (rc != YVEX_OK) return rc;
    rc = yvex_clamped_swiglu_bf16((const float *)gate->data, (const float *)up->data,
        output->bytes / sizeof(float), limit, 1.0f, (float *)output->data, err);
    return rc == YVEX_OK ? cpu_neural_publish(&output, 1u, err) : rc;
}

static int cpu_alias_snake(yvex_backend *backend, const yvex_alias_snake_request *r,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    int rc = yvex_alias_snake_admit(backend, r, facts, err);
    if (rc != YVEX_OK) return rc;
    rc = yvex_signal_alias_snake_f32((const float *)r->input->data, r->batch, r->channels, r->length,
        (const float *)r->alpha->encoded, (const float *)r->beta->encoded,
        (const float *)r->up_filter->encoded, (const float *)r->down_filter->encoded,
        (float *)r->output->data, (float *)r->workspace->data, r->workspace->bytes / sizeof(float), err);
    if (rc == YVEX_OK) r->output->is_written = 1;
    return rc;
}

static int cpu_bf16_round(yvex_backend *backend, yvex_device_tensor *value,
    unsigned long long count, yvex_backend_operation_facts *facts, yvex_error *err)
{
    if (!facts || !count || !backend_tensor_owner_is(backend, value) ||
        !backend_tensor_f32_elements(value, count) || !value->is_written) return cpu_neural_bounds(err);
    memset(facts, 0, sizeof(*facts));
    for (unsigned long long i = 0u; i < count; ++i) {
        float *x = (float *)value->data + i;
        *x = yvex_quant_bf16_decode(yvex_quant_bf16_encode(*x));
    }
    return cpu_neural_publish(&value, 1u, err);
}

static int cpu_silu(yvex_backend *backend, const yvex_device_tensor *input,
    yvex_device_tensor *output, unsigned long long count, int bf16,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    if (!count || (bf16 != 0 && bf16 != 1)) return cpu_neural_bounds(err);
    int rc = cpu_neural_admit(backend, &input, &count, 1u, &output, &count, 1u, facts, err);
    if (rc != YVEX_OK) return rc;
    for (unsigned long long i = 0u; i < count; ++i) {
        float x = ((const float *)input->data)[i], y = x / (1.0f + expf(-x));
        ((float *)output->data)[i] = bf16 ? yvex_quant_bf16_decode(yvex_quant_bf16_encode(y)) : y;
    }
    return cpu_neural_publish(&output, 1u, err);
}

static int cpu_indexed_conditioning(yvex_backend *backend, const yvex_device_tensor *input,
    const yvex_device_tensor *table, const unsigned int *indices, const yvex_device_tensor *update,
    yvex_device_tensor *output, unsigned long long rows, unsigned long long width,
    unsigned long long table_rows, unsigned long long parameters, unsigned int first, unsigned int scale,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    unsigned long long count, table_count;
    if (!rows || !width || !table_rows || !parameters || first >= parameters ||
        (!update && scale >= parameters) || !indices ||
        !yvex_core_u64_mul(rows, width, &count) ||
        !yvex_core_u64_mul(table_rows, parameters, &table_count) ||
        !yvex_core_u64_mul(table_count, width, &table_count)) return cpu_neural_bounds(err);
    for (unsigned long long row = 0u; row < rows; ++row)
        if (indices[row] >= table_rows) return cpu_neural_bounds(err);
    int rc = cpu_neural_admit(backend, (const yvex_device_tensor *[]){input, table, update},
        (unsigned long long[]){count, table_count, count}, update ? 3u : 2u,
        &output, &count, 1u, facts, err);
    if (rc != YVEX_OK) return rc;
    const float *x = (const float *)input->data, *t = (const float *)table->data;
    const float *u = update ? (const float *)update->data : NULL;
    float *y = (float *)output->data;
    for (unsigned long long i = 0u; i < count; ++i) {
        unsigned long long base = (unsigned long long)indices[i / width] * parameters * width + i % width;
        float value;
        if (update) {
            value = yvex_quant_bf16_decode(yvex_quant_bf16_encode(t[base + first * width] * u[i]));
            value = x[i] + value;
        } else {
            float factor = yvex_quant_bf16_decode(yvex_quant_bf16_encode(1.0f + t[base + scale * width]));
            value = yvex_quant_bf16_decode(yvex_quant_bf16_encode(x[i] * factor));
            value += t[base + first * width];
        }
        y[i] = yvex_quant_bf16_decode(yvex_quant_bf16_encode(value));
    }
    return cpu_neural_publish(&output, 1u, err);
}

static int cpu_modulate_bf16(yvex_backend *backend, const yvex_device_tensor *input,
    const yvex_device_tensor *table, const unsigned int *indices, yvex_device_tensor *output,
    unsigned long long rows, unsigned long long width, unsigned long long table_rows,
    unsigned long long parameters, unsigned int shift, unsigned int scale,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    return cpu_indexed_conditioning(backend, input, table, indices, NULL, output,
        rows, width, table_rows, parameters, shift, scale, facts, err);
}

static int cpu_gated_residual_bf16(yvex_backend *backend, const yvex_device_tensor *input,
    const yvex_device_tensor *table, const unsigned int *indices, const yvex_device_tensor *update,
    yvex_device_tensor *output, unsigned long long rows, unsigned long long width,
    unsigned long long table_rows, unsigned long long parameters, unsigned int gate,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    return cpu_indexed_conditioning(backend, input, table, indices, update, output,
        rows, width, table_rows, parameters, gate, 0u, facts, err);
}

static const yvex_backend_transformer_operations *cpu_transformer_operations(const yvex_backend *backend)
{
    static const yvex_backend_transformer_operations operations = {
        .bf16_round = cpu_bf16_round, .silu = cpu_silu,
        .modulate_bf16 = cpu_modulate_bf16, .gated_residual_bf16 = cpu_gated_residual_bf16,
        .combine_f32 = cpu_combine_f32, .clamp_f32 = cpu_clamp_f32,
        .clamped_swiglu_bf16 = cpu_clamped_swiglu_bf16,
        .convolution_1d = cpu_convolution_1d, .alias_snake = cpu_alias_snake,
        .feature_mean = cpu_stream_mean, .final = cpu_mhc_head, .residual_post = cpu_residual_post,
        .residual_pre = cpu_residual_pre,
        .linear_bias_f32 = cpu_linear_bias_f32, .normalization_f32 = cpu_normalization_f32,
        .weighted_rms_bf16 = cpu_weighted_rms_bf16,
        .sinusoidal_embedding = cpu_sinusoidal_embedding,
        .scaled_residual_f32 = cpu_scaled_residual_f32, .split_interleaved_three = cpu_split_interleaved_three,
        .swiglu_split_f32 = cpu_swiglu_split_f32, .rotary_half_f32 = cpu_rotary_half_f32,
        .attention_workspace_required = cpu_attention_workspace, .attention_execute = cpu_full_attention};
    (void)backend;
    return &operations;
}

static const yvex_backend_vtable cpu_vtable = {
    .memory_stats = cpu_memory_stats,
    .device_info = cpu_device_info,
    .tensor_alloc = cpu_tensor_alloc,
    .tensor_free = cpu_tensor_free,
    .tensor_write = cpu_tensor_write,
    .tensor_read = cpu_tensor_read,
    .tensor_zero = cpu_tensor_zero,
    .tensor_copy = cpu_tensor_copy,
    .sync = cpu_sync,
    .query_capability = cpu_query_capability,
    .op_embed = cpu_op_embed,
    .op_rms_norm = cpu_op_rms_norm,
    .op_rope = cpu_op_rope,
    .op_matmul = cpu_op_matmul,
    .op_mlp = cpu_op_mlp,
    .op_attention = cpu_op_attention,
    .transformer_operations = cpu_transformer_operations,
};

int yvex_backend_open_cpu(yvex_backend **out, yvex_error *err)
{
    yvex_backend *backend;

    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_backend_open_cpu", "out is required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;

    backend = (yvex_backend *)calloc(1, sizeof(*backend));
    if (!backend) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_backend_open_cpu",
                       "failed to allocate CPU backend");
        return YVEX_ERR_NOMEM;
    }
    backend->kind = YVEX_BACKEND_KIND_CPU;
    atomic_init(&backend->status, YVEX_BACKEND_STATUS_READY);
    backend->vtable = &cpu_vtable;
    backend->resource_owner = backend;
    atomic_init(&backend->lifecycle, 0ull);
    backend->device_info.kind = YVEX_BACKEND_KIND_CPU;
    backend->device_info.name = "cpu";
    backend->tensor_id_next = 1;

    *out = backend;
    yvex_error_clear(err);
    return YVEX_OK;
}

static float cpu_decode_f16(const unsigned char *bytes)
{
    unsigned short bits = (unsigned short)bytes[0] | (unsigned short)((unsigned short)bytes[1] << 8);

    return yvex_quant_f16_decode(bits);
}

static double backend_sqrt_double(double x)
{
    double guess;
    unsigned int i;

    if (x <= 0.0) {
        return 0.0;
    }
    guess = x >= 1.0 ? x : 1.0;
    for (i = 0; i < 32u; ++i) {
        guess = 0.5 * (guess + (x / guess));
    }
    return guess;
}

static double backend_abs_double(double x)
{
    return x < 0.0 ? -x : x;
}

static double backend_wrap_radians(double x)
{
    const double two_pi = 6.28318530717958647692;

    while (x > 3.14159265358979323846) {
        x -= two_pi;
    }
    while (x < -3.14159265358979323846) {
        x += two_pi;
    }
    return x;
}

static void backend_sincos_double(double x, double *sine, double *cosine)
{
    double x2;
    double s;
    double c;

    x = backend_wrap_radians(x);
    x2 = x * x;
    s = x * (1.0 -
             (x2 / 6.0) +
             ((x2 * x2) / 120.0) -
             ((x2 * x2 * x2) / 5040.0) +
             ((x2 * x2 * x2 * x2) / 362880.0));
    c = 1.0 -
        (x2 / 2.0) +
        ((x2 * x2) / 24.0) -
        ((x2 * x2 * x2) / 720.0) +
        ((x2 * x2 * x2 * x2) / 40320.0);
    if (backend_abs_double(s) < 0.000000000001) {
        s = 0.0;
    }
    if (backend_abs_double(c) < 0.000000000001) {
        c = 0.0;
    }
    if (sine) {
        *sine = s;
    }
    if (cosine) {
        *cosine = c;
    }
}

static double backend_exp_double(double x)
{
    const double ln2 = 0.69314718055994530942;
    double term;
    double sum;
    int n = 0;
    unsigned int i;

    if (x < -60.0) {
        return 0.0;
    }
    if (x > 60.0) {
        x = 60.0;
    }
    while (x > 0.5) {
        x -= ln2;
        ++n;
    }
    while (x < -0.5) {
        x += ln2;
        --n;
    }

    term = 1.0;
    sum = 1.0;
    for (i = 1u; i <= 18u; ++i) {
        term *= x / (double)i;
        sum += term;
    }
    while (n > 0) {
        sum *= 2.0;
        --n;
    }
    while (n < 0) {
        sum *= 0.5;
        ++n;
    }
    return sum;
}

static double backend_silu_double(double x)
{
    return x / (1.0 + backend_exp_double(-x));
}

static int cpu_op_embed(yvex_backend *backend,
                      const yvex_device_tensor *embedding,
                      const unsigned int *token_ids,
                      unsigned long long token_count,
                      yvex_device_tensor *out,
                      yvex_error *err)
{
    unsigned long long hidden_size;
    unsigned long long vocab_size;
    unsigned long long e;
    unsigned long long t;
    const float *embedding_f32;
    const unsigned char *embedding_f16;
    float *out_data;
    int rc;

    rc = yvex_backend_validate_embed(
        backend, embedding, token_ids, token_count, out, &hidden_size, &vocab_size,
        "backend layer CPU embed supports F32 and F16 embeddings with F32 output",
        "yvex_backend_op_embed", err);
    if (rc != YVEX_OK) {
        return rc;
    }

    embedding_f32 = (const float *)embedding->data;
    embedding_f16 = (const unsigned char *)embedding->data;
    out_data = (float *)out->data;
    for (t = 0; t < token_count; ++t) {
        unsigned int token_id = token_ids[t];
        for (e = 0; e < hidden_size; ++e) {
            unsigned long long index = ((unsigned long long)token_id * hidden_size) + e;
            if (embedding->dtype == YVEX_DTYPE_F16) {
                out_data[(t * hidden_size) + e] =
                    cpu_decode_f16(embedding_f16 + (index * 2ull));
            } else {
                out_data[(t * hidden_size) + e] = embedding_f32[index];
            }
        }
    }
    out->is_written = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Compute scaled causal attention directly over admitted CPU F32 tensors. */
static int cpu_op_attention(yvex_backend *backend,
                          const yvex_device_tensor *query,
                          const yvex_device_tensor *keys,
                          const yvex_device_tensor *values,
                          unsigned long long seq_len,
                          unsigned long long position,
                          float scale,
                          int causal,
                          yvex_device_tensor *score_scratch,
                          yvex_device_tensor *probability_scratch,
                          yvex_device_tensor *out,
                          yvex_error *err)
{
    const float *q_data;
    const float *k_data;
    const float *v_data;
    float *score_data;
    float *prob_data;
    float *out_data;
    unsigned long long head_dim;
    unsigned long long kv_elements;
    unsigned long long visible_count;
    unsigned long long i;
    unsigned long long d;
    double max_score = 0.0;
    double sum_exp = 0.0;
    int rc;

    if (!isfinite(scale) || scale <= 0.0f) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_backend_op_attention",
                       "scale must be finite and positive");
        return YVEX_ERR_INVALID_ARG;
    }
    rc = yvex_backend_validate_attention(
        backend, query, keys, values, seq_len, position, score_scratch,
        probability_scratch, out, &head_dim, &kv_elements,
        "yvex_backend_op_attention", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    (void)kv_elements;

    q_data = (const float *)query->data;
    k_data = (const float *)keys->data;
    v_data = (const float *)values->data;
    score_data = (float *)score_scratch->data;
    prob_data = (float *)probability_scratch->data;
    out_data = (float *)out->data;
    visible_count = causal ? position + 1ull : seq_len;

    for (i = 0; i < seq_len; ++i) {
        double score = 0.0;
        if (causal && i > position) {
            score_data[i] = 0.0f;
            prob_data[i] = 0.0f;
            continue;
        }
        for (d = 0; d < head_dim; ++d) {
            score += (double)q_data[d] * (double)k_data[(i * head_dim) + d];
        }
        score *= (double)scale;
        score_data[i] = (float)score;
        if (i == 0 || score > max_score) {
            max_score = score;
        }
    }
    for (i = 0; i < visible_count; ++i) {
        double e = backend_exp_double((double)score_data[i] - max_score);
        prob_data[i] = (float)e;
        sum_exp += e;
    }
    if (sum_exp <= 0.0) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_backend_op_attention",
                       "attention softmax sum is zero");
        return YVEX_ERR_STATE;
    }
    for (i = 0; i < visible_count; ++i) {
        prob_data[i] = (float)((double)prob_data[i] / sum_exp);
    }
    for (d = 0; d < head_dim; ++d) {
        double value = 0.0;
        for (i = 0; i < visible_count; ++i) {
            value += (double)prob_data[i] * (double)v_data[(i * head_dim) + d];
        }
        out_data[d] = (float)value;
    }

    score_scratch->is_written = 1;
    probability_scratch->is_written = 1;
    out->is_written = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}

static int cpu_op_matmul(yvex_backend *backend,
                       const yvex_device_tensor *input,
                       const yvex_device_tensor *weight,
                       yvex_device_tensor *out,
                       yvex_error *err)
{
    const float *input_data;
    const float *weight_data;
    float *out_data;
    unsigned long long m;
    unsigned long long k;
    unsigned long long n;
    unsigned long long row;
    unsigned long long col;
    int rc;

    rc = yvex_backend_validate_matmul(backend, input, weight, out, &m, &k, &n,
                                      "yvex_backend_op_matmul", err);
    if (rc != YVEX_OK) {
        return rc;
    }

    input_data = (const float *)input->data;
    weight_data = (const float *)weight->data;
    out_data = (float *)out->data;
    for (row = 0; row < m; ++row) {
        for (col = 0; col < n; ++col) {
            double sum = 0.0;
            unsigned long long inner;
            for (inner = 0; inner < k; ++inner) {
                sum += (double)input_data[(row * k) + inner] *
                       (double)weight_data[(inner * n) + col];
            }
            out_data[(row * n) + col] = (float)sum;
        }
    }
    out->is_written = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Evaluate the gated SiLU CPU MLP primitive without temporary tensor ownership. */
static int cpu_op_mlp(yvex_backend *backend,
                    const yvex_device_tensor *input,
                    const yvex_device_tensor *gate_weight,
                    const yvex_device_tensor *up_weight,
                    const yvex_device_tensor *down_weight,
                    const yvex_mlp_options *options,
                    yvex_device_tensor *intermediate,
                    yvex_device_tensor *out,
                    yvex_error *err)
{
    const float *input_data;
    const float *gate_data;
    const float *up_data;
    const float *down_data;
    float *intermediate_data;
    float *out_data;
    unsigned long long batch;
    unsigned long long hidden_dim;
    unsigned long long ffn_dim;
    unsigned long long gate_offset;
    unsigned long long up_offset;
    unsigned long long down_offset;
    unsigned long long row;
    unsigned long long j;
    unsigned long long h;
    int rc;

    rc = yvex_backend_validate_mlp(
        backend, input, gate_weight, up_weight, down_weight, options,
        intermediate, out, &batch, &hidden_dim, &ffn_dim, &gate_offset,
        &up_offset, &down_offset, "yvex_backend_op_mlp", err);
    if (rc != YVEX_OK) {
        return rc;
    }

    input_data = (const float *)input->data;
    gate_data = (const float *)gate_weight->data + gate_offset;
    up_data = (const float *)up_weight->data + up_offset;
    down_data = (const float *)down_weight->data + down_offset;
    intermediate_data = (float *)intermediate->data;
    out_data = (float *)out->data;

    for (row = 0; row < batch; ++row) {
        for (j = 0; j < ffn_dim; ++j) {
            double gate_sum = 0.0;
            double up_sum = 0.0;
            for (h = 0; h < hidden_dim; ++h) {
                double x = (double)input_data[(row * hidden_dim) + h];
                gate_sum += x * (double)gate_data[(h * ffn_dim) + j];
                up_sum += x * (double)up_data[(h * ffn_dim) + j];
            }
            intermediate_data[(row * ffn_dim) + j] =
                (float)(backend_silu_double(gate_sum) * up_sum);
        }
        for (h = 0; h < hidden_dim; ++h) {
            double sum = 0.0;
            for (j = 0; j < ffn_dim; ++j) {
                sum += (double)intermediate_data[(row * ffn_dim) + j] *
                       (double)down_data[(j * hidden_dim) + h];
            }
            out_data[(row * hidden_dim) + h] = (float)sum;
        }
    }
    intermediate->is_written = 1;
    out->is_written = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}

static int cpu_op_rope(yvex_backend *backend,
                     const yvex_device_tensor *input,
                     unsigned long long position,
                     float rope_base,
                     yvex_device_tensor *out,
                     yvex_error *err)
{
    const float *input_data;
    float *out_data;
    unsigned long long head_dim;
    unsigned long long pair_count;
    unsigned long long pair;
    double inverse_root;
    double frequency = 1.0;
    int rc;

    if (!backend_tensor_owner_is(backend, input) ||
        !backend_tensor_owner_is(backend, out)) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_backend_op_rope",
                       "input and output tensors must belong to this backend");
        return YVEX_ERR_STATE;
    }
    if (input->dtype != YVEX_DTYPE_F32 || out->dtype != YVEX_DTYPE_F32) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_backend_op_rope",
                       "CPU RoPE supports F32 input/output");
        return YVEX_ERR_UNSUPPORTED;
    }
    if (!yvex_backend_tensor_same_shape(input, out)) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_backend_op_rope",
                       "RoPE output shape must match input shape");
        return YVEX_ERR_FORMAT;
    }
    if (!isfinite(rope_base) || rope_base <= 1.0f) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_backend_op_rope",
                       "rope_base must be finite and greater than 1");
        return YVEX_ERR_INVALID_ARG;
    }
    rc = yvex_backend_validate_rope(input, &head_dim, "yvex_backend_op_rope", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    if (!backend_tensor_f32_elements(input, head_dim) ||
        !backend_tensor_f32_elements(out, head_dim)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_backend_op_rope",
                       "RoPE input/output bytes must match F32 head_dim");
        return YVEX_ERR_BOUNDS;
    }

    pair_count = head_dim / 2ull;
    inverse_root = 1.0 / yvex_backend_nth_root((double)rope_base, pair_count);
    input_data = (const float *)input->data;
    out_data = (float *)out->data;

    for (pair = 0; pair < pair_count; ++pair) {
        unsigned long long even_index = pair * 2ull;
        unsigned long long odd_index = even_index + 1ull;
        double sine;
        double cosine;
        double even = (double)input_data[even_index];
        double odd = (double)input_data[odd_index];
        double angle = (double)position * frequency;

        backend_sincos_double(angle, &sine, &cosine);
        out_data[even_index] = (float)((even * cosine) - (odd * sine));
        out_data[odd_index] = (float)((even * sine) + (odd * cosine));
        frequency *= inverse_root;
    }

    out->is_written = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}

static int cpu_op_rms_norm(yvex_backend *backend,
                         const yvex_device_tensor *input,
                         const yvex_device_tensor *weight,
                         float epsilon,
                         yvex_device_tensor *out,
                         yvex_error *err)
{
    const float *input_data;
    const float *weight_f32;
    const unsigned char *weight_f16;
    float *out_data;
    unsigned long long hidden_size;
    unsigned long long row_count;
    unsigned long long row;
    unsigned long long i;
    int rc;

    rc = yvex_backend_validate_rms_norm(
        backend, input, weight, epsilon, out, &hidden_size,
        "CPU RMSNorm supports F32 input/output with F16 or F32 weight",
        "yvex_backend_op_rms_norm", err);
    if (rc != YVEX_OK) {
        return rc;
    }

    input_data = (const float *)input->data;
    weight_f32 = (const float *)weight->data;
    weight_f16 = (const unsigned char *)weight->data;
    out_data = (float *)out->data;
    row_count = input->rank == 2 ? input->dims[0] : 1ull;

    for (row = 0ull; row < row_count; ++row) {
        unsigned long long offset = row * hidden_size;
        double sum_squares = 0.0;
        double rms;
        double inv_rms;

        for (i = 0ull; i < hidden_size; ++i) {
            sum_squares += (double)input_data[offset + i] *
                           (double)input_data[offset + i];
        }
        rms = backend_sqrt_double((sum_squares / (double)hidden_size) + (double)epsilon);
        inv_rms = rms > 0.0 ? 1.0 / rms : 0.0;
        for (i = 0ull; i < hidden_size; ++i) {
            float w = weight->dtype == YVEX_DTYPE_F16
                          ? cpu_decode_f16(weight_f16 + (i * 2ull))
                          : weight_f32[i];
            out_data[offset + i] =
                (float)((double)input_data[offset + i] * inv_rms * (double)w);
        }
    }

    out->is_written = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}

static int cpu_tensor_alloc(yvex_backend *backend,
                          const yvex_backend_tensor_desc *desc,
                          yvex_device_tensor **out,
                          yvex_error *err)
{
    yvex_device_tensor *tensor;
    unsigned int i;
    int rc;

    if (!backend || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "cpu.tensor_alloc", "backend and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;

    rc = yvex_backend_memory_can_add(backend, desc->bytes, "CPU", "cpu.tensor_alloc", err);
    if (rc != YVEX_OK) {
        return rc;
    }

    tensor = (yvex_device_tensor *)calloc(1, sizeof(*tensor));
    if (!tensor) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "cpu.tensor_alloc", "failed to allocate tensor object");
        return YVEX_ERR_NOMEM;
    }
    tensor->data = (unsigned char *)calloc(1, (size_t)desc->bytes);
    if (!tensor->data) {
        free(tensor);
        yvex_error_set(err, YVEX_ERR_NOMEM, "cpu.tensor_alloc", "failed to allocate tensor data");
        return YVEX_ERR_NOMEM;
    }
    tensor->name = yvex_core_strdup(desc->name);
    if (!tensor->name) {
        free(tensor->data);
        free(tensor);
        yvex_error_set(err, YVEX_ERR_NOMEM, "cpu.tensor_alloc", "failed to copy tensor name");
        return YVEX_ERR_NOMEM;
    }
    tensor->owner = backend;
    tensor->owner_id = backend->tensor_id_next++;
    tensor->dtype = desc->dtype;
    tensor->rank = desc->rank;
    for (i = 0; i < desc->rank; ++i) {
        tensor->dims[i] = desc->dims[i];
    }
    tensor->bytes = desc->bytes;

    backend_memory_acquire(backend, desc->bytes);

    *out = tensor;
    yvex_error_clear(err);
    return YVEX_OK;
}

static int cpu_tensor_free(yvex_backend *backend,
                         yvex_device_tensor *tensor,
                         yvex_error *err)
{
    if (!backend || !tensor || !backend_tensor_owner_is(backend, tensor)) {
        yvex_error_set(err, YVEX_ERR_STATE, "cpu.tensor_free",
                       "tensor does not belong to this backend");
        return YVEX_ERR_STATE;
    }
    backend_memory_release(backend, tensor->bytes);
    tensor->owner = NULL;
    tensor->owner_id = 0;
    free(tensor->data);
    free(tensor->name);
    free(tensor);
    yvex_error_clear(err);
    return YVEX_OK;
}

static int cpu_tensor_write(yvex_backend *backend,
                          yvex_device_tensor *tensor,
                          const void *src,
                          unsigned long long len,
                          yvex_error *err)
{
    int rc = yvex_backend_tensor_rw_validate(
        "yvex_backend_tensor_write", backend, tensor, len, err);

    if (rc != YVEX_OK) {
        return rc;
    }
    memcpy(tensor->data, src, (size_t)len);
    tensor->is_written = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}

static int cpu_tensor_read(yvex_backend *backend,
                         const yvex_device_tensor *tensor,
                         void *dst,
                         unsigned long long len,
                         yvex_error *err)
{
    int rc = yvex_backend_tensor_rw_validate(
        "yvex_backend_tensor_read", backend, tensor, len, err);

    if (rc != YVEX_OK) {
        return rc;
    }
    memcpy(dst, tensor->data, (size_t)len);
    yvex_error_clear(err);
    return YVEX_OK;
}

static int cpu_tensor_zero(yvex_backend *backend, yvex_device_tensor *tensor,
                           yvex_error *err)
{
    if (!backend_tensor_owner_is(backend, tensor) || tensor->borrowed_host) {
        yvex_error_set(err, YVEX_ERR_STATE, "cpu.tensor.zero",
                       "one CPU-owned mutable tensor is required");
        return YVEX_ERR_STATE;
    }
    memset(tensor->data, 0, (size_t)tensor->bytes);
    tensor->is_written = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}

static int cpu_tensor_copy(yvex_backend *backend,
                         yvex_device_tensor *dst,
                         const yvex_device_tensor *src,
                         yvex_error *err)
{
    int status = yvex_backend_tensor_copy_validate(
        backend, dst, src, "yvex_backend_tensor_copy", err);

    if (status != YVEX_OK) {
        return status;
    }
    memcpy(dst->data, src->data, (size_t)src->bytes);
    dst->is_written = src->is_written;
    yvex_error_clear(err);
    return YVEX_OK;
}
