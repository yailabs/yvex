/*
 * yvex_runtime_descriptor_report.c - runtime descriptor report facts.
 *
 * Owner:
 *   src/model
 *
 * Owns:
 *   missing runtime descriptor blocker facts for report construction.
 *
 * Does not own:
 *   CLI rendering, graph binding, graph execution, runtime generation, eval,
 *   benchmark, or release claims.
 *
 * Invariants:
 *   report construction does not mutate runtime state.
 *
 * Boundary:
 *   descriptor reports are not graph execution.
 */
#include "yvex_runtime_descriptor_report.h"

/* Contract: initializes a runtime descriptor report fact without allocation. */
void yvex_runtime_descriptor_report_init(yvex_runtime_descriptor_report_fact *fact,
                                         const char *status,
                                         const char *blocker,
                                         const char *next_row)
{
    if (!fact) return;
    fact->status = status ? status : "unsupported";
    fact->blocker = blocker ? blocker : "runtime descriptor projection not implemented";
    fact->next_row = next_row ? next_row : "V010.RUNTIME.DESCRIPTOR.GGUF.0";
}
