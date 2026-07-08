/*
 * yvex_graph_plan.c - Graph execution-plan construction facts.
 *
 * Owner:
 *   src/graph
 *
 * Owns:
 *   plan lifecycle, backend label validation, backend capability probing, and
 *   plan status facts.
 *
 * Does not own:
 *   graph construction internals, memory-plan internals, graph reports,
 *   primitive execution, CLI parsing, rendering, stdout/stderr output,
 *   generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   backend probing records capability facts only and does not imply graph
 *   execution readiness.
 *
 * Boundary:
 *   plan-only capability facts are not full graph runtime support.
 */
#include "yvex_graph_plan.h"

#include <stdlib.h>
#include <string.h>

static int backend_allowed(const char *name)
{
    return strcmp(name, "cpu") == 0 || strcmp(name, "none") == 0 || strcmp(name, "cuda") == 0;
}

int yvex_graph_backend_valid(const char *name)
{
    return name && (strcmp(name, "cpu") == 0 || strcmp(name, "cuda") == 0);
}

yvex_backend_kind yvex_graph_backend_kind_from_name(const char *name)
{
    return strcmp(name, "cuda") == 0 ? YVEX_BACKEND_KIND_CUDA : YVEX_BACKEND_KIND_CPU;
}

static int fill_backend_status(yvex_plan *plan, const char *backend_name, yvex_error *err)
{
    yvex_backend *backend = NULL;
    yvex_backend_options backend_options;
    int rc;

    if (strcmp(backend_name, "cpu") == 0 || strcmp(backend_name, "cuda") == 0) {
        memset(&backend_options, 0, sizeof(backend_options));
        backend_options.kind = yvex_graph_backend_kind_from_name(backend_name);
        rc = yvex_backend_open(&backend, &backend_options, err);
        if (rc == YVEX_ERR_UNSUPPORTED && strcmp(backend_name, "cuda") == 0) {
            plan->backend_status = yvex_graph_strdup("unavailable");
            yvex_error_clear(err);
            return plan->backend_status ? YVEX_OK : YVEX_ERR_NOMEM;
        }
        if (rc != YVEX_OK) {
            return rc;
        }
        plan->backend_status = yvex_graph_strdup("available");
        plan->backend_tensor_alloc = yvex_backend_supports(backend, YVEX_BACKEND_CAP_TENSOR_ALLOC);
        plan->backend_tensor_read_write = yvex_backend_supports(backend, YVEX_BACKEND_CAP_TENSOR_READ_WRITE);
        plan->backend_op_embed = yvex_backend_supports(backend, YVEX_BACKEND_CAP_OP_EMBED);
        plan->backend_op_matmul = yvex_backend_supports(backend, YVEX_BACKEND_CAP_OP_MATMUL);
        plan->backend_op_mlp = yvex_backend_supports(backend, YVEX_BACKEND_CAP_OP_MLP);
        plan->backend_op_rms_norm = yvex_backend_supports(backend, YVEX_BACKEND_CAP_OP_RMS_NORM);
        plan->backend_op_rope = yvex_backend_supports(backend, YVEX_BACKEND_CAP_OP_ROPE);
        plan->backend_op_attention = yvex_backend_supports(backend, YVEX_BACKEND_CAP_OP_ATTENTION);
        yvex_backend_close(backend);
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
                        "backend label unsupported in graph planner: %s", backend_name);
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
