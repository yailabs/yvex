/*
 * yvex_backend_qtype.h - backend qtype support facts.
 *
 * Owner:
 *   src/backend
 *
 * Owns:
 *   backend qtype compute/refusal boundary records.
 *
 * Does not own:
 *   GGUF qtype byte geometry, quantization kernels, runtime generation, eval,
 *   benchmark, or release claims.
 *
 * Invariants:
 *   qtype compute support must be explicit per backend.
 *
 * Boundary:
 *   qtype refusal facts are not quantization or generation support.
 */
#ifndef YVEX_BACKEND_QTYPE_H
#define YVEX_BACKEND_QTYPE_H

typedef struct {
    const char *backend;
    const char *qtype;
    const char *compute_status;
    const char *reason;
} yvex_backend_qtype_fact;

void yvex_backend_qtype_refuse(yvex_backend_qtype_fact *fact,
                               const char *backend,
                               const char *qtype);

#endif
