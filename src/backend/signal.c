/* Portable signal numerics: explicit geometry and buffers, never decoder topology. */
#include <yvex/internal/neural_operations.h>
#include <yvex/qtype.h>
#include "src/backend/private.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

static int signal_refuse(yvex_error *err, yvex_status status, const char *message)
{
    yvex_error_set(err, status, "backend.signal", message);
    return status;
}

static int signal_disjoint(const void *left, unsigned long long left_bytes,
    const void *right, unsigned long long right_bytes)
{
    uintptr_t a = (uintptr_t)left, b = (uintptr_t)right;
    return left_bytes <= UINTPTR_MAX - a && right_bytes <= UINTPTR_MAX - b &&
        (a + left_bytes <= b || b + right_bytes <= a);
}

int yvex_mhc_pre_admit(yvex_backend *backend, const yvex_mhc_device_request *r,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    unsigned long long counts[8], mixing, square, total = 0u;
    const yvex_mhc_geometry *g = r ? &r->geometry : NULL;
    if (facts) memset(facts, 0, sizeof(*facts));
    if (!facts || !g || !r->rows || !g->streams || !g->width || !g->sinkhorn_iterations ||
        !isfinite(g->rms_epsilon) || g->rms_epsilon <= 0.0 || !isfinite(g->epsilon) ||
        g->epsilon <= 0.0 || !isfinite(g->post_multiplier) || g->post_multiplier <= 0.0 ||
        !yvex_core_u64_mul(g->streams, g->width, &counts[0]) ||
        !yvex_core_u64_mul(counts[0], r->rows, &counts[0]) ||
        !yvex_core_u64_add(g->streams, 2u, &mixing) ||
        !yvex_core_u64_mul(mixing, g->streams, &mixing) ||
        !yvex_core_u64_mul(mixing, r->rows, &counts[1]) ||
        !yvex_core_u64_mul(g->width, r->rows, &counts[4]) ||
        !yvex_core_u64_mul(g->streams, r->rows, &counts[5]) ||
        !yvex_core_u64_mul(g->streams, g->streams, &square) ||
        !yvex_core_u64_mul(square, r->rows, &counts[6])) goto invalid;
    counts[2] = 3u; counts[3] = mixing; counts[7] = counts[0];
    const yvex_device_tensor *t[] = {r->inputs[0], r->inputs[1], r->inputs[2], r->inputs[3],
        r->outputs[0], r->outputs[1], r->outputs[2], r->workspace};
    size_t count = yvex_backend_kind_of(backend) == YVEX_BACKEND_KIND_CPU ? 7u : 8u;
    for (size_t i = 0u; i < count; ++i) {
        if (!backend_tensor_owner_is(backend, t[i]) ||
            !backend_tensor_f32_elements(t[i], counts[i]) ||
            (i < 4u ? !t[i]->is_written : t[i]->borrowed_host) ||
            !yvex_core_u64_add(total, t[i]->bytes, &total)) goto invalid;
        if (i >= 4u)
            for (size_t j = 0u; j < i; ++j)
                if (!signal_disjoint(t[i]->data, t[i]->bytes, t[j]->data, t[j]->bytes)) goto invalid;
    }
    for (size_t i = 0u; i < 3u; ++i) r->outputs[i]->is_written = 0;
    facts->activation_bytes = total;
    facts->compulsory_memory_facts_available = 1;
    return YVEX_OK;
invalid:
    return signal_refuse(err, YVEX_ERR_FORMAT,
        "mHC operands require bounded geometry, independent owned storage and exact initialized inputs");
}

