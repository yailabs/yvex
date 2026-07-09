/*
 * yvex_graph_bind.h - graph binding boundary facts.
 *
 * Owner:
 *   src/graph
 *
 * Owns:
 *   runtime descriptor tensor-role to graph bind-plan status.
 *
 * Does not own:
 *   graph execution, backend tensor allocation, prefill, KV, decode, logits,
 *   sampling, or generation.
 *
 * Invariants:
 *   graph binding requires runtime descriptor roles and buffer facts.
 *
 * Boundary:
 *   graph binding is not graph execution.
 */
#ifndef YVEX_GRAPH_BIND_H
#define YVEX_GRAPH_BIND_H

typedef struct {
    const char *status;
    const char *missing_role;
    const char *next_row;
} yvex_graph_bind_fact;

void yvex_graph_bind_refuse_missing_role(yvex_graph_bind_fact *fact, const char *role);
int yvex_graph_bind_supported(int runtime_descriptor_ok, const char **reason);

#endif
