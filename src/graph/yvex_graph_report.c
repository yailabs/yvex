/*
 * yvex_graph_report.c - typed graph report construction and dump compatibility.
 *
 * Owner:
 *   src/graph
 *
 * Owns:
 *   graph report buffers, graph descriptor report construction, and legacy
 *   FILE-based dump compatibility for graph, memory-plan, and plan facts.
 *
 * Does not own:
 *   CLI input parsing, command dispatch, renderer serialization,
 *   stdout/stderr output, primitive execution, graph guard execution,
 *   generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   report builders collect facts only; compatibility dump functions write
 *   only to their explicit FILE argument and never select stdout/stderr.
 *
 * Boundary:
 *   graph report construction is not transformer execution or generation
 *   readiness.
 */
#include "yvex_graph_report.h"
#include "yvex_graph_private.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int graph_file_write_all(FILE *fp, const char *text, size_t len)
{
    return len == 0u || fwrite(text, 1u, len, fp) == len;
}

static int graph_file_writef(FILE *fp, const char *fmt, ...)
{
    char stack[512];
    char *heap = NULL;
    va_list ap;
    va_list ap2;
    int need;
    int ok;

    va_start(ap, fmt);
    va_copy(ap2, ap);
    need = vsnprintf(stack, sizeof(stack), fmt, ap);
    va_end(ap);
    if (need < 0) {
        va_end(ap2);
        return 0;
    }
    if ((size_t)need < sizeof(stack)) {
        va_end(ap2);
        return graph_file_write_all(fp, stack, (size_t)need);
    }
    heap = (char *)malloc((size_t)need + 1u);
    if (!heap) {
        va_end(ap2);
        return 0;
    }
    ok = vsnprintf(heap, (size_t)need + 1u, fmt, ap2) == need &&
         graph_file_write_all(fp, heap, (size_t)need);
    va_end(ap2);
    free(heap);
    return ok;
}

int yvex_graph_report_appendf(yvex_graph_report *report,
                              const char *fmt,
                              ...)
{
    char stack[512];
    char *target;
    va_list ap;
    va_list ap2;
    int need;
    size_t required;
    size_t next_cap;

    if (!report || !fmt) {
        return YVEX_ERR_INVALID_ARG;
    }
    va_start(ap, fmt);
    va_copy(ap2, ap);
    need = vsnprintf(stack, sizeof(stack), fmt, ap);
    va_end(ap);
    if (need < 0) {
        va_end(ap2);
        return YVEX_ERR_FORMAT;
    }
    required = report->body_len + (size_t)need + 1u;
    if (required > report->body_cap) {
        next_cap = report->body_cap ? report->body_cap * 2u : 4096u;
        while (next_cap < required) {
            next_cap *= 2u;
        }
        target = (char *)realloc(report->body, next_cap);
        if (!target) {
            va_end(ap2);
            return YVEX_ERR_NOMEM;
        }
        report->body = target;
        report->body_cap = next_cap;
    }
    if ((size_t)need < sizeof(stack)) {
        memcpy(report->body + report->body_len, stack, (size_t)need + 1u);
    } else {
        (void)vsnprintf(report->body + report->body_len,
                        report->body_cap - report->body_len,
                        fmt,
                        ap2);
    }
    va_end(ap2);
    report->body_len += (size_t)need;
    return YVEX_OK;
}

void yvex_graph_report_clear(yvex_graph_report *report)
{
    if (!report) {
        return;
    }
    free(report->body);
    memset(report, 0, sizeof(*report));
}