static int signal_admit(yvex_backend *backend, const yvex_device_tensor *input,
    yvex_device_tensor *output, yvex_device_tensor *workspace,
    unsigned long long input_values, unsigned long long output_values,
    unsigned long long scratch_values, const yvex_component_encoded_weight *const *weights,
    const unsigned long long *counts, size_t count,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    if (facts) memset(facts, 0, sizeof(*facts));
    if (!facts || !backend_tensor_owner_is(backend, input) || !input->is_written ||
        !backend_tensor_f32_elements(input, input_values) || !input_values ||
        !backend_tensor_owner_is(backend, output) || !output_values || output->borrowed_host ||
        !backend_tensor_f32_elements(output, output_values) ||
        !signal_disjoint(input->data, input->bytes, output->data, output->bytes)) goto invalid;
    if (scratch_values && (!backend_tensor_owner_is(backend, workspace) || workspace->borrowed_host ||
        !backend_tensor_f32_elements(workspace, scratch_values) ||
        !signal_disjoint(input->data, input->bytes, workspace->data, workspace->bytes) ||
        !signal_disjoint(output->data, output->bytes, workspace->data, workspace->bytes))) goto invalid;
    for (size_t i = 0u; i < count; ++i) {
        const yvex_component_encoded_weight *w = weights[i];
        unsigned long long bytes;
        if (!w) { if (counts[i]) goto invalid; else continue; }
        if (!w->encoded || w->qtype != YVEX_GGUF_QTYPE_F32 || !counts[i] ||
            !yvex_core_u64_mul(counts[i], sizeof(float), &bytes) || bytes != w->encoded_bytes ||
            !yvex_core_u64_add(facts->active_weight_bytes, bytes, &facts->active_weight_bytes)) goto invalid;
        if (yvex_backend_kind_of(backend) == YVEX_BACKEND_KIND_CPU &&
            (!signal_disjoint(output->data, output->bytes, w->encoded, bytes) ||
             (scratch_values && !signal_disjoint(workspace->data, workspace->bytes, w->encoded, bytes))))
            goto invalid;
    }
    if (!yvex_core_u64_add(input->bytes, output->bytes, &facts->activation_bytes) ||
        (scratch_values && !yvex_core_u64_add(facts->activation_bytes, workspace->bytes,
            &facts->activation_bytes))) goto invalid;
    facts->compulsory_memory_facts_available = 1;
    output->is_written = 0;
    return YVEX_OK;
invalid:
    return signal_refuse(err, YVEX_ERR_FORMAT,
        "signal operands have incompatible ownership, extent, precision or alias");
}

int yvex_neural_elementwise_admit(yvex_backend *backend, const yvex_device_tensor *const *inputs,
    size_t count, yvex_device_tensor *output, yvex_backend_operation_facts *facts, yvex_error *err)
{
    if (!inputs || !count || count > 16u || !output || !output->bytes || output->bytes % sizeof(float))
        return signal_refuse(err, YVEX_ERR_INVALID_ARG, "bounded elementwise operands are required");
    unsigned long long values = output->bytes / sizeof(float);
    for (size_t i = 0u; i < count; ++i) {
        int rc = signal_admit(backend, inputs[i], output, NULL, values, values, 0u, NULL, NULL, 0u, facts, err);
        if (rc != YVEX_OK) return rc;
        if (inputs[i]->rank != output->rank || memcmp(inputs[i]->dims, output->dims,
            output->rank * sizeof(output->dims[0])))
            return signal_refuse(err, YVEX_ERR_FORMAT, "elementwise shapes disagree");
    }
    if (!yvex_core_u64_mul(output->bytes, count + 1u, &facts->activation_bytes))
        return signal_refuse(err, YVEX_ERR_BOUNDS, "elementwise population overflowed");
    return YVEX_OK;
}

