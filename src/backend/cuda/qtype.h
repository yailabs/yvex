/*
 * cuda_qtype.h - CUDA qtype support facts.
 *
 * Owner:
 *   src/backend/cuda
 *
 * Owns:
 *   CUDA qtype support/refusal records.
 *
 * Does not own:
 *   GGUF qtype byte geometry, quantization kernels for full runtime,
 *   transformer graph execution, runtime generation, eval, benchmark, or
 *   release claims.
 *
 * Invariants:
 *   CUDA qtype support remains explicit and fail-closed.
 *
 * Boundary:
 *   CUDA qtype facts are not CUDA generation support.
 */
#ifndef YVEX_CUDA_QTYPE_H
#define YVEX_CUDA_QTYPE_H

#include "src/gguf/quant_numeric.h"

#include <yvex/backend.h>

typedef struct {
    const char *qtype;
    const char *status;
    const char *reason;
} yvex_cuda_qtype_fact;

void yvex_cuda_qtype_refuse(yvex_cuda_qtype_fact *fact, const char *qtype);

int yvex_cuda_quant_row_dot(yvex_backend *backend,
                            unsigned int qtype,
                            const unsigned char *encoded,
                            size_t encoded_bytes,
                            const float *vector,
                            unsigned long long elements,
                            float *out,
                            yvex_quant_failure *failure,
                            yvex_error *err);

#endif
