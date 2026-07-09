/*
 * yvex_graph_bind.c - graph bind-plan facts.
 *
 * Owner:
 *   src/graph
 *
 * Owns:
 *   runtime descriptor tensor-role to graph binding blockers.
 *
 * Does not own:
 *   graph execution, backend execution, prefill, KV, decode, logits, sampling,
 *   generation, eval, benchmark, or release claims.
 *
 * Invariants:
 *   missing role and missing buffer binding states refuse explicitly.
 *
 * Boundary:
 *   graph bind-plan facts do not execute transformer graph work.
 */
#include "yvex_graph_bind.h"

/* Contract: initializes a graph bind refusal for a missing runtime role. */
void yvex_graph_bind_refuse_missing_role(yvex_graph_bind_fact *fact, const char *role)
{
    if (!fact) return;
    fact->status = "blocked";
    fact->missing_role = role ? role : "unknown";
    fact->next_row = "V010.GRAPH.24";
}

/* Contract: reports graph binding support state from descriptor evidence. */
int yvex_graph_bind_supported(int runtime_descriptor_ok, const char **reason)
{
    if (runtime_descriptor_ok) {
        if (reason) *reason = "runtime descriptor present for graph bind planning";
        return 1;
    }
    if (reason) *reason = "graph binding blocked by missing runtime descriptor";
    return 0;
}