int yvex_convolution_1d_admit(yvex_backend *backend, const yvex_convolution_1d_request *r,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    unsigned long long length, input, output, weights, scales;
    if (!r) return signal_refuse(err, YVEX_ERR_INVALID_ARG, "convolution operation is required");
    const yvex_convolution_1d_geometry *g = &r->geometry;
    int rc = yvex_convolution_1d_output_length(g, &length, err);
    if (rc != YVEX_OK) return rc;
    if (length != r->output_length ||
        !yvex_core_u64_mul(g->batch, g->input_channels, &input) ||
        !yvex_core_u64_mul(input, g->input_length, &input) ||
        !yvex_core_u64_mul(g->batch, g->output_channels, &output) ||
        !yvex_core_u64_mul(output, length, &output) ||
        !yvex_core_u64_mul(g->input_channels, g->output_channels, &weights) ||
        !yvex_core_u64_mul(weights, g->kernel_size, &weights))
        return signal_refuse(err, YVEX_ERR_BOUNDS, "convolution population disagrees with admitted geometry");
    scales = r->gain ? (g->transposed ? g->input_channels : g->output_channels) : 0u;
    return signal_admit(backend, r->input, r->output, r->workspace, input, output, scales,
        (const yvex_component_encoded_weight *[]){r->weight, r->gain, r->bias},
        (unsigned long long[]){weights, scales, r->bias ? g->output_channels : 0u}, 3u, facts, err);
}

int yvex_alias_snake_admit(yvex_backend *backend, const yvex_alias_snake_request *r,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    unsigned long long values, scratch;
    if (!r || !r->batch || !r->channels || !r->length ||
        !yvex_core_u64_mul(r->batch, r->channels, &values) ||
        !yvex_core_u64_mul(values, r->length, &values) ||
        !yvex_core_u64_mul(values, 2u, &scratch))
        return signal_refuse(err, YVEX_ERR_BOUNDS, "alias-free activation population overflowed");
    return signal_admit(backend, r->input, r->output, r->workspace, values, values, scratch,
        (const yvex_component_encoded_weight *[]){r->alpha, r->beta, r->up_filter, r->down_filter},
        (unsigned long long[]){r->channels, r->channels, 12u, 12u}, 4u, facts, err);
}

int yvex_convolution_1d_output_length(const yvex_convolution_1d_geometry *geometry,
                                    unsigned long long *output_length, yvex_error *err)
{
    unsigned long long dilated_kernel;
    unsigned long long padded;
    unsigned long long result;

    if (output_length) *output_length = 0ull;
    if (!geometry || !output_length || !geometry->batch || !geometry->input_channels ||
        !geometry->output_channels || !geometry->input_length || !geometry->kernel_size ||
        !geometry->stride || !geometry->dilation)
        return signal_refuse(err, YVEX_ERR_INVALID_ARG,
                                    "Conv1D geometry requires nonzero extents");
    if (!yvex_core_u64_mul(geometry->kernel_size - 1ull, geometry->dilation,
                           &dilated_kernel) ||
        !yvex_core_u64_add(dilated_kernel, 1ull, &dilated_kernel))
        return signal_refuse(err, YVEX_ERR_BOUNDS,
                                    "Conv1D dilated kernel extent overflowed");
    if (geometry->transposed) {
        unsigned long long expanded;
        unsigned long long removed;

        if (geometry->output_padding >= geometry->stride)
            return signal_refuse(err, YVEX_ERR_INVALID_ARG,
                                        "transposed Conv1D output padding exceeds its stride");
        if (!yvex_core_u64_mul(geometry->input_length - 1ull, geometry->stride,
                               &expanded) ||
            !yvex_core_u64_mul(geometry->padding, 2ull, &removed) ||
            !yvex_core_u64_add(expanded, dilated_kernel, &result) ||
            !yvex_core_u64_add(result, geometry->output_padding, &result) || result <= removed)
            return signal_refuse(err, YVEX_ERR_BOUNDS,
                                        "transposed Conv1D output extent is invalid");
        result -= removed;
    } else {
        unsigned long long added;

        if (geometry->output_padding)
            return signal_refuse(err, YVEX_ERR_INVALID_ARG,
                                        "ordinary Conv1D has no output padding");
        if (!yvex_core_u64_mul(geometry->padding, 2ull, &added) ||
            !yvex_core_u64_add(geometry->input_length, added, &padded) ||
            padded < dilated_kernel)
            return signal_refuse(err, YVEX_ERR_BOUNDS,
                                        "Conv1D kernel exceeds padded input extent");
        result = (padded - dilated_kernel) / geometry->stride + 1ull;
    }
    *output_length = result;
    yvex_error_clear(err);
    return YVEX_OK;
}

