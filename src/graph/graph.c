/*
 * YVEX - Graph object
 *
 * File: src/graph/graph.c
 * Layer: graph implementation
 *
 * Purpose:
 *   Implements lifecycle and inspection helpers for F0 graph objects. Graphs
 *   are owned planning artifacts built by builder.c and never execute ops.
 *
 * Implements:
 *   - yvex_graph_close
 *   - yvex_graph_status_of
 *   - yvex_graph_* accessors
 *
 * Invariants:
 *   - graph accessors tolerate null handles
 *   - graph close frees copied strings and owned arrays
 *   - graph status names are stable CLI/test strings
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_graph
 */
#include "graph_internal.h"

#include <stdlib.h>

void yvex_graph_close(yvex_graph *graph)
{
    unsigned long long i;

    if (!graph) {
        return;
    }
    free(graph->architecture);
    free(graph->model_name);
    for (i = 0; i < graph->value_count; ++i) {
        yvex_graph_value_clear(&graph->values[i]);
    }
    for (i = 0; i < graph->op_count; ++i) {
        yvex_graph_op_clear(&graph->ops[i]);
    }
    for (i = 0; i < graph->missing_count; ++i) {
        yvex_graph_missing_clear(&graph->missing[i]);
    }
    free(graph->values);
    free(graph->ops);
    free(graph->edges);
    free(graph->missing);
    free(graph);
}

yvex_graph_status yvex_graph_status_of(const yvex_graph *graph)
{
    return graph ? graph->status : YVEX_GRAPH_STATUS_EMPTY;
}

const char *yvex_graph_status_name(yvex_graph_status status)
{
    switch (status) {
    case YVEX_GRAPH_STATUS_EMPTY: return "empty";
    case YVEX_GRAPH_STATUS_BUILT: return "built";
    case YVEX_GRAPH_STATUS_PARTIAL: return "partial";
    case YVEX_GRAPH_STATUS_UNSUPPORTED: return "unsupported";
    case YVEX_GRAPH_STATUS_INVALID: return "invalid";
    }
    return "unknown";
}

unsigned long long yvex_graph_value_count(const yvex_graph *graph)
{
    return graph ? graph->value_count : 0;
}

unsigned long long yvex_graph_op_count(const yvex_graph *graph)
{
    return graph ? graph->op_count : 0;
}

unsigned long long yvex_graph_missing_required_count(const yvex_graph *graph)
{
    return graph ? graph->missing_count : 0;
}

const yvex_graph_value_info *yvex_graph_value_at(const yvex_graph *graph,
                                                 unsigned long long index)
{
    if (!graph || index >= graph->value_count) {
        return NULL;
    }
    return &graph->values[index];
}

const yvex_graph_op_info *yvex_graph_op_at(const yvex_graph *graph,
                                           unsigned long long index)
{
    if (!graph || index >= graph->op_count) {
        return NULL;
    }
    return &graph->ops[index];
}

const yvex_graph_missing_required *yvex_graph_missing_required_at(const yvex_graph *graph,
                                                                  unsigned long long index)
{
    if (!graph || index >= graph->missing_count) {
        return NULL;
    }
    return &graph->missing[index];
}