void print_graph_guard_report(const yvex_cli_graph_guard_report *report)
{
    if (!report) {
        return;
    }
    (void)graph_file_writef(stdout, "graph_integrity_guard: %s\n", report->guard_status ? report->guard_status : "fail");
    (void)graph_file_writef(stdout, "graph_execution_phase: %s\n", report->phase ? report->phase : "preflight");
    (void)graph_file_writef(stdout, "graph_kind: %s\n", report->graph_kind ? report->graph_kind : "unknown");
    (void)graph_file_writef(stdout, "integrity_status: %s\n", report->integrity_status ? report->integrity_status : "unchecked");
    (void)graph_file_writef(stdout, "identity_status: %s\n", report->identity_status ? report->identity_status : "unregistered");
    (void)graph_file_writef(stdout, "metadata_status: %s\n", report->metadata_status ? report->metadata_status : "unregistered");
    (void)graph_file_writef(stdout, "shape_status: %s\n", report->shape_status ? report->shape_status : "unchecked");
    (void)graph_file_writef(stdout, "range_status: %s\n", report->range_status ? report->range_status : "unchecked");
    (void)graph_file_writef(stdout, "slice_range_status: %s\n", report->slice_range_status ? report->slice_range_status : "unchecked");
    (void)graph_file_writef(stdout, "backend_status: %s\n", report->backend_status ? report->backend_status : "not-opened");
    (void)graph_file_writef(stdout, "backend_op_status: %s\n", report->backend_op_status ? report->backend_op_status : "unchecked");
    (void)graph_file_writef(stdout, "dispatch_attempted: %s\n", report->dispatch_attempted ? "true" : "false");
    (void)graph_file_writef(stdout, "reference_read_attempted: %s\n", report->reference_read_attempted ? "true" : "false");
    (void)graph_file_writef(stdout, "output_allocation_attempted: %s\n", report->output_allocation_attempted ? "true" : "false");
    (void)graph_file_writef(stdout, "cleanup_attempted: %s\n", report->cleanup_attempted ? "true" : "false");
    (void)graph_file_writef(stdout, "cleanup_status: %s\n", report->cleanup_status ? report->cleanup_status : "not-needed");
    (void)graph_file_writef(stdout, "output_bytes_planned: %llu\n", report->output_bytes_planned);
    (void)graph_file_writef(stdout, "output_bytes_allocated: %llu\n", report->output_bytes_allocated);
    (void)graph_file_writef(stdout, "reference_bytes_planned: %llu\n", report->reference_bytes_planned);
}

int yvex_graph_exit_for_status(int status)
{
    switch (status) {
    case YVEX_OK:
        return 0;
    case YVEX_ERR_INVALID_ARG:
        return 2;
    case YVEX_ERR_IO:
    case YVEX_ERR_NOMEM:
        return 3;
    case YVEX_ERR_FORMAT:
    case YVEX_ERR_BOUNDS:
        return 4;
    case YVEX_ERR_UNSUPPORTED:
        return 5;
    default:
        return 1;
    }
}

static void report_shape_to_file(FILE *fp,
                                 const unsigned long long *dims,
                                 unsigned int rank)
{
    unsigned int i;

    (void)graph_file_writef(fp, "[");
    for (i = 0; i < rank; ++i) {
        if (i > 0) {
            (void)graph_file_writef(fp, ",");
        }
        (void)graph_file_writef(fp, "%llu", dims[i]);
    }
    (void)graph_file_writef(fp, "]");
}

static void report_edges_to_file(FILE *fp, const unsigned int *ids, unsigned int count)
{
    unsigned int i;

    (void)graph_file_writef(fp, "[");
    if (!ids) {
        (void)graph_file_writef(fp, "]");
        return;
    }
    for (i = 0; i < count; ++i) {
        if (i > 0) {
            (void)graph_file_writef(fp, ",");
        }
        (void)graph_file_writef(fp, "%u", ids[i]);
    }
    (void)graph_file_writef(fp, "]");
}

static int report_shape_to_report(yvex_graph_report *report,
                                  const unsigned long long *dims,
                                  unsigned int rank)
{
    unsigned int i;
    int rc;

    rc = yvex_graph_report_appendf(report, "[");
    for (i = 0; rc == YVEX_OK && i < rank; ++i) {
        rc = yvex_graph_report_appendf(report, "%s%llu",
                                       i > 0 ? "," : "",
                                       dims[i]);
    }
    if (rc == YVEX_OK) {
        rc = yvex_graph_report_appendf(report, "]");
    }
    return rc;
}

static int report_edges_to_report(yvex_graph_report *report,
                                  const unsigned int *ids,
                                  unsigned int count)
{
    unsigned int i;
    int rc;

    rc = yvex_graph_report_appendf(report, "[");
    if (!ids) {
        return rc == YVEX_OK ? yvex_graph_report_appendf(report, "]") : rc;
    }
    for (i = 0; rc == YVEX_OK && i < count; ++i) {
        rc = yvex_graph_report_appendf(report, "%s%u",
                                       i > 0 ? "," : "",
                                       ids[i]);
    }
    if (rc == YVEX_OK) {
        rc = yvex_graph_report_appendf(report, "]");
    }
    return rc;
}