static int conv1d_extents(const yvex_convolution_1d_geometry *geometry,
                                unsigned long long output_length,
                                unsigned long long *input_elements,
                                unsigned long long *weight_elements,
                                unsigned long long *output_elements, yvex_error *err)
{
    unsigned long long input_rows;
    unsigned long long weight_rows;
    unsigned long long output_rows;

    if (!yvex_core_u64_mul(geometry->batch, geometry->input_channels, &input_rows) ||
        !yvex_core_u64_mul(input_rows, geometry->input_length, input_elements) ||
        !yvex_core_u64_mul(geometry->input_channels, geometry->output_channels,
                           &weight_rows) ||
        !yvex_core_u64_mul(weight_rows, geometry->kernel_size, weight_elements) ||
        !yvex_core_u64_mul(geometry->batch, geometry->output_channels, &output_rows) ||
        !yvex_core_u64_mul(output_rows, output_length, output_elements))
        return signal_refuse(err, YVEX_ERR_BOUNDS,
                                    "Conv1D tensor extent overflowed");
    return YVEX_OK;
}

static int conv1d_scale(const float *weight, unsigned long long base,
                              unsigned long long count, const float *gain,
                              unsigned long long gain_index, float *scale,
                              yvex_error *err)
{
    double squared = 0.0;
    unsigned long long index;

    *scale = 1.0f;
    if (!gain) return YVEX_OK;
    for (index = 0ull; index < count; ++index) {
        double value = weight[base + index];
        squared += value * value;
    }
    if (!isfinite(squared) || squared <= 0.0 || !isfinite(gain[gain_index]))
        return signal_refuse(err, YVEX_ERR_FORMAT,
                                    "Conv1D weight normalization is not finite");
    *scale = gain[gain_index] / sqrtf((float)squared);
    if (!isfinite(*scale))
        return signal_refuse(err, YVEX_ERR_FORMAT,
                                    "Conv1D weight normalization scale is not finite");
    return YVEX_OK;
}

static int conv1d_forward(const yvex_convolution_1d_geometry *geometry,
                                const float *input, const float *weight,
                                const float *bias, const float *gain,
                                unsigned long long output_length, float *output,
                                yvex_error *err)
{
    unsigned long long batch;
    unsigned long long output_channel;

    for (output_channel = 0ull; output_channel < geometry->output_channels;
         ++output_channel) {
        unsigned long long weight_base = output_channel * geometry->input_channels *
                                         geometry->kernel_size;
        float scale;
        int rc = conv1d_scale(
            weight, weight_base, geometry->input_channels * geometry->kernel_size,
            gain, output_channel, &scale, err);
        if (rc != YVEX_OK) return rc;
        for (batch = 0ull; batch < geometry->batch; ++batch) {
            unsigned long long output_position;
            for (output_position = 0ull; output_position < output_length;
                 ++output_position) {
                float sum = bias ? bias[output_channel] : 0.0f;
                unsigned long long input_channel;
                for (input_channel = 0ull; input_channel < geometry->input_channels;
                     ++input_channel) {
                    unsigned long long kernel;
                    for (kernel = 0ull; kernel < geometry->kernel_size; ++kernel) {
                        unsigned long long projected = output_position * geometry->stride +
                                                       kernel * geometry->dilation;
                        unsigned long long input_position;
                        unsigned long long input_index;
                        unsigned long long weight_index;
                        if (projected < geometry->padding) continue;
                        input_position = projected - geometry->padding;
                        if (input_position >= geometry->input_length) continue;
                        input_index = (batch * geometry->input_channels + input_channel) *
                                      geometry->input_length + input_position;
                        weight_index = weight_base + input_channel * geometry->kernel_size +
                                       kernel;
                        sum += input[input_index] * weight[weight_index] * scale;
                    }
                }
                if (!isfinite(sum))
                    return signal_refuse(err, YVEX_ERR_FORMAT,
                                                "Conv1D produced a non-finite value");
                output[(batch * geometry->output_channels + output_channel) * output_length +
                       output_position] = sum;
            }
        }
    }
    return YVEX_OK;
}

