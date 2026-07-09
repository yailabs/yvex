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

typedef struct {
    const char *qtype;
    const char *status;
    const char *reason;
} yvex_cuda_qtype_fact;

void yvex_cuda_qtype_refuse(yvex_cuda_qtype_fact *fact, const char *qtype);

#endif
