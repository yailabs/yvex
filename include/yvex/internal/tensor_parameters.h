/* Generic physical-program parameter binding over an admitted tensor source. */
#ifndef INCLUDE_YVEX_INTERNAL_TENSOR_PARAMETERS_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_TENSOR_PARAMETERS_H_INCLUDED

#include <yvex/internal/program_kernels.h>
#include <yvex/internal/tensor_source.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct yvex_tensor_parameters yvex_tensor_parameters;
int yvex_tensor_parameters_open(yvex_tensor_parameters **, const yvex_tensor_source *,
    const yvex_program_physical *, size_t parameter_count,
    int (*parameter_name)(void *, unsigned long long, char[256], yvex_error *),
    void *parameter_context, yvex_error *);
const yvex_program_kernel_parameter *yvex_tensor_parameters_view(const yvex_tensor_parameters *, size_t *);
unsigned long long yvex_tensor_parameters_encoded_bytes(const yvex_tensor_parameters *);
void yvex_tensor_parameters_close(yvex_tensor_parameters **);

#ifdef __cplusplus
}
#endif
#endif
