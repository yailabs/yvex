/*
 * YVEX - Graph value storage
 *
 * File: src/graph/value.c
 * Layer: graph implementation
 *
 * Purpose:
 *   Provides private allocation helpers for graph planner graph values, operations, and
 *   missing-role diagnostics. The graph owns copied names and diagnostic
 *   strings so dumps remain valid after descriptor inputs are released.
 *
 * Implements:
 *   - yvex_graph_add_value
 *   - yvex_graph_add_op
 *   - yvex_graph_add_missing
 *
 * Invariants:
 *   - graph records own copied strings
 *   - value/op IDs are stable within a graph build
 *   - op edges are inspectable by graph dump only, not executable
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_graph
 */
#include "graph_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

char *yvex_graph_strdup(const char *text)
{
    size_t len;
    char *copy;

    if (!text) {
        text = "";
    }
    len = strlen(text);
    copy = (char *)malloc(len + 1u);
    if (!copy) {
        return NULL;
    }
    memcpy(copy, text, len + 1u);
    return copy;
}

void yvex_graph_value_clear(yvex_graph_value_info *value)
{
    if (!value) {
        return;
    }
    free((char *)value->name);
    free((char *)value->source_tensor_name);
    memset(value, 0, sizeof(*value));
}

void yvex_graph_op_clear(yvex_graph_op_info *op)
{
    if (!op) {
        return;
    }
    free((char *)op->name);
    free((char *)op->reason);
    memset(op, 0, sizeof(*op));
}

void yvex_graph_missing_clear(yvex_graph_missing_required *missing)
{
    if (!missing) {
        return;
    }
    free((char *)missing->role_name);
    free((char *)missing->reason);
    memset(missing, 0, sizeof(*missing));
}

static int reserve_values(yvex_graph *graph, unsigned long long need, yvex_error *err)
{
    yvex_graph_value_info *next;
    unsigned long long next_cap;

    if (graph->value_cap >= need) {
        return YVEX_OK;
    }
    next_cap = graph->value_cap == 0 ? 4 : graph->value_cap * 2u;
    while (next_cap < need) {
        next_cap *= 2u;
    }
    if (next_cap > (unsigned long long)(SIZE_MAX / sizeof(*graph->values))) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_graph_add_value", "value table too large");
        return YVEX_ERR_NOMEM;
    }
    next = (yvex_graph_value_info *)realloc(graph->values, (size_t)next_cap * sizeof(*graph->values));
    if (!next) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_graph_add_value", "failed to grow value table");
        return YVEX_ERR_NOMEM;
    }
    memset(next + graph->value_cap, 0, (size_t)(next_cap - graph->value_cap) * sizeof(*next));
    graph->values = next;
    graph->value_cap = next_cap;
    return YVEX_OK;
}

static int reserve_ops(yvex_graph *graph, unsigned long long need, yvex_error *err)
{
    yvex_graph_op_info *next_ops;
    yvex_graph_op_edges *next_edges;
    unsigned long long next_cap;

    if (graph->op_cap >= need) {
        return YVEX_OK;
    }
    next_cap = graph->op_cap == 0 ? 4 : graph->op_cap * 2u;
    while (next_cap < need) {
        next_cap *= 2u;
    }
    if (next_cap > (unsigned long long)(SIZE_MAX / sizeof(*graph->ops)) ||
        next_cap > (unsigned long long)(SIZE_MAX / sizeof(*graph->edges))) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_graph_add_op", "op table too large");
        return YVEX_ERR_NOMEM;
    }
    next_ops = (yvex_graph_op_info *)realloc(graph->ops, (size_t)next_cap * sizeof(*graph->ops));
    if (!next_ops) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_graph_add_op", "failed to grow op table");
        return YVEX_ERR_NOMEM;
    }
    graph->ops = next_ops;
    next_edges = (yvex_graph_op_edges *)realloc(graph->edges, (size_t)next_cap * sizeof(*graph->edges));
    if (!next_edges) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_graph_add_op", "failed to grow op edge table");
        return YVEX_ERR_NOMEM;
    }
    memset(graph->ops + graph->op_cap, 0, (size_t)(next_cap - graph->op_cap) * sizeof(*graph->ops));
    memset(next_edges + graph->op_cap, 0, (size_t)(next_cap - graph->op_cap) * sizeof(*next_edges));
    graph->edges = next_edges;
    graph->op_cap = next_cap;
    return YVEX_OK;
}

static int reserve_missing(yvex_graph *graph, unsigned long long need, yvex_error *err)
{
    yvex_graph_missing_required *next;
    unsigned long long next_cap;

    if (graph->missing_cap >= need) {
        return YVEX_OK;
    }
    next_cap = graph->missing_cap == 0 ? 4 : graph->missing_cap * 2u;
    while (next_cap < need) {
        next_cap *= 2u;
    }
    if (next_cap > (unsigned long long)(SIZE_MAX / sizeof(*graph->missing))) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_graph_add_missing", "missing table too large");
        return YVEX_ERR_NOMEM;
    }
    next = (yvex_graph_missing_required *)realloc(graph->missing,
                                                 (size_t)next_cap * sizeof(*graph->missing));
    if (!next) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_graph_add_missing", "failed to grow missing table");
        return YVEX_ERR_NOMEM;
    }
    memset(next + graph->missing_cap, 0, (size_t)(next_cap - graph->missing_cap) * sizeof(*next));
    graph->missing = next;
    graph->missing_cap = next_cap;
    return YVEX_OK;
}