static int conv1d_transposed(const yvex_convolution_1d_geometry *geometry,
                                   const float *input, const float *weight,
                                   const float *bias, const float *gain,
                                   unsigned long long output_length, float *output,
                                   yvex_error *err)
{
    unsigned long long output_elements = geometry->batch * geometry->output_channels *
                                         output_length;
    unsigned long long index;
    unsigned long long batch;

    for (index = 0ull; index < output_elements; ++index)
        output[index] = bias ? bias[(index / output_length) % geometry->output_channels] : 0.0f;
    for (batch = 0ull; batch < geometry->batch; ++batch) {
        unsigned long long input_channel;
        for (input_channel = 0ull; input_channel < geometry->input_channels; ++input_channel) {
            unsigned long long weight_base = input_channel * geometry->output_channels *
                                             geometry->kernel_size;
            float scale;
            int rc = conv1d_scale(
                weight, weight_base, geometry->output_channels * geometry->kernel_size,
                gain, input_channel, &scale, err);
            if (rc != YVEX_OK) return rc;
            for (index = 0ull; index < geometry->input_length; ++index) {
                float value = input[(batch * geometry->input_channels + input_channel) *
                                    geometry->input_length + index] * scale;
                unsigned long long output_channel;
                for (output_channel = 0ull; output_channel < geometry->output_channels;
                     ++output_channel) {
                    unsigned long long kernel;
                    for (kernel = 0ull; kernel < geometry->kernel_size; ++kernel) {
                        unsigned long long projected = index * geometry->stride +
                                                       kernel * geometry->dilation;
                        unsigned long long output_position;
                        unsigned long long output_index;
                        unsigned long long weight_index;
                        if (projected < geometry->padding) continue;
                        output_position = projected - geometry->padding;
                        if (output_position >= output_length) continue;
                        output_index = (batch * geometry->output_channels + output_channel) *
                                       output_length + output_position;
                        weight_index = weight_base + output_channel * geometry->kernel_size +
                                       kernel;
                        output[output_index] += value * weight[weight_index];
                    }
                }
            }
        }
    }
    for (index = 0ull; index < output_elements; ++index)
        if (!isfinite(output[index]))
            return signal_refuse(err, YVEX_ERR_FORMAT,
                                        "transposed Conv1D produced a non-finite value");
    return YVEX_OK;
}

int yvex_convolution_1d_f32(const yvex_convolution_1d_geometry *geometry,
                          const float *input, unsigned long long input_count,
                          const float *weight, unsigned long long weight_count,
                          const float *bias, unsigned long long bias_count,
                          const float *gain, unsigned long long gain_count,
                          float *output, unsigned long long output_count,
                          yvex_error *err)
{
    unsigned long long output_length;
    unsigned long long expected_input;
    unsigned long long expected_weight;
    unsigned long long expected_output;
    unsigned long long normalized_channels;
    int rc;

    if (!geometry || !input || !weight || !output)
        return signal_refuse(err, YVEX_ERR_INVALID_ARG,
                                    "Conv1D requires geometry, input, weight, and output");
    rc = yvex_convolution_1d_output_length(geometry, &output_length, err);
    if (rc != YVEX_OK) return rc;
    rc = conv1d_extents(geometry, output_length, &expected_input,
                              &expected_weight, &expected_output, err);
    if (rc != YVEX_OK) return rc;
    normalized_channels = geometry->transposed ? geometry->input_channels :
                                                 geometry->output_channels;
    if (input_count != expected_input || weight_count != expected_weight ||
        output_count != expected_output || (bias && bias_count != geometry->output_channels) ||
        (!bias && bias_count) || (gain && gain_count != normalized_channels) ||
        (!gain && gain_count))
        return signal_refuse(err, YVEX_ERR_BOUNDS,
                                    "Conv1D tensor extents differ from geometry");
    rc = geometry->transposed ?
             conv1d_transposed(geometry, input, weight, bias, gain,
                                     output_length, output, err) :
             conv1d_forward(geometry, input, weight, bias, gain,
                                  output_length, output, err);
    if (rc == YVEX_OK) yvex_error_clear(err);
    return rc;
}

