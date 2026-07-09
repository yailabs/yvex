/*
 * cuda_qtype.c - CUDA qtype refusal facts.
 *
 * Owner:
 *   src/backend/cuda
 *
 * Owns:
 *   CUDA qtype support/refusal state.
 *
 * Does not own:
 *   GGUF qtype byte geometry, quantization, full transformer graph execution,
 *   runtime generation, eval, benchmark, or release claims.
 *
 * Invariants:
 *   qtype compute support must be proven by dedicated CUDA rows.
 *
 * Boundary:
 *   CUDA qtype facts do not make CUDA runtime generation available.
 */
#include "cuda_qtype.h"

/* Contract: initializes CUDA qtype refusal without launching kernels. */
void yvex_cuda_qtype_refuse(yvex_cuda_qtype_fact *fact, const char *qtype)
{
    if (!fact) return;
    fact->qtype = qtype ? qtype : "unknown";
    fact->status = "unknown";
    fact->reason = "CUDA qtype compute/refusal matrix is future-owned by V010.QUANT.2";
}
