/*
 * yvex_backend_qtype.c - backend qtype numeric-capability projections.
 *
 * Owner:
 *   src/backend
 *
 * Owns:
 *   backend-specific projection of canonical qtype compute/refusal facts.
 *
 * Does not own:
 *   GGUF qtype byte geometry, codec truth, backend kernels, runtime
 *   generation, eval, benchmark, or release claims.
 *
 * Invariants:
 *   no backend may promote a capability absent from TRACK.QUANT registry.
 *
 * Boundary:
 *   qtype matrix facts do not quantize tensors or execute model operations.
 */
#include "yvex_backend_qtype.h"

#include "../gguf/yvex_quant_numeric.h"

#include <string.h>

/* Projects immutable numeric truth without probing hardware or launching work. */
void yvex_backend_qtype_refuse(yvex_backend_qtype_fact *fact,
                               const char *backend,
                               const char *qtype)
{
    const yvex_quant_numeric_capability *capability;
    int available = 0;

    if (!fact) return;
    fact->backend = backend ? backend : "unknown";
    fact->qtype = qtype ? qtype : "unknown";
    capability = yvex_quant_numeric_capability_by_name(qtype);
    if (!capability) {
        fact->compute_status = "unknown-qtype";
        fact->reason = "unknown-qtype";
        return;
    }
    if (backend && strcmp(backend, "cpu") == 0)
        available = capability->dedicated_cpu_compute_available;
    else if (backend && strcmp(backend, "cuda") == 0)
        available = capability->dedicated_cuda_compute_available;
    else {
        fact->compute_status = "unsupported-backend";
        fact->reason = "backend-not-covered-by-numeric-contract";
        return;
    }
    fact->compute_status = available ? "available" : "unavailable";
    fact->reason = available
        ? "dedicated-encoded-row-dot-v1"
        : yvex_quant_refusal_name(
              capability->identity_known && capability->storage_admitted
                  ? backend && strcmp(backend, "cuda") == 0
                      ? YVEX_QUANT_REFUSAL_CUDA_COMPUTE_UNAVAILABLE
                      : YVEX_QUANT_REFUSAL_CPU_COMPUTE_UNAVAILABLE
                  : capability->refusal);
}
