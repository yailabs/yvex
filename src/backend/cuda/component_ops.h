/* CUDA component operations shared by capability publication, implementations, and oracles. */
#ifndef SRC_BACKEND_CUDA_COMPONENT_OPS_H_INCLUDED
#define SRC_BACKEND_CUDA_COMPONENT_OPS_H_INCLUDED

#include <yvex/internal/component.h>
#include <yvex/internal/convolution.h>
#include <yvex/internal/neural_operations.h>
#include "src/backend/cuda/private.h"

#ifdef __cplusplus
extern "C" {
#endif

/* F32 contractions use graph-rebindable narrow rows or zero-workspace BLAS.
 * Callers own finite-result validation, lifetime and publication. */
#define YVEX_CUDA_F32_INLINE_ROWS 16ull
/* Equivalent ROW arithmetic only. Q8_0 register tiles require the measured
 * SM121 wide population; smaller cases retain the admitted row consumer. */
static inline CUfunction cuda_q8_row_matrix_select(yvex_backend *backend,
    unsigned qtype, unsigned long long rows, unsigned long long width,
    unsigned long long inputs, unsigned *grid, unsigned *block)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    if (!state || !state->q8_row_matrix_function ||
        (qtype != YVEX_GGUF_QTYPE_MXFP4 && qtype != YVEX_GGUF_QTYPE_Q2_K &&
         qtype != YVEX_GGUF_QTYPE_Q8_0) ||
        !yvex_cuda_q8_row_matrix_geometry(rows, width, inputs, grid, block)) return NULL;
    if (qtype == YVEX_GGUF_QTYPE_Q8_0 &&
        (rows < 256ull || width < 2048ull || inputs < 128ull ||
         backend->device_info.compute_capability_major != 12 ||
         backend->device_info.compute_capability_minor != 1)) return NULL;
    if (state->mxfp4_row_matrix_shared_function && qtype == YVEX_GGUF_QTYPE_MXFP4 &&
        rows >= 64ull && inputs >= 64ull && backend->device_info.compute_capability_major == 12 &&
        backend->device_info.compute_capability_minor == 1) {
        *grid = (unsigned)(((rows + 63ull) / 64ull) * ((inputs + 31ull) / 32ull));
        *block = 256u;
        return state->mxfp4_row_matrix_shared_function;
    }
    return state->q8_row_matrix_function;
}

int yvex_cuda_f32_matrix(yvex_backend *, CUdeviceptr, unsigned long long,
    unsigned long long, unsigned long long, CUdeviceptr, CUdeviceptr, int, int, yvex_error *);
int yvex_cuda_bf16_matrix(yvex_cuda_work *, const yvex_backend_attention_weight *,
    CUdeviceptr, unsigned long long, unsigned long long, unsigned long long,
    CUdeviceptr, CUdeviceptr, int, CUdeviceptr, yvex_error *);
int yvex_cuda_prepared_matrix(yvex_cuda_work *, const yvex_backend_attention_weight *,
    CUdeviceptr, unsigned long long, unsigned long long, unsigned long long,
    CUdeviceptr, CUdeviceptr, int, CUdeviceptr, yvex_error *);

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
