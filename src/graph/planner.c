/*
 * YVEX - Planner
 *
 * File: src/graph/planner.c
 * Layer: graph implementation
 *
 * Purpose:
 *   Combines a graph and estimate-only memory plan into a high-level planning
 *   object. In G0, CPU backend availability/capabilities are probed through
 *   the backend ABI, while execution remains disabled.
 *
 * Implements:
 *   - yvex_plan_create
 *   - yvex_plan_close
 *   - yvex_plan_graph
 *   - yvex_plan_memory
 *   - yvex_plan_dump
 *
 * Invariants:
 *   - plan owns graph and memory plan
 *   - cpu is available when the CPU backend ABI opens
 *   - cuda is reported unsupported until L0
 *   - execution_ready remains false in G0
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_planner
 */
#include "graph_internal.h"

#include <stdlib.h>
#include <string.h>

static int backend_allowed(const char *name)
{
    return strcmp(name, "cpu") == 0 || strcmp(name, "none") == 0 || strcmp(name, "cuda") == 0;
}

static int fill_backend_status(yvex_plan *plan, const char *backend_name, yvex_error *err)
{
    yvex_backend *backend = NULL;
    int rc;

    if (strcmp(backend_name, "cpu") == 0) {
        rc = yvex_backend_open_cpu(&backend, err);
        if (rc != YVEX_OK) {
            return rc;
        }
        plan->backend_status = yvex_graph_strdup("available");
        plan->backend_tensor_alloc = yvex_backend_supports(backend, YVEX_BACKEND_CAP_TENSOR_ALLOC);
        plan->backend_tensor_read_write = yvex_backend_supports(backend, YVEX_BACKEND_CAP_TENSOR_READ_WRITE);
        plan->backend_op_embed = yvex_backend_supports(backend, YVEX_BACKEND_CAP_OP_EMBED);
        plan->backend_op_matmul = yvex_backend_supports(backend, YVEX_BACKEND_CAP_OP_MATMUL);
        plan->backend_op_rms_norm = yvex_backend_supports(backend, YVEX_BACKEND_CAP_OP_RMS_NORM);
        plan->backend_op_attention = yvex_backend_supports(backend, YVEX_BACKEND_CAP_OP_ATTENTION);
        yvex_backend_close(backend);
    } else if (strcmp(backend_name, "cuda") == 0) {
        plan->backend_status = yvex_graph_strdup("unsupported");
    } else {
        plan->backend_status = yvex_graph_strdup("not-selected");
    }

    if (!plan->backend_status) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_plan_create", "failed to copy backend status");
        return YVEX_ERR_NOMEM;
    }
    return YVEX_OK;
}

int yvex_plan_create(yvex_plan **out,
                     const yvex_model_descriptor *model,
                     const yvex_tensor_table *tensors,
                     const yvex_plan_options *options,
                     yvex_error *err)
{
    yvex_plan *plan;
    yvex_graph_build_options graph_options;
    const char *backend_name = "cpu";
    int rc;

    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_plan_create", "out is required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;

    if (!model || !tensors) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_plan_create", "model and tensors are required");
        return YVEX_ERR_INVALID_ARG;
    }

    memset(&graph_options, 0, sizeof(graph_options));
    graph_options.sequence_length = 1;
    graph_options.context_length = yvex_model_context_length(model) > 0
                                       ? yvex_model_context_length(model)
                                       : 1;
    graph_options.include_prefill_path = 1;

    if (options) {
        if (options->sequence_length > 0) {
            graph_options.sequence_length = options->sequence_length;
        }
        if (options->context_length > 0) {
            graph_options.context_length = options->context_length;
        }
        if (options->backend_name) {
            backend_name = options->backend_name;
        }
    }

    if (!backend_allowed(backend_name)) {
        yvex_error_setf(err, YVEX_ERR_UNSUPPORTED, "yvex_plan_create",
                        "backend label unsupported in F0: %s", backend_name);
        return YVEX_ERR_UNSUPPORTED;
    }

    plan = (yvex_plan *)calloc(1, sizeof(*plan));
    if (!plan) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_plan_create", "failed to allocate plan");
        return YVEX_ERR_NOMEM;
    }
    plan->backend_name = yvex_graph_strdup(backend_name);
    if (!plan->backend_name) {
        yvex_plan_close(plan);
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_plan_create", "failed to copy backend label");
        return YVEX_ERR_NOMEM;
    }
    rc = fill_backend_status(plan, backend_name, err);
    if (rc != YVEX_OK) {
        yvex_plan_close(plan);
        return rc;
    }

    rc = yvex_graph_build_for_model(&plan->graph, model, tensors, &graph_options, err);
    if (rc == YVEX_OK) {
        rc = yvex_memory_plan_from_graph(&plan->memory, plan->graph, tensors, err);
    }
    if (rc != YVEX_OK) {
        yvex_plan_close(plan);
        return rc;
    }

    *out = plan;
    yvex_error_clear(err);
    return YVEX_OK;
}

