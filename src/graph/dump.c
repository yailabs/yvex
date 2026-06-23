/*
 * YVEX - Graph dump
 *
 * File: src/graph/dump.c
 * Layer: graph implementation
 *
 * Purpose:
 *   Emits deterministic text dumps for graph planner graph planning artifacts. Dumps are
 *   CLI proof surfaces and test fixtures; they do not describe executable
 *   backend state.
 *
 * Implements:
 *   - yvex_graph_dump
 *
 * Invariants:
 *   - output is deterministic for a given graph
 *   - missing required roles are printed explicitly
 *   - no execution-readiness claim is emitted
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_graph
 */
#include "graph_internal.h"

static void dump_shape(FILE *fp, const unsigned long long *dims, unsigned int rank)
{
    unsigned int i;

    fprintf(fp, "[");
    for (i = 0; i < rank; ++i) {
        if (i > 0) {
            fprintf(fp, ",");
        }
        fprintf(fp, "%llu", dims[i]);
    }
    fprintf(fp, "]");
}

static void dump_edge_list(FILE *fp, const unsigned int *ids, unsigned int count)
{
    unsigned int i;

    fprintf(fp, "[");
    if (!ids) {
        fprintf(fp, "]");
        return;
    }
    for (i = 0; i < count; ++i) {
        if (i > 0) {
            fprintf(fp, ",");
        }
        fprintf(fp, "%u", ids[i]);
    }
    fprintf(fp, "]");
}

int yvex_graph_dump(const yvex_graph *graph, FILE *fp, yvex_error *err)
{
    unsigned long long i;

    if (!graph || !fp) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_graph_dump", "graph and fp are required");
        return YVEX_ERR_INVALID_ARG;
    }

    fprintf(fp, "graph status: %s\n", yvex_graph_status_name(graph->status));
    fprintf(fp, "architecture: %s\n", graph->architecture ? graph->architecture : "unknown");
    fprintf(fp, "model_name: %s\n", graph->model_name ? graph->model_name : "");
    fprintf(fp, "values: %llu\n", graph->value_count);
    fprintf(fp, "ops: %llu\n", graph->op_count);
    fprintf(fp, "missing_required: %llu\n", graph->missing_count);
    fprintf(fp, "\n");

    for (i = 0; i < graph->value_count; ++i) {
        const yvex_graph_value_info *value = &graph->values[i];
        fprintf(fp, "value %u %s kind=%s shape=",
                value->id,
                value->name ? value->name : "",
                yvex_value_kind_name(value->kind));
        dump_shape(fp, value->dims, value->rank);
        fprintf(fp, " dtype=%s residency=%s",
                yvex_dtype_name(value->dtype),
                yvex_residency_name(value->residency));
        if (value->source_tensor_name && value->source_tensor_name[0] != '\0') {
            fprintf(fp, " source=%s", value->source_tensor_name);
        }
        fprintf(fp, "\n");
    }
    if (graph->value_count > 0 || graph->op_count > 0 || graph->missing_count > 0) {
        fprintf(fp, "\n");
    }

    for (i = 0; i < graph->op_count; ++i) {
        const yvex_graph_op_info *op = &graph->ops[i];
        const yvex_graph_op_edges *edges = yvex_graph_op_edges_at(graph, i);
        fprintf(fp, "op %u %s status=%s inputs=",
                op->id,
                op->name ? op->name : yvex_op_kind_name(op->kind),
                yvex_op_status_name(op->status));
        dump_edge_list(fp, edges ? edges->input_ids : NULL, op->input_count);
        fprintf(fp, " outputs=");
        dump_edge_list(fp, edges ? edges->output_ids : NULL, op->output_count);
        if (op->reason && op->reason[0] != '\0') {
            fprintf(fp, " reason=\"%s\"", op->reason);
        }
        fprintf(fp, "\n");
    }
    if (graph->op_count > 0 && graph->missing_count > 0) {
        fprintf(fp, "\n");
    }

    for (i = 0; i < graph->missing_count; ++i) {
        const yvex_graph_missing_required *missing = &graph->missing[i];
        fprintf(fp, "missing %s reason=\"%s\"\n",
                missing->role_name ? missing->role_name : yvex_tensor_role_name(missing->role),
                missing->reason ? missing->reason : "");
    }
    fprintf(fp, "status: graph-%s\n", yvex_graph_status_name(graph->status));

    yvex_error_clear(err);
    return YVEX_OK;
}
