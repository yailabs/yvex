/*
 * yvex_backend_qtype.c - backend qtype refusal facts.
 *
 * Owner:
 *   src/backend
 *
 * Owns:
 *   backend qtype compute/refusal matrix boundary state.
 *
 * Does not own:
 *   GGUF qtype byte geometry, quantization, backend kernels, runtime
 *   generation, eval, benchmark, or release claims.
 *
 * Invariants:
 *   unknown backend/qtype pairs refuse until V010.QUANT.2 closes them.
 *
 * Boundary:
 *   qtype matrix facts do not quantize tensors or execute model operations.
 */
#include "yvex_backend_qtype.h"

/* Contract: initializes backend qtype refusal without compute attempts. */
void yvex_backend_qtype_refuse(yvex_backend_qtype_fact *fact,
                               const char *backend,
                               const char *qtype)
{
    if (!fact) return;
    fact->backend = backend ? backend : "unknown";
    fact->qtype = qtype ? qtype : "unknown";
    fact->compute_status = "unknown";
    fact->reason = "backend qtype compute/refusal matrix is future-owned by V010.QUANT.2";
}