int yvex_signal_alias_snake_f32(const float *input, unsigned long long batch,
                               unsigned long long channels, unsigned long long length,
                               const float *alpha_log, const float *beta_log,
                               const float up_filter[12], const float down_filter[12],
                               float *output, float *scratch,
                               unsigned long long scratch_count, yvex_error *err)
{
    unsigned long long doubled;
    unsigned long long groups;
    unsigned long long total_elements;
    unsigned long long padded_length;
    unsigned long long group;

    if (!input || !batch || !channels || !length || !alpha_log || !beta_log ||
        !up_filter || !down_filter || !output || !scratch ||
        !yvex_core_u64_mul(batch, channels, &groups) ||
        !yvex_core_u64_mul(groups, length, &total_elements) ||
        total_elements > (unsigned long long)SIZE_MAX / sizeof(float) ||
        !yvex_core_u64_mul(length, 2ull, &doubled) ||
        !yvex_core_u64_add(length, 10ull, &padded_length) ||
        padded_length > (~0ull - 15ull) / 2ull || scratch_count < doubled)
        return signal_refuse(err, YVEX_ERR_INVALID_ARG,
                                    "alias-free SnakeBeta requires complete bounded buffers");
    for (group = 0ull; group < groups; ++group) {
        unsigned long long position;
        unsigned long long channel = group % channels;
        float alpha = expf(alpha_log[channel]);
        float beta = expf(beta_log[channel]);

        if (!isfinite(alpha) || !isfinite(beta) || beta <= 0.0f)
            return signal_refuse(err, YVEX_ERR_FORMAT,
                                        "SnakeBeta parameters are not finite");
        for (position = 0ull; position < doubled; ++position) {
            unsigned long long raw_position = position + 15ull;
            float value = 0.0f;
            unsigned long long padded_position;
            for (padded_position = 0ull; padded_position < padded_length;
                 ++padded_position) {
                unsigned long long projected = padded_position * 2ull;
                unsigned long long kernel;
                unsigned long long source_position;
                if (raw_position < projected || raw_position - projected >= 12ull) continue;
                kernel = raw_position - projected;
                source_position = padded_position < 5ull ? 0ull : padded_position - 5ull;
                if (source_position >= length) source_position = length - 1ull;
                value += input[group * length + source_position] * up_filter[kernel];
            }
            value *= 2.0f;
            value += sinf(alpha * value) * sinf(alpha * value) / (beta + 1.0e-9f);
            if (!isfinite(value))
                return signal_refuse(err, YVEX_ERR_FORMAT,
                                            "SnakeBeta produced a non-finite value");
            scratch[position] = value;
        }
        for (position = 0ull; position < length; ++position) {
            float value = 0.0f;
            unsigned long long kernel;
            for (kernel = 0ull; kernel < 12ull; ++kernel) {
                unsigned long long padded = position * 2ull + kernel;
                unsigned long long source_position = padded < 5ull ? 0ull : padded - 5ull;
                if (source_position >= doubled) source_position = doubled - 1ull;
                value += scratch[source_position] * down_filter[kernel];
            }
            if (!isfinite(value))
                return signal_refuse(err, YVEX_ERR_FORMAT,
                                            "SnakeBeta downsampling produced a non-finite value");
            output[group * length + position] = value;
        }
    }
    yvex_error_clear(err);
    return YVEX_OK;
}
