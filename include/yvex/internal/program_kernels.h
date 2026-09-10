/* Prepared parameter/linear implementations for physical tensor operations. */
#ifndef INCLUDE_YVEX_INTERNAL_PROGRAM_KERNELS_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_PROGRAM_KERNELS_H_INCLUDED
#include <yvex/internal/program_device.h>
#include <yvex/internal/component.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct yvex_program_kernels yvex_program_kernels;
typedef struct {
    unsigned long long tensor_id;
    yvex_component_encoded_weight weight;
} yvex_program_kernel_parameter;
int yvex_program_kernels_open(yvex_program_kernels **, const yvex_program_physical *,
    const yvex_program_kernel_parameter *, size_t parameter_count, yvex_backend *, unsigned long long host_limit,
    unsigned long long device_limit, yvex_error *);
int yvex_program_kernels_prepare(yvex_program_kernels *, unsigned long long rows,
    unsigned long long host_limit, unsigned long long device_limit, yvex_error *);
int yvex_program_kernels_invoke(yvex_program_kernels *, const yvex_program_device_invocation *,
    yvex_backend_operation_facts *, yvex_error *);
const yvex_device_tensor *yvex_program_kernels_small_weight(const yvex_program_kernels *, yvex_ir_id);
void yvex_program_kernels_resources(const yvex_program_kernels *, unsigned long long *host, unsigned long long *device);
int yvex_program_kernels_close(yvex_program_kernels **, yvex_error *);
#ifdef __cplusplus
}
#endif
#endif
