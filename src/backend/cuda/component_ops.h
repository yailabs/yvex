/* CUDA component operations shared by capability publication, implementations, and oracles. */
#ifndef SRC_BACKEND_CUDA_COMPONENT_OPS_H_INCLUDED
#define SRC_BACKEND_CUDA_COMPONENT_OPS_H_INCLUDED

#include <yvex/internal/component.h>
#include <yvex/internal/convolution.h>
#include <yvex/internal/neural_operations.h>

#ifdef __cplusplus
extern "C" {
#endif

int yvex_cuda_convolution_1d(yvex_backend *, const yvex_convolution_1d_request *,
    yvex_backend_operation_facts *, yvex_error *);
int yvex_cuda_alias_snake(yvex_backend *, const yvex_alias_snake_request *,
    yvex_backend_operation_facts *, yvex_error *);
int yvex_cuda_combine_f32(yvex_backend *, const yvex_device_tensor *const *, size_t, int,
    yvex_device_tensor *, yvex_backend_operation_facts *, yvex_error *);
int yvex_cuda_clamp_f32(yvex_backend *, const yvex_device_tensor *, yvex_device_tensor *, float, float,
    yvex_backend_operation_facts *, yvex_error *);

#ifdef __cplusplus
}
#endif
#endif /* SRC_BACKEND_CUDA_COMPONENT_OPS_H_INCLUDED */
