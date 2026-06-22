/*
 * YVEX - Graph internals
 *
 * File: src/graph/graph_internal.h
 * Layer: graph implementation
 *
 * Purpose:
 *   Shares private graph, memory plan, and planner structures across the F0
 *   implementation files. Public consumers see only opaque handles.
 *
 * Owns:
 *   - concrete yvex_graph storage
 *   - concrete yvex_memory_plan storage
 *   - concrete yvex_plan storage
 *
 * Does not own:
 *   - public API declarations
 *   - backend/session execution
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_graph
 */
#ifndef YVEX_GRAPH_INTERNAL_H
#define YVEX_GRAPH_INTERNAL_H

#include <yvex/yvex.h>

typedef struct {
    unsigned int input_ids[4];
    unsigned int output_ids[4];
} yvex_graph_op_edges;

struct yvex_graph {
    yvex_graph_status status;
    char *architecture;
    char *model_name;
    unsigned long long sequence_length;
    unsigned long long context_length;
    yvex_graph_value_info *values;
    unsigned long long value_count;
    unsigned long long value_cap;
    yvex_graph_op_info *ops;
    yvex_graph_op_edges *edges;
    unsigned long long op_count;
    unsigned long long op_cap;
    yvex_graph_missing_required *missing;
    unsigned long long missing_count;
    unsigned long long missing_cap;
};

struct yvex_memory_plan {
    yvex_memory_plan_status status;
    yvex_memory_plan_summary summary;
};

struct yvex_plan {
    char *backend_name;
    char *backend_status;
    yvex_graph *graph;
    yvex_memory_plan *memory;
};

char *yvex_graph_strdup(const char *text);
void yvex_graph_value_clear(yvex_graph_value_info *value);
void yvex_graph_op_clear(yvex_graph_op_info *op);
void yvex_graph_missing_clear(yvex_graph_missing_required *missing);

int yvex_graph_add_value(yvex_graph *graph,
                         yvex_value_kind kind,
                         const char *name,
                         unsigned int rank,
                         const unsigned long long *dims,
                         yvex_dtype dtype,
                         yvex_residency residency,
                         const char *source_tensor_name,
                         unsigned int *out_id,
                         yvex_error *err);
int yvex_graph_add_op(yvex_graph *graph,
                      yvex_op_kind kind,
                      yvex_op_status status,
                      const char *name,
                      const unsigned int *inputs,
                      unsigned int input_count,
                      const unsigned int *outputs,
                      unsigned int output_count,
                      const char *reason,
                      yvex_error *err);
int yvex_graph_add_missing(yvex_graph *graph,
                           yvex_tensor_role role,
                           const char *reason,
                           yvex_error *err);

const yvex_graph_op_edges *yvex_graph_op_edges_at(const yvex_graph *graph,
                                                  unsigned long long index);

#endif /* YVEX_GRAPH_INTERNAL_H */
