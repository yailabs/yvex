/*
 * yvex_backend_report.c - typed backend report facts.
 *
 * Owner:
 *   src/backend
 *
 * Owns:
 *   backend capability/qtype report construction records.
 *
 * Does not own:
 *   CLI rendering, graph execution, quantization, runtime generation, eval,
 *   benchmark, or release claims.
 *
 * Invariants:
 *   report construction does not open devices or execute kernels.
 *
 * Boundary:
 *   backend reports do not promote backend runtime readiness.
 */
#include "yvex_backend_report.h"

/* Contract: initializes a backend report fact without backend side effects. */
void yvex_backend_report_fact_init(yvex_backend_report_fact *fact,
                                   const char *kind,
                                   const char *status,
                                   const char *reason,
                                   const char *next_row)
{
    if (!fact) return;
    fact->kind = kind ? kind : "backend";
    fact->status = status ? status : "unsupported";
    fact->reason = reason ? reason : "backend capability is future-owned";
    fact->next_row = next_row ? next_row : "V010.QUANT.2";
}
