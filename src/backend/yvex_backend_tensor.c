/*
 * yvex_backend_tensor.c - backend tensor boundary facts.
 *
 * Owner:
 *   src/backend
 *
 * Owns:
 *   backend tensor allocation/read/write/bind refusal status.
 *
 * Does not own:
 *   graph execution, quantization, runtime generation, eval, benchmark, or
 *   release claims.
 *
 * Invariants:
 *   tensor boundary facts do not allocate backend memory in this row.
 *
 * Boundary:
 *   backend tensor boundary reporting is not runtime execution.
 */
#include "yvex_backend_tensor.h"

/* Contract: initializes a backend tensor refusal fact without backend calls. */
void yvex_backend_tensor_refuse(yvex_backend_tensor_fact *fact,
                                const char *operation,
                                const char *reason)
{
    if (!fact) return;
    fact->operation = operation ? operation : "backend-tensor";
    fact->status = "unsupported";
    fact->reason = reason ? reason : "backend tensor operation is future-owned";
}
