/* Execute admitted signal/spatial operations; program composition belongs to the compiler. */
#include "src/backend/cuda/private.h"
#include "src/backend/cuda/component_ops.h"

#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <yvex/internal/convolution.h>
#include <yvex/qtype.h>

#define CONVOLUTION_BLOCK 256u

static int decoder_refuse(yvex_error *err, yvex_status status,
                          const char *stage, const char *message)
{
    yvex_error_set(err, status, stage, message);
    return status;
}


static int convolution_grid(unsigned long long tasks, unsigned int *grid,
                            const char *stage, yvex_error *err)
{
    if (!tasks || tasks > (unsigned long long)UINT_MAX * CONVOLUTION_BLOCK) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, stage,
                       "convolution launch geometry exceeds the CUDA grid bound");
        return YVEX_ERR_BOUNDS;
    }
    *grid = (unsigned int)((tasks + CONVOLUTION_BLOCK - 1ull) / CONVOLUTION_BLOCK);
    return YVEX_OK;
}

static int convolution_launch(yvex_backend *backend, CUfunction function,
                              unsigned int grid, unsigned int block, void **parameters,
                              const char *stage, yvex_error *err)
{
    int rc = yvex_cuda_launch(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
                              function, grid, block, 0u, parameters, stage, err);
    if (rc == YVEX_OK)
        rc = yvex_cuda_synchronize(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
                                   stage, err);
    return rc;
}

static int convolution_weight_address(
    yvex_backend *backend, const yvex_convolution_weight *weight,
    unsigned long long expected_bytes, CUdeviceptr *address,
    const char *stage, yvex_error *err)
{
    if (!weight || !weight->encoded || weight->qtype != YVEX_GGUF_QTYPE_F32 ||
        weight->encoded_bytes != expected_bytes ||
        yvex_backend_resident_resolve(backend, weight->encoded,
                                      weight->encoded_bytes, address) !=
            YVEX_BACKEND_RESIDENT_HIT) {
        yvex_error_set(err, YVEX_ERR_FORMAT, stage,
                       "convolution requires an exact resident F32 weight");
        return YVEX_ERR_FORMAT;
    }
    return YVEX_OK;
}

static int signal_cuda_weights(yvex_backend *backend,
    const yvex_component_encoded_weight *const *weights, size_t count,
    const yvex_device_tensor *output, const yvex_device_tensor *workspace,
    CUdeviceptr *addresses, yvex_error *err)
{
    for (size_t i = 0u; i < count; ++i) {
        addresses[i] = 0u;
        if (!weights[i]) continue;
        int rc = convolution_weight_address(backend, weights[i], weights[i]->encoded_bytes,
            addresses + i, "cuda.signal.parameter", err);
        if (rc != YVEX_OK) return rc;
        CUdeviceptr start = addresses[i];
        unsigned long long bytes = weights[i]->encoded_bytes;
        const yvex_device_tensor *writable[] = {output, workspace};
        for (size_t j = 0u; j < 2u; ++j) {
            const yvex_device_tensor *t = writable[j];
            if (!t) continue;
            CUdeviceptr at = yvex_cuda_tensor_ptr(t);
            if (bytes > UINT64_MAX - start || t->bytes > UINT64_MAX - at ||
                (start < at + t->bytes && at < start + bytes))
                return decoder_refuse(err, YVEX_ERR_FORMAT, "cuda.signal.parameter",
                    "immutable signal parameter aliases writable storage");
        }
    }
    return YVEX_OK;
}

int yvex_cuda_convolution_1d(yvex_backend *backend, const yvex_convolution_1d_request *r,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    int rc = yvex_convolution_1d_admit(backend, r, facts, err);
    if (rc != YVEX_OK) return rc;
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    yvex_convolution_1d_geometry g = r->geometry;
    unsigned long long length = r->output_length, tasks = r->output->bytes / sizeof(float);
    CUdeviceptr parameters_address[3];
    rc = signal_cuda_weights(backend,
        (const yvex_component_encoded_weight *[]){r->weight, r->gain, r->bias}, 3u,
        r->output, r->gain ? r->workspace : NULL, parameters_address, err);
    if (rc != YVEX_OK) return rc;
    CUdeviceptr weight = parameters_address[0], gain = parameters_address[1], bias = parameters_address[2];
    CUdeviceptr input = yvex_cuda_tensor_ptr(r->input), output = yvex_cuda_tensor_ptr(r->output);
    CUdeviceptr scale = r->gain ? yvex_cuda_tensor_ptr(r->workspace) : 0u;
    unsigned int grid;
    if (r->gain) {
        unsigned long long rows = g.transposed ? g.input_channels : g.output_channels;
        unsigned long long width = r->weight->encoded_bytes / sizeof(float) / rows;
        void *args[] = {&weight, &gain, &scale, &rows, &width};
        rc = convolution_grid(rows, &grid, "cuda.signal.scale", err);
        if (rc == YVEX_OK) rc = convolution_launch(backend, state ? state->conv_scale_function : NULL,
            grid, CONVOLUTION_BLOCK, args, "cuda.signal.scale", err);
        if (rc == YVEX_OK) facts->kernel_launches++;
    }
    if (rc == YVEX_OK) {
        void *args[] = {&input, &weight, &bias, &scale, &output, &g.batch, &g.input_channels,
            &g.output_channels, &g.input_length, &length, &g.kernel_size, &g.stride, &g.dilation, &g.padding};
        rc = convolution_grid(tasks, &grid, "cuda.signal.convolution", err);
        CUfunction kernel = !state ? NULL : g.transposed ? state->conv1d_transposed_function : state->conv1d_function;
        if (rc == YVEX_OK) rc = convolution_launch(backend, kernel, grid, CONVOLUTION_BLOCK,
            args, "cuda.signal.convolution", err);
        if (rc == YVEX_OK) facts->kernel_launches++;
    }
    if (rc == YVEX_OK) r->output->is_written = 1;
    return rc;
}

