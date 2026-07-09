/*
 * yvex_graph_execute.c - graph execution refusal facts.
 *
 * Owner:
 *   src/graph
 *
 * Owns:
 *   future graph execution boundary and explicit unsupported status.
 *
 * Does not own:
 *   prefill, KV, decode, logits, sampling, runtime generation, eval,
 *   benchmark, or release claims.
 *
 * Invariants:
 *   this file does not execute transformer graph work in the target seed row.
 *
 * Boundary:
 *   graph execution remains future-owned.
 */
#include "yvex_graph_execute.h"

/* Contract: reports graph execution support state without executing graph work. */
int yvex_graph_execute_supported(const char **reason)
{
    if (reason) *reason = "transformer graph execution is future-owned by V010.GRAPH.24";
    return 0;
}