int yvex_graph_add_value(yvex_graph *graph,
                         yvex_value_kind kind,
                         const char *name,
                         unsigned int rank,
                         const unsigned long long *dims,
                         yvex_dtype dtype,
                         yvex_residency residency,
                         const char *source_tensor_name,
                         unsigned int *out_id,
                         yvex_error *err)
{
    yvex_graph_value_info *value;
    unsigned int i;
    int rc;

    if (!graph || !name || rank > YVEX_GRAPH_MAX_DIMS || (rank > 0 && !dims)) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_graph_add_value", "invalid value arguments");
        return YVEX_ERR_INVALID_ARG;
    }
    rc = reserve_values(graph, graph->value_count + 1u, err);
    if (rc != YVEX_OK) {
        return rc;
    }

    value = &graph->values[graph->value_count];
    value->id = (unsigned int)graph->value_count;
    value->kind = kind;
    value->name = yvex_graph_strdup(name);
    if (!value->name) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_graph_add_value", "failed to copy value name");
        return YVEX_ERR_NOMEM;
    }
    value->rank = rank;
    for (i = 0; i < rank; ++i) {
        value->dims[i] = dims[i];
    }
    value->dtype = dtype;
    value->residency = residency;
    if (source_tensor_name) {
        value->source_tensor_name = yvex_graph_strdup(source_tensor_name);
        if (!value->source_tensor_name) {
            yvex_graph_value_clear(value);
            yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_graph_add_value", "failed to copy source tensor");
            return YVEX_ERR_NOMEM;
        }
    }

    if (out_id) {
        *out_id = value->id;
    }
    graph->value_count += 1u;
    return YVEX_OK;
}

int yvex_graph_add_op(yvex_graph *graph,
                      yvex_op_kind kind,
                      yvex_op_status status,
                      const char *name,
                      const unsigned int *inputs,
                      unsigned int input_count,
                      const unsigned int *outputs,
                      unsigned int output_count,
                      const char *reason,
                      yvex_error *err)
{
    yvex_graph_op_info *op;
    yvex_graph_op_edges *edges;
    unsigned int i;
    int rc;

    if (!graph || !name || input_count > 4u || output_count > 4u ||
        (input_count > 0 && !inputs) || (output_count > 0 && !outputs)) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_graph_add_op", "invalid op arguments");
        return YVEX_ERR_INVALID_ARG;
    }
    rc = reserve_ops(graph, graph->op_count + 1u, err);
    if (rc != YVEX_OK) {
        return rc;
    }

    op = &graph->ops[graph->op_count];
    edges = &graph->edges[graph->op_count];
    op->id = (unsigned int)graph->op_count;
    op->kind = kind;
    op->status = status;
    op->name = yvex_graph_strdup(name);
    op->reason = yvex_graph_strdup(reason ? reason : "");
    if (!op->name || !op->reason) {
        yvex_graph_op_clear(op);
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_graph_add_op", "failed to copy op text");
        return YVEX_ERR_NOMEM;
    }
    op->input_count = input_count;
    op->output_count = output_count;
    for (i = 0; i < input_count; ++i) {
        edges->input_ids[i] = inputs[i];
    }
    for (i = 0; i < output_count; ++i) {
        edges->output_ids[i] = outputs[i];
    }

    graph->op_count += 1u;
    return YVEX_OK;
}

int yvex_graph_add_missing(yvex_graph *graph,
                           yvex_tensor_role role,
                           const char *reason,
                           yvex_error *err)
{
    yvex_graph_missing_required *missing;
    int rc;

    if (!graph) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_graph_add_missing", "graph is required");
        return YVEX_ERR_INVALID_ARG;
    }
    rc = reserve_missing(graph, graph->missing_count + 1u, err);
    if (rc != YVEX_OK) {
        return rc;
    }

    missing = &graph->missing[graph->missing_count];
    missing->role = role;
    missing->role_name = yvex_graph_strdup(yvex_tensor_role_name(role));
    missing->reason = yvex_graph_strdup(reason ? reason : "");
    if (!missing->role_name || !missing->reason) {
        yvex_graph_missing_clear(missing);
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_graph_add_missing", "failed to copy missing diagnostic");
        return YVEX_ERR_NOMEM;
    }
    graph->missing_count += 1u;
    return YVEX_OK;
}

const yvex_graph_op_edges *yvex_graph_op_edges_at(const yvex_graph *graph,
                                                  unsigned long long index)
{
    if (!graph || index >= graph->op_count) {
        return NULL;
    }
    return &graph->edges[index];
}