int yvex_cuda_alias_snake(yvex_backend *backend, const yvex_alias_snake_request *r,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    int rc = yvex_alias_snake_admit(backend, r, facts, err);
    if (rc != YVEX_OK) return rc;
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    CUdeviceptr addresses[4];
    rc = signal_cuda_weights(backend,
        (const yvex_component_encoded_weight *[]){r->alpha, r->beta, r->up_filter, r->down_filter},
        4u, r->output, r->workspace, addresses, err);
    if (rc != YVEX_OK) return rc;
    CUdeviceptr input = yvex_cuda_tensor_ptr(r->input), output = yvex_cuda_tensor_ptr(r->output);
    CUdeviceptr scratch = yvex_cuda_tensor_ptr(r->workspace);
    unsigned long long groups = r->batch * r->channels, channels = r->channels, length = r->length;
    unsigned int grid;
    void *up_args[] = {&input, &addresses[0], &addresses[1], &addresses[2], &scratch, &groups, &channels, &length};
    rc = convolution_grid(r->workspace->bytes / sizeof(float), &grid, "cuda.signal.alias-up", err);
    if (rc == YVEX_OK) rc = convolution_launch(backend, state ? state->alias_up_function : NULL,
        grid, CONVOLUTION_BLOCK, up_args, "cuda.signal.alias-up", err);
    if (rc == YVEX_OK) {
        facts->kernel_launches++;
        void *down_args[] = {&scratch, &addresses[3], &output, &groups, &length};
        rc = convolution_grid(r->output->bytes / sizeof(float), &grid, "cuda.signal.alias-down", err);
        if (rc == YVEX_OK) rc = convolution_launch(backend, state ? state->alias_down_function : NULL,
            grid, CONVOLUTION_BLOCK, down_args, "cuda.signal.alias-down", err);
        if (rc == YVEX_OK) facts->kernel_launches++;
    }
    if (rc == YVEX_OK) r->output->is_written = 1;
    return rc;
}

int yvex_cuda_combine_f32(yvex_backend *backend, const yvex_device_tensor *const *inputs,
    size_t count, int mean, yvex_device_tensor *output, yvex_backend_operation_facts *facts, yvex_error *err)
{
    int rc = yvex_neural_elementwise_admit(backend, inputs, count, output, facts, err);
    if (rc != YVEX_OK) return rc;
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    unsigned long long values = output->bytes / sizeof(float);
    unsigned int grid;
    rc = convolution_grid(values, &grid, "cuda.combine", err);
    if (rc == YVEX_OK) rc = yvex_backend_tensor_copy(backend, output, inputs[0], err);
    if (rc == YVEX_OK) facts->kernel_launches++;
    output->is_written = 0;
    CUdeviceptr destination = yvex_cuda_tensor_ptr(output);
    for (size_t i = 1u; rc == YVEX_OK && i < count + (mean ? 1u : 0u); ++i) {
        CUdeviceptr source = i < count ? yvex_cuda_tensor_ptr(inputs[i]) : destination;
        float destination_scale = i < count ? 1.0f : 1.0f / (float)count;
        float source_scale = i < count ? 1.0f : 0.0f;
        void *args[] = {&destination, &source, &values, &destination_scale, &source_scale};
        rc = convolution_launch(backend, state ? state->vector_update_function : NULL,
            grid, CONVOLUTION_BLOCK, args, "cuda.combine", err);
        if (rc == YVEX_OK) facts->kernel_launches++;
    }
    if (rc == YVEX_OK) output->is_written = 1;
    return rc;
}

