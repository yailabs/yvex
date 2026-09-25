/* Exceptional decoded row-dot recovery shared by CUDA kernel families. */
#ifndef SRC_BACKEND_CUDA_DOT_RECOVERY_H_INCLUDED
#define SRC_BACKEND_CUDA_DOT_RECOVERY_H_INCLUDED

#include "src/backend/cuda/kernel_primitives.h"

#ifdef __CUDACC__
static __device__ float qtype_dot_recover_f64(
    const unsigned char *row, const float *input, unsigned long long width,
    unsigned int qtype)
{
    double recovered = 0.0;
    for (unsigned long long i = 0ull; i < width; ++i)
        recovered += (double)qtype_value(row, i, qtype) * (double)input[i];
    return (float)recovered;
}

static __device__ float qtype_q8_dot_recover_f64(
    const unsigned char *row, const unsigned char *input,
    unsigned long long blocks, unsigned long long weight_block,
    unsigned int qtype, int *status)
{
    double recovered = 0.0;
    for (unsigned long long block = 0ull; block < blocks; ++block) {
        const unsigned char *weight = row + block * weight_block;
        const unsigned char *q8 = input + block * YVEX_CUDA_Q8_K_BYTES;
        float scale = __uint_as_float(qtype_load_u32(q8));
        if (!isfinite(scale)) {
            atomicCAS(status, 0, 1);
            return 0.0f;
        }
        for (unsigned int i = 0u; i < YVEX_CUDA_Q8_K_BLOCK; ++i) {
            float decoded = qtype_value(weight, i, qtype);
            if (!isfinite(decoded)) {
                atomicCAS(status, 0, 1);
                return 0.0f;
            }
            int quantized = (int)(signed char)q8[4u + i];
            recovered += (double)decoded * (double)scale * (double)quantized;
        }
    }
    return (float)recovered;
}
#endif

#endif
