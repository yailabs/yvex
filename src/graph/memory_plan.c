/*
 * YVEX - Memory plan
 *
 * File: src/graph/memory_plan.c
 * Layer: graph implementation
 *
 * Purpose:
 *   Builds estimate-only memory summaries from F0 graphs and tensor tables.
 *   This module performs arithmetic over descriptor facts only; it does not
 *   allocate backend buffers or query devices.
 *
 * Implements:
 *   - yvex_memory_plan_from_graph
 *   - yvex_memory_plan_close
 *   - yvex_memory_plan_* accessors
 *   - yvex_memory_plan_dump
 *
 * Invariants:
 *   - no backend allocation happens
 *   - unknown tensor accounting remains explicit
 *   - activation estimates are deterministic for tests
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_memory_plan
 */
#include "graph_internal.h"

#include <limits.h>
#include <stdlib.h>

static int add_checked(unsigned long long a,
                       unsigned long long b,
                       unsigned long long *out,
                       yvex_error *err)
{
    if (a > ULLONG_MAX - b) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_memory_plan_from_graph", "memory byte total overflow");
        return YVEX_ERR_BOUNDS;
    }
    *out = a + b;
    return YVEX_OK;
}

static int compute_activation_peak(const yvex_graph *graph,
                                   unsigned long long *out,
                                   yvex_error *err)
{
    unsigned long long peak = 0;
    unsigned long long i;

    for (i = 0; i < yvex_graph_value_count(graph); ++i) {
        const yvex_graph_value_info *value = yvex_graph_value_at(graph, i);
        unsigned long long elements;
        unsigned long long bytes;
        int rc;

        if (!value || value->kind != YVEX_VALUE_ACTIVATION) {
            continue;
        }
        rc = yvex_shape_product(value->dims, value->rank, &elements, err);
        if (rc != YVEX_OK) {
            return rc;
        }
        rc = yvex_dtype_storage_bytes(value->dtype, elements, &bytes, err);
        if (rc == YVEX_ERR_UNSUPPORTED) {
            yvex_error_clear(err);
            continue;
        }
        if (rc != YVEX_OK) {
            return rc;
        }
        if (bytes > peak) {
            peak = bytes;
        }
    }

    *out = peak;
    return YVEX_OK;
}

int yvex_memory_plan_from_graph(yvex_memory_plan **out,
                                const yvex_graph *graph,
                                const yvex_tensor_table *tensors,
                                yvex_error *err)
{
    yvex_memory_plan *plan;
    unsigned long long i;
    int rc;

    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_memory_plan_from_graph", "out is required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;

    if (!graph || !tensors) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_memory_plan_from_graph",
                       "graph and tensors are required");
        return YVEX_ERR_INVALID_ARG;
    }

    plan = (yvex_memory_plan *)calloc(1, sizeof(*plan));
    if (!plan) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_memory_plan_from_graph",
                       "failed to allocate memory plan");
        return YVEX_ERR_NOMEM;
    }

    for (i = 0; i < yvex_tensor_table_count(tensors); ++i) {
        const yvex_tensor_info *tensor = yvex_tensor_table_at(tensors, i);
        if (!tensor) {
            continue;
        }
        if (tensor->storage_bytes == 0) {
            plan->summary.model_tensor_bytes_unknown_count += 1u;
        } else {
            rc = add_checked(plan->summary.model_tensor_bytes_known,
                             tensor->storage_bytes,
                             &plan->summary.model_tensor_bytes_known,
                             err);
            if (rc != YVEX_OK) {
                yvex_memory_plan_close(plan);
                return rc;
            }
        }
    }

    rc = compute_activation_peak(graph, &plan->summary.activation_peak_bytes, err);
    if (rc != YVEX_OK) {
        yvex_memory_plan_close(plan);
        return rc;
    }

    rc = add_checked(plan->summary.model_tensor_bytes_known,
                     plan->summary.activation_peak_bytes,
                     &plan->summary.total_known_bytes,
                     err);
    if (rc != YVEX_OK) {
        yvex_memory_plan_close(plan);
        return rc;
    }
    rc = add_checked(plan->summary.total_known_bytes,
                     plan->summary.kv_cache_bytes,
                     &plan->summary.total_known_bytes,
                     err);
    if (rc != YVEX_OK) {
        yvex_memory_plan_close(plan);
        return rc;
    }
    rc = add_checked(plan->summary.total_known_bytes,
                     plan->summary.scratch_peak_bytes,
                     &plan->summary.total_known_bytes,
                     err);
    if (rc != YVEX_OK) {
        yvex_memory_plan_close(plan);
        return rc;
    }

    switch (yvex_graph_status_of(graph)) {
    case YVEX_GRAPH_STATUS_BUILT:
        plan->status = YVEX_MEMORY_PLAN_ESTIMATED;
        break;
    case YVEX_GRAPH_STATUS_PARTIAL:
        plan->status = YVEX_MEMORY_PLAN_PARTIAL;
        break;
    case YVEX_GRAPH_STATUS_UNSUPPORTED:
    case YVEX_GRAPH_STATUS_INVALID:
        plan->status = YVEX_MEMORY_PLAN_UNSUPPORTED;
        break;
    case YVEX_GRAPH_STATUS_EMPTY:
    default:
        plan->status = YVEX_MEMORY_PLAN_EMPTY;
        break;
    }

    *out = plan;
    yvex_error_clear(err);
    return YVEX_OK;
}

void yvex_memory_plan_close(yvex_memory_plan *plan)
{
    free(plan);
}

yvex_memory_plan_status yvex_memory_plan_status_of(const yvex_memory_plan *plan)
{
    return plan ? plan->status : YVEX_MEMORY_PLAN_EMPTY;
}

const char *yvex_memory_plan_status_name(yvex_memory_plan_status status)
{
    switch (status) {
    case YVEX_MEMORY_PLAN_EMPTY: return "empty";
    case YVEX_MEMORY_PLAN_ESTIMATED: return "estimated";
    case YVEX_MEMORY_PLAN_PARTIAL: return "partial";
    case YVEX_MEMORY_PLAN_UNSUPPORTED: return "unsupported";
    }
    return "unknown";
}

int yvex_memory_plan_get_summary(const yvex_memory_plan *plan,
                                 yvex_memory_plan_summary *out,
                                 yvex_error *err)
{
    if (!plan || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_memory_plan_get_summary",
                       "plan and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = plan->summary;
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_memory_plan_dump(const yvex_memory_plan *plan,
                          FILE *fp,
                          yvex_error *err)
{
    if (!plan || !fp) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_memory_plan_dump",
                       "plan and fp are required");
        return YVEX_ERR_INVALID_ARG;
    }

    fprintf(fp, "memory:\n");
    fprintf(fp, "  model_tensor_bytes_known: %llu\n", plan->summary.model_tensor_bytes_known);
    fprintf(fp, "  model_tensor_bytes_unknown_count: %llu\n",
            plan->summary.model_tensor_bytes_unknown_count);
    fprintf(fp, "  activation_peak_bytes: %llu\n", plan->summary.activation_peak_bytes);
    fprintf(fp, "  kv_cache_bytes: %llu\n", plan->summary.kv_cache_bytes);
    fprintf(fp, "  scratch_peak_bytes: %llu\n", plan->summary.scratch_peak_bytes);
    fprintf(fp, "  total_known_bytes: %llu\n", plan->summary.total_known_bytes);

    yvex_error_clear(err);
    return YVEX_OK;
}