int yvex_cuda_clamp_f32(yvex_backend *backend, const yvex_device_tensor *input,
    yvex_device_tensor *output, float lower, float upper, yvex_backend_operation_facts *facts, yvex_error *err)
{
    if (!isfinite(lower) || !isfinite(upper) || lower > upper)
        return decoder_refuse(err, YVEX_ERR_FORMAT, "cuda.clamp", "finite ordered clamp bounds required");
    int rc = yvex_neural_elementwise_admit(backend, &input, 1u, output, facts, err);
    if (rc != YVEX_OK) return rc;
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    unsigned long long values = output->bytes / sizeof(float);
    unsigned int grid;
    rc = convolution_grid(values, &grid, "cuda.clamp", err);
    if (rc == YVEX_OK) rc = yvex_backend_tensor_copy(backend, output, input, err);
    if (rc == YVEX_OK) facts->kernel_launches++;
    output->is_written = 0;
    if (rc == YVEX_OK) {
        CUdeviceptr address = yvex_cuda_tensor_ptr(output);
        void *args[] = {&address, &values, &lower, &upper};
        rc = convolution_launch(backend, state ? state->clamp_function : NULL,
            grid, CONVOLUTION_BLOCK, args, "cuda.clamp", err);
        if (rc == YVEX_OK) facts->kernel_launches++;
    }
    if (rc == YVEX_OK) output->is_written = 1;
    return rc;
}

int yvex_backend_conv2d_f32(
    yvex_backend *backend, const yvex_convolution_2d_geometry *geometry,
    const yvex_device_tensor *input, const yvex_convolution_weight *weight,
    const yvex_convolution_weight *bias, yvex_device_tensor *output,
    yvex_convolution_cuda_result *result, yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    unsigned long long padded_height, padded_width, output_height, output_width;
    unsigned long long input_values, output_values, weight_values, bytes;
    CUdeviceptr input_address, weight_address = 0, bias_address = 0, output_address;
    unsigned int grid;
    int reflect_padding, rc;
    if (result) memset(result, 0, sizeof(*result));
    if (!backend || !geometry || !input || !weight || !output || !result || !state ||
        yvex_backend_kind_of(backend) != YVEX_BACKEND_KIND_CUDA ||
        !yvex_backend_tensor_owned_by(backend, input) ||
        !yvex_backend_tensor_owned_by(backend, output) ||
        !geometry->batch || !geometry->input_channels || !geometry->output_channels ||
        !geometry->input_height || !geometry->input_width || !geometry->kernel_height ||
        !geometry->kernel_width || !geometry->stride_height || !geometry->stride_width ||
        !geometry->weight_temporal_extent ||
        geometry->weight_temporal_index >= geometry->weight_temporal_extent ||
        geometry->padding > YVEX_CONVOLUTION_PADDING_REFLECT ||
        !yvex_core_u64_add(geometry->input_height, geometry->padding_top,
                           &padded_height) ||
        !yvex_core_u64_add(padded_height, geometry->padding_bottom,
                           &padded_height) ||
        !yvex_core_u64_add(geometry->input_width, geometry->padding_left,
                           &padded_width) ||
        !yvex_core_u64_add(padded_width, geometry->padding_right, &padded_width) ||
        padded_height < geometry->kernel_height ||
        padded_width < geometry->kernel_width) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "cuda.conv2d",
                       "one complete bounded Conv2D request is required");
        return YVEX_ERR_INVALID_ARG;
    }
    output_height = (padded_height - geometry->kernel_height) /
                        geometry->stride_height + 1ull;
    output_width = (padded_width - geometry->kernel_width) /
                       geometry->stride_width + 1ull;
    if (!yvex_core_u64_mul(geometry->batch, geometry->input_channels, &input_values) ||
        !yvex_core_u64_mul(input_values, geometry->input_height, &input_values) ||
        !yvex_core_u64_mul(input_values, geometry->input_width, &input_values) ||
        !yvex_core_u64_mul(geometry->batch, geometry->output_channels, &output_values) ||
        !yvex_core_u64_mul(output_values, output_height, &output_values) ||
        !yvex_core_u64_mul(output_values, output_width, &output_values) ||
        !yvex_core_u64_mul(geometry->output_channels, geometry->input_channels,
                           &weight_values) ||
        !yvex_core_u64_mul(weight_values, geometry->weight_temporal_extent,
                           &weight_values) ||
        !yvex_core_u64_mul(weight_values, geometry->kernel_height, &weight_values) ||
        !yvex_core_u64_mul(weight_values, geometry->kernel_width, &weight_values) ||
        !yvex_core_u64_mul(input_values, sizeof(float), &bytes) ||
        input->bytes != bytes ||
        !yvex_core_u64_mul(output_values, sizeof(float), &bytes) ||
        output->bytes != bytes ||
        !yvex_core_u64_mul(weight_values, sizeof(float), &bytes)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "cuda.conv2d.geometry",
                       "Conv2D tensor geometry overflowed or differs from its device tensors");
        return YVEX_ERR_BOUNDS;
    }
    rc = convolution_weight_address(backend, weight, bytes, &weight_address,
                                    "cuda.conv2d.weight", err);
    if (rc == YVEX_OK && bias) {
        if (!yvex_core_u64_mul(geometry->output_channels, sizeof(float), &bytes))
            rc = YVEX_ERR_BOUNDS;
        else
            rc = convolution_weight_address(backend, bias, bytes, &bias_address,
                                            "cuda.conv2d.bias", err);
    }
    if (rc != YVEX_OK) return rc;
    input_address = yvex_cuda_tensor_ptr(input);
    output_address = yvex_cuda_tensor_ptr(output);
    reflect_padding = geometry->padding == YVEX_CONVOLUTION_PADDING_REFLECT;
    rc = convolution_grid(output_values, &grid, "cuda.conv2d.launch", err);
    if (rc == YVEX_OK) {
        void *parameters[] = {
            &input_address, &weight_address, &bias_address, &output_address,
            (void *)&geometry->batch, (void *)&geometry->input_channels,
            (void *)&geometry->output_channels, (void *)&geometry->input_height,
            (void *)&geometry->input_width, &output_height, &output_width,
            (void *)&geometry->kernel_height, (void *)&geometry->kernel_width,
            (void *)&geometry->stride_height, (void *)&geometry->stride_width,
            (void *)&geometry->padding_top, (void *)&geometry->padding_left,
            (void *)&geometry->weight_temporal_extent,
            (void *)&geometry->weight_temporal_index, &reflect_padding};
        rc = convolution_launch(backend, state->conv2d_function, grid,
                                CONVOLUTION_BLOCK, parameters, "cuda.conv2d", err);
    }
    if (rc == YVEX_OK) {
        output->is_written = 1;
        result->kernel_launches = 1ull;
        result->output_height = output_height;
        result->output_width = output_width;
        result->output_values = output_values;
        result->complete = 1;
        yvex_error_clear(err);
    }
    return rc;
}