void yvex_plan_close(yvex_plan *plan)
{
    if (!plan) {
        return;
    }
    free(plan->backend_name);
    free(plan->backend_status);
    yvex_memory_plan_close(plan->memory);
    yvex_graph_close(plan->graph);
    free(plan);
}

const yvex_graph *yvex_plan_graph(const yvex_plan *plan)
{
    return plan ? plan->graph : NULL;
}

const yvex_memory_plan *yvex_plan_memory(const yvex_plan *plan)
{
    return plan ? plan->memory : NULL;
}

int yvex_plan_dump(const yvex_plan *plan, FILE *fp, yvex_error *err)
{
    if (!plan || !fp || !plan->graph || !plan->memory) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_plan_dump", "complete plan and fp are required");
        return YVEX_ERR_INVALID_ARG;
    }

    fprintf(fp, "plan status: %s\n", yvex_memory_plan_status_name(yvex_memory_plan_status_of(plan->memory)));
    fprintf(fp, "backend: %s\n", plan->backend_name ? plan->backend_name : "");
    fprintf(fp, "backend_status: %s\n", plan->backend_status ? plan->backend_status : "unknown");
    if (plan->backend_status && strcmp(plan->backend_status, "available") == 0) {
        fprintf(fp, "backend_capabilities:\n");
        fprintf(fp, "  tensor_alloc: %s\n", plan->backend_tensor_alloc ? "yes" : "no");
        fprintf(fp, "  tensor_read_write: %s\n", plan->backend_tensor_read_write ? "yes" : "no");
        fprintf(fp, "  op_embed: %s\n", plan->backend_op_embed ? "yes" : "no");
        fprintf(fp, "  op_matmul: %s\n", plan->backend_op_matmul ? "yes" : "no");
        fprintf(fp, "  op_rms_norm: %s\n", plan->backend_op_rms_norm ? "yes" : "no");
        fprintf(fp, "  op_attention: %s\n", plan->backend_op_attention ? "yes" : "no");
    }
    fprintf(fp, "architecture: %s\n", plan->graph->architecture ? plan->graph->architecture : "unknown");
    fprintf(fp, "model_name: %s\n", plan->graph->model_name ? plan->graph->model_name : "");
    fprintf(fp, "graph_status: %s\n", yvex_graph_status_name(plan->graph->status));
    fprintf(fp, "ops: %llu\n", plan->graph->op_count);
    fprintf(fp, "missing_required: %llu\n", plan->graph->missing_count);
    fprintf(fp, "\n");

    if (yvex_memory_plan_dump(plan->memory, fp, err) != YVEX_OK) {
        return YVEX_ERR_INVALID_ARG;
    }
    fprintf(fp, "\n");
    fprintf(fp, "execution_ready: false\n");
    if (plan->backend_status && strcmp(plan->backend_status, "unsupported") == 0) {
        fprintf(fp, "reason: CUDA backend not implemented in G0\n");
    } else if (plan->graph->status == YVEX_GRAPH_STATUS_PARTIAL) {
        fprintf(fp, "reason: graph partial; missing output_norm, output_head; backend lacks full graph ops\n");
    } else if (plan->graph->status == YVEX_GRAPH_STATUS_BUILT) {
        fprintf(fp, "reason: session execution not implemented\n");
    } else {
        fprintf(fp, "reason: graph unsupported; backend lacks full graph ops\n");
    }
    fprintf(fp, "status: plan-only\n");

    yvex_error_clear(err);
    return YVEX_OK;
}
