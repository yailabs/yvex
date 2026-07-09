/*
 * yvex_graph_execute.h - graph execution boundary.
 *
 * Owner:
 *   src/graph
 *
 * Owns:
 *   future graph execution support/refusal status.
 *
 * Does not own:
 *   prefill, KV, decode, logits, sampling, runtime generation, eval,
 *   benchmark, or release claims.
 *
 * Invariants:
 *   graph execution refuses until runtime graph execution is implemented.
 *
 * Boundary:
 *   primitive and bind-plan facts are not transformer execution.
 */
#ifndef YVEX_GRAPH_EXECUTE_H
#define YVEX_GRAPH_EXECUTE_H

int yvex_graph_execute_supported(const char **reason);

#endif