int yvex_backend_group_norm_silu_f32(
    yvex_backend *backend, const yvex_device_tensor *input,
    const yvex_convolution_weight *weight, const yvex_convolution_weight *bias,
    unsigned long long batch, unsigned long long channels,
    unsigned long long height, unsigned long long width,
    unsigned long long groups, float epsilon, yvex_device_tensor *output,
    yvex_convolution_cuda_result *result, yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    unsigned long long values, bytes, parameter_bytes, tasks;
    CUdeviceptr input_address, weight_address = 0, bias_address = 0, output_address;
    unsigned int grid;
    int rc;
    if (result) memset(result, 0, sizeof(*result));
    if (!backend || !input || !weight || !bias || !output || !result || !state ||
        !batch || !channels || !height || !width || !groups || channels % groups ||
        epsilon <= 0.0f || !yvex_backend_tensor_owned_by(backend, input) ||
        !yvex_backend_tensor_owned_by(backend, output) ||
        !yvex_core_u64_mul(batch, channels, &values) ||
        !yvex_core_u64_mul(values, height, &values) ||
        !yvex_core_u64_mul(values, width, &values) ||
        !yvex_core_u64_mul(values, sizeof(float), &bytes) ||
        input->bytes != bytes || output->bytes != bytes ||
        !yvex_core_u64_mul(channels, sizeof(float), &parameter_bytes)) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "cuda.group-norm-silu",
                       "one complete isolated GroupNorm+SiLU request is required");
        return YVEX_ERR_INVALID_ARG;
    }
    rc = convolution_weight_address(backend, weight, parameter_bytes, &weight_address,
                                    "cuda.group-norm-silu.weight", err);
    if (rc == YVEX_OK)
        rc = convolution_weight_address(backend, bias, parameter_bytes, &bias_address,
                                        "cuda.group-norm-silu.bias", err);
    if (rc != YVEX_OK) return rc;
    input_address = yvex_cuda_tensor_ptr(input);
    output_address = yvex_cuda_tensor_ptr(output);
    tasks = batch * groups;
    rc = convolution_grid(tasks, &grid, "cuda.group-norm-silu.launch", err);
    if (rc == YVEX_OK) {
        void *parameters[] = {&input_address, &weight_address, &bias_address,
                              &output_address, &batch, &channels, &height, &width,
                              &groups, &epsilon};
        rc = convolution_launch(backend, state->group_norm_silu_function, grid,
                                CONVOLUTION_BLOCK, parameters,
                                "cuda.group-norm-silu", err);
    }
    if (rc == YVEX_OK) {
        output->is_written = 1;
        result->kernel_launches = 1ull;
        result->output_height = height;
        result->output_width = width;
        result->output_values = values;
        result->complete = 1;
        yvex_error_clear(err);
    }
    return rc;
}