int yvex_graph_dump(const yvex_graph *graph, FILE *fp, yvex_error *err)
{
    unsigned long long i;

    if (!graph || !fp) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_graph_dump", "graph and fp are required");
        return YVEX_ERR_INVALID_ARG;
    }

    (void)graph_file_writef(fp, "graph status: %s\n", yvex_graph_status_name(graph->status));
    (void)graph_file_writef(fp, "architecture: %s\n", graph->architecture ? graph->architecture : "unknown");
    (void)graph_file_writef(fp, "model_name: %s\n", graph->model_name ? graph->model_name : "");
    (void)graph_file_writef(fp, "values: %llu\n", graph->value_count);
    (void)graph_file_writef(fp, "ops: %llu\n", graph->op_count);
    (void)graph_file_writef(fp, "missing_required: %llu\n\n", graph->missing_count);

    for (i = 0; i < graph->value_count; ++i) {
        const yvex_graph_value_info *value = &graph->values[i];
        (void)graph_file_writef(fp, "value %u %s kind=%s shape=",
                                value->id,
                                value->name ? value->name : "",
                                yvex_value_kind_name(value->kind));
        report_shape_to_file(fp, value->dims, value->rank);
        (void)graph_file_writef(fp, " dtype=%s residency=%s",
                                yvex_dtype_name(value->dtype),
                                yvex_residency_name(value->residency));
        if (value->source_tensor_name && value->source_tensor_name[0] != '\0') {
            (void)graph_file_writef(fp, " source=%s", value->source_tensor_name);
        }
        (void)graph_file_writef(fp, "\n");
    }
    if (graph->value_count > 0 || graph->op_count > 0 || graph->missing_count > 0) {
        (void)graph_file_writef(fp, "\n");
    }

    for (i = 0; i < graph->op_count; ++i) {
        const yvex_graph_op_info *op = &graph->ops[i];
        const yvex_graph_op_edges *edges = yvex_graph_op_edges_at(graph, i);
        (void)graph_file_writef(fp, "op %u %s status=%s inputs=",
                                op->id,
                                op->name ? op->name : yvex_op_kind_name(op->kind),
                                yvex_op_status_name(op->status));
        report_edges_to_file(fp, edges ? edges->input_ids : NULL, op->input_count);
        (void)graph_file_writef(fp, " outputs=");
        report_edges_to_file(fp, edges ? edges->output_ids : NULL, op->output_count);
        if (op->reason && op->reason[0] != '\0') {
            (void)graph_file_writef(fp, " reason=\"%s\"", op->reason);
        }
        (void)graph_file_writef(fp, "\n");
    }
    if (graph->op_count > 0 && graph->missing_count > 0) {
        (void)graph_file_writef(fp, "\n");
    }

    for (i = 0; i < graph->missing_count; ++i) {
        const yvex_graph_missing_required *missing = &graph->missing[i];
        (void)graph_file_writef(fp, "missing %s reason=\"%s\"\n",
                                missing->role_name ? missing->role_name : yvex_tensor_role_name(missing->role),
                                missing->reason ? missing->reason : "");
    }
    (void)graph_file_writef(fp, "status: graph-%s\n", yvex_graph_status_name(graph->status));

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

    (void)graph_file_writef(fp, "memory:\n");
    (void)graph_file_writef(fp, "  model_tensor_bytes_known: %llu\n", plan->summary.model_tensor_bytes_known);
    (void)graph_file_writef(fp, "  model_tensor_bytes_unknown_count: %llu\n",
                            plan->summary.model_tensor_bytes_unknown_count);
    (void)graph_file_writef(fp, "  activation_peak_bytes: %llu\n", plan->summary.activation_peak_bytes);
    (void)graph_file_writef(fp, "  kv_cache_bytes: %llu\n", plan->summary.kv_cache_bytes);
    (void)graph_file_writef(fp, "  scratch_peak_bytes: %llu\n", plan->summary.scratch_peak_bytes);
    (void)graph_file_writef(fp, "  total_known_bytes: %llu\n", plan->summary.total_known_bytes);

    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_plan_dump(const yvex_plan *plan, FILE *fp, yvex_error *err)
{
    if (!plan || !fp || !plan->graph || !plan->memory) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_plan_dump", "complete plan and fp are required");
        return YVEX_ERR_INVALID_ARG;
    }

    (void)graph_file_writef(fp, "plan status: %s\n", yvex_memory_plan_status_name(yvex_memory_plan_status_of(plan->memory)));
    (void)graph_file_writef(fp, "backend: %s\n", plan->backend_name ? plan->backend_name : "");
    (void)graph_file_writef(fp, "backend_status: %s\n", plan->backend_status ? plan->backend_status : "unknown");
    if (plan->backend_status && strcmp(plan->backend_status, "available") == 0) {
        (void)graph_file_writef(fp, "backend_capabilities:\n");
        (void)graph_file_writef(fp, "  tensor_alloc: %s\n", plan->backend_tensor_alloc ? "yes" : "no");
        (void)graph_file_writef(fp, "  tensor_read_write: %s\n", plan->backend_tensor_read_write ? "yes" : "no");
        (void)graph_file_writef(fp, "  op_embed: %s\n", plan->backend_op_embed ? "yes" : "no");
        (void)graph_file_writef(fp, "  op_matmul: %s\n", plan->backend_op_matmul ? "yes" : "no");
        (void)graph_file_writef(fp, "  op_mlp: %s\n", plan->backend_op_mlp ? "yes" : "no");
        (void)graph_file_writef(fp, "  op_rms_norm: %s\n", plan->backend_op_rms_norm ? "yes" : "no");
        (void)graph_file_writef(fp, "  op_rope: %s\n", plan->backend_op_rope ? "yes" : "no");
        (void)graph_file_writef(fp, "  op_attention: %s\n", plan->backend_op_attention ? "yes" : "no");
    }
    (void)graph_file_writef(fp, "architecture: %s\n", plan->graph->architecture ? plan->graph->architecture : "unknown");
    (void)graph_file_writef(fp, "model_name: %s\n", plan->graph->model_name ? plan->graph->model_name : "");
    (void)graph_file_writef(fp, "graph_status: %s\n", yvex_graph_status_name(plan->graph->status));
    (void)graph_file_writef(fp, "ops: %llu\n", plan->graph->op_count);
    (void)graph_file_writef(fp, "missing_required: %llu\n\n", plan->graph->missing_count);

    if (yvex_memory_plan_dump(plan->memory, fp, err) != YVEX_OK) {
        return YVEX_ERR_INVALID_ARG;
    }
    (void)graph_file_writef(fp, "\nexecution_ready: false\n");
    if (plan->backend_status && strcmp(plan->backend_status, "unavailable") == 0) {
        (void)graph_file_writef(fp, "reason: CUDA runtime/device not available\n");
    } else if (plan->graph->status == YVEX_GRAPH_STATUS_PARTIAL) {
        (void)graph_file_writef(fp, "reason: graph partial; missing output_norm, output_head; backend lacks full graph ops\n");
    } else if (plan->graph->status == YVEX_GRAPH_STATUS_BUILT) {
        (void)graph_file_writef(fp, "reason: session execution not implemented\n");
    } else {
        (void)graph_file_writef(fp, "reason: graph unsupported; backend lacks full graph ops\n");
    }
    (void)graph_file_writef(fp, "status: plan-only\n");

    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_graph_report_build(const yvex_graph_report_request *request,
                            yvex_graph_report *report,
                            yvex_error *err)
{
    yvex_cli_tokenizer_context ctx;
    yvex_graph_build_options options;
    yvex_graph *graph = NULL;
    int rc;
    unsigned long long i;

    if (!request || !report || !request->model) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "graph_report", "model is required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(report, 0, sizeof(*report));
    memset(&ctx, 0, sizeof(ctx));
    memset(&options, 0, sizeof(options));
    options.sequence_length = request->sequence_length ? request->sequence_length : 1ull;
    options.context_length = request->context_length;
    options.include_prefill_path = 1;

    rc = open_model_context(request->model, &ctx, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_graph_build_for_model(&graph, ctx.model, ctx.table, &options, err);
    if (rc == YVEX_OK) {
        report->kind = YVEX_GRAPH_REPORT_KIND_GRAPH;
        report->status = "graph-report";
        report->graph_status = yvex_graph_status_name(graph->status);
        report->architecture = graph->architecture;
        report->model_name = graph->model_name;
        report->value_count = graph->value_count;
        report->op_count = graph->op_count;
        report->missing_required_count = graph->missing_count;
        report->execution_ready = 0;
        report->boundary = "graph construction is not transformer execution";
        report->exit_code = 0;
        rc = yvex_graph_report_appendf(report, "graph status: %s\n", yvex_graph_status_name(graph->status));
        if (rc == YVEX_OK) rc = yvex_graph_report_appendf(report, "architecture: %s\n", graph->architecture ? graph->architecture : "unknown");
        if (rc == YVEX_OK) rc = yvex_graph_report_appendf(report, "model_name: %s\n", graph->model_name ? graph->model_name : "");
        if (rc == YVEX_OK) rc = yvex_graph_report_appendf(report, "values: %llu\n", graph->value_count);
        if (rc == YVEX_OK) rc = yvex_graph_report_appendf(report, "ops: %llu\n", graph->op_count);
        if (rc == YVEX_OK) rc = yvex_graph_report_appendf(report, "missing_required: %llu\n\n", graph->missing_count);
        for (i = 0; rc == YVEX_OK && i < graph->value_count; ++i) {
            const yvex_graph_value_info *value = &graph->values[i];
            rc = yvex_graph_report_appendf(report,
                                           "value %u %s kind=%s shape=",
                                           value->id,
                                           value->name ? value->name : "",
                                           yvex_value_kind_name(value->kind));
            if (rc == YVEX_OK) {
                rc = report_shape_to_report(report, value->dims, value->rank);
            }
            if (rc == YVEX_OK) {
                rc = yvex_graph_report_appendf(
                    report,
                    " dtype=%s residency=%s",
                    yvex_dtype_name(value->dtype),
                    yvex_residency_name(value->residency));
            }
            if (rc == YVEX_OK && value->source_tensor_name &&
                value->source_tensor_name[0] != '\0') {
                rc = yvex_graph_report_appendf(report, " source=%s",
                                               value->source_tensor_name);
            }
            if (rc == YVEX_OK) {
                rc = yvex_graph_report_appendf(report, "\n");
            }
        }
        if (rc == YVEX_OK &&
            (graph->value_count > 0 || graph->op_count > 0 ||
             graph->missing_count > 0)) {
            rc = yvex_graph_report_appendf(report, "\n");
        }
        for (i = 0; rc == YVEX_OK && i < graph->op_count; ++i) {
            const yvex_graph_op_info *op = &graph->ops[i];
            const yvex_graph_op_edges *edges = yvex_graph_op_edges_at(graph, i);
            rc = yvex_graph_report_appendf(report,
                                           "op %u %s status=%s inputs=",
                                           op->id,
                                           op->name ? op->name : yvex_op_kind_name(op->kind),
                                           yvex_op_status_name(op->status));
            if (rc == YVEX_OK) {
                rc = report_edges_to_report(report,
                                            edges ? edges->input_ids : NULL,
                                            op->input_count);
            }
            if (rc == YVEX_OK) {
                rc = yvex_graph_report_appendf(report, " outputs=");
            }
            if (rc == YVEX_OK) {
                rc = report_edges_to_report(report,
                                            edges ? edges->output_ids : NULL,
                                            op->output_count);
            }
            if (rc == YVEX_OK && op->reason && op->reason[0] != '\0') {
                rc = yvex_graph_report_appendf(report, " reason=%s",
                                               op->reason);
            }
            if (rc == YVEX_OK) {
                rc = yvex_graph_report_appendf(report, "\n");
            }
        }
        if (rc == YVEX_OK && graph->op_count > 0 && graph->missing_count > 0) {
            rc = yvex_graph_report_appendf(report, "\n");
        }
        for (i = 0; rc == YVEX_OK && i < graph->missing_count; ++i) {
            const yvex_graph_missing_required *missing = &graph->missing[i];
            rc = yvex_graph_report_appendf(
                report,
                "missing %s reason=\"%s\"\n",
                missing->role_name
                    ? missing->role_name
                    : yvex_tensor_role_name(missing->role),
                missing->reason ? missing->reason : "");
        }
        if (rc == YVEX_OK) {
            rc = yvex_graph_report_appendf(report,
                                           "status: graph-%s\n",
                                           yvex_graph_status_name(graph->status));
        }
        if (rc == YVEX_OK) {
            yvex_error_clear(err);
        } else {
            yvex_error_set(err, rc, "graph_report", "failed to append graph report");
        }
    }
    yvex_graph_close(graph);
    close_model_context(&ctx);
    return rc;
}

int yvex_graph_plan_report_build(const yvex_graph_report_request *request,
                                 yvex_graph_report *report,
                                 yvex_error *err)
{
    (void)request;
    (void)report;
    yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "graph_plan_report", "plan report command is not owned by graph CLI");
    return YVEX_ERR_UNSUPPORTED;
}
