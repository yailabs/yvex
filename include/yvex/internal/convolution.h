/* Admitted convolution, spatial normalization and elementwise backend work. */
#ifndef INCLUDE_YVEX_INTERNAL_CONVOLUTION_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_CONVOLUTION_H_INCLUDED

#include <yvex/core.h>
#include <yvex/internal/neural_operations.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct yvex_backend yvex_backend;
typedef struct yvex_device_tensor yvex_device_tensor;
typedef struct yvex_component_encoded_weight yvex_convolution_weight;
typedef enum {
    YVEX_CONVOLUTION_PADDING_ZERO = 0,
    YVEX_CONVOLUTION_PADDING_REFLECT = 1
} yvex_convolution_padding;

typedef struct yvex_convolution_2d_geometry {
    unsigned long long batch, input_channels, output_channels;
    unsigned long long input_height, input_width;
    unsigned long long kernel_height, kernel_width;
    unsigned long long stride_height, stride_width;
    unsigned long long padding_top, padding_bottom, padding_left, padding_right;
    unsigned long long weight_temporal_extent, weight_temporal_index;
    yvex_convolution_padding padding;
} yvex_convolution_2d_geometry;

typedef struct {
    unsigned long long kernel_launches, output_height, output_width, output_values;
    int complete;
} yvex_convolution_cuda_result;

int yvex_backend_conv2d_f32(
    yvex_backend *backend, const yvex_convolution_2d_geometry *geometry,
    const yvex_device_tensor *input, const yvex_convolution_weight *weight,
    const yvex_convolution_weight *bias, yvex_device_tensor *output,
    yvex_convolution_cuda_result *result, yvex_error *err);
int yvex_backend_group_norm_silu_f32(
    yvex_backend *backend, const yvex_device_tensor *input,
    const yvex_convolution_weight *weight, const yvex_convolution_weight *bias,
    unsigned long long batch, unsigned long long channels,
    unsigned long long height, unsigned long long width,
    unsigned long long groups, float epsilon, yvex_device_tensor *output,
    yvex_convolution_cuda_result *result, yvex_error *err);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_YVEX_INTERNAL_CONVOLUTION_H_INCLUDED */
