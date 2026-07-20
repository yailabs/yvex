/* Owner: graph typed-report projection.
 * Owns: report buffers and graph, memory-plan, plan, and admission fact projection.
 * Does not own: CLI parsing/rendering, execution, capability decisions, or stdout selection.
 * Invariants: reports project typed owner facts and never infer a higher capability stage.
 * Boundary: an explicit FILE compatibility sink is not operator-I/O ownership.
 * Purpose: serialize immutable graph-domain facts for existing internal consumers.
 * Inputs: admitted graph, plan, memory, guard, and request values.
 * Effects: grows caller-owned reports or writes only to an explicit FILE argument.
 * Failure: allocation, formatting, or invalid-input failures leave typed error state. */
#include "src/graph/private.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Purpose: Project typed file write all facts into the caller-owned report sink.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
static int graph_file_write_all(FILE *fp, const char *text, size_t len)
{
    return len == 0u || fwrite(text, 1u, len, fp) == len;
}

// Purpose: Project typed file writef facts into the caller-owned report sink.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
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

// Purpose: Project typed report appendf facts into the caller-owned report sink.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
static int graph_report_appendf(yvex_graph_report *report,
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

// Purpose: Release graph-owned resources held by report clear.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
void yvex_graph_report_clear(yvex_graph_report *report)
{
    if (!report) {
        return;
    }
    free(report->body);
    memset(report, 0, sizeof(*report));
}

typedef struct {
    yvex_model_ref model;
    yvex_model_ref_identity_result identity;
    yvex_token_input tokens;
    yvex_cli_graph_guard_report guard;
    yvex_engine *engine;
    yvex_engine_options engine_options;
    yvex_partial_graph_options partial_options;
    yvex_segment_graph_options segment_options;
    yvex_partial_graph_result partial_result;
    yvex_segment_graph_result segment_result;
    unsigned long long vocab_size;
    unsigned int selected_token;
    int segment;
    int tokens_supplied;
} selected_graph_run;

// Purpose: Project one typed graph-admission record without changing its capability meaning.
// Inputs: borrowed immutable guard facts and a caller-owned report buffer.
// Effects: appends text facts only; it does not execute, allocate backend state, or promote support.
// Failure: append allocation failure leaves the report uncommitted and capability facts unchanged.
// Boundary: this is report projection, not graph admission or execution authority.
static void graph_guard_append(yvex_graph_report *report,
                               const yvex_cli_graph_guard_report *guard)
{
    (void)graph_report_appendf(report, "graph_integrity_guard: %s\n",
                               guard->guard_status ? guard->guard_status : "fail");
    (void)graph_report_appendf(report, "graph_execution_phase: %s\n",
                               guard->phase ? guard->phase : "preflight");
    (void)graph_report_appendf(report, "graph_kind: %s\n",
                               guard->graph_kind ? guard->graph_kind : "unknown");
    (void)graph_report_appendf(report, "integrity_status: %s\n",
                               guard->integrity_status ? guard->integrity_status : "unchecked");
    (void)graph_report_appendf(report, "identity_status: %s\n",
                               guard->identity_status ? guard->identity_status : "unregistered");
    (void)graph_report_appendf(report, "metadata_status: %s\n",
                               guard->metadata_status ? guard->metadata_status : "unregistered");
    (void)graph_report_appendf(report, "shape_status: %s\n",
                               guard->shape_status ? guard->shape_status : "unchecked");
    (void)graph_report_appendf(report, "range_status: %s\n",
                               guard->range_status ? guard->range_status : "unchecked");
    (void)graph_report_appendf(report, "slice_range_status: %s\n",
                               guard->slice_range_status ? guard->slice_range_status : "unchecked");
    (void)graph_report_appendf(report, "backend_status: %s\n",
                               guard->backend_status ? guard->backend_status : "not-opened");
    (void)graph_report_appendf(report, "backend_op_status: %s\n",
                               guard->backend_op_status ? guard->backend_op_status : "unchecked");
    (void)graph_report_appendf(report, "dispatch_attempted: %s\n",
                               guard->dispatch_attempted ? "true" : "false");
    (void)graph_report_appendf(report, "reference_read_attempted: %s\n",
                               guard->reference_read_attempted ? "true" : "false");
    (void)graph_report_appendf(report, "output_allocation_attempted: %s\n",
                               guard->output_allocation_attempted ? "true" : "false");
    (void)graph_report_appendf(report, "cleanup_attempted: %s\n",
                               guard->cleanup_attempted ? "true" : "false");
    (void)graph_report_appendf(report, "cleanup_status: %s\n",
                               guard->cleanup_status ? guard->cleanup_status : "not-needed");
    (void)graph_report_appendf(report, "output_bytes_planned: %llu\n",
                               guard->output_bytes_planned);
    (void)graph_report_appendf(report, "output_bytes_allocated: %llu\n",
                               guard->output_bytes_allocated);
    (void)graph_report_appendf(report, "reference_bytes_planned: %llu\n",
                               guard->reference_bytes_planned);
}

// Purpose: Normalize executor lifecycle facts back into their admitted preflight record.
// Inputs: one mutable guard plus immutable executor lifecycle, byte, and status facts.
// Effects: updates only the caller-owned guard record.
// Failure: absent optional strings preserve prior fail-closed admission facts.
// Boundary: the executor remains the authority for dispatch and cleanup outcomes.
static void graph_guard_apply(yvex_cli_graph_guard_report *guard,
                              const char *guard_status,
                              const char *phase,
                              const char *shape_status,
                              const char *range_status,
                              const char *slice_range_status,
                              const char *backend_status,
                              const char *backend_op_status,
                              int dispatch_attempted,
                              int reference_read_attempted,
                              int output_allocation_attempted,
                              int cleanup_attempted,
                              const char *cleanup_status,
                              unsigned long long output_bytes_planned,
                              unsigned long long output_bytes_allocated,
                              unsigned long long reference_bytes_planned,
                              int success)
{
    guard->guard_status = success && guard_status ? guard_status : "fail";
    guard->phase = phase ? phase : (success ? "complete" : "dispatch");
    guard->shape_status = shape_status ? shape_status : guard->shape_status;
    guard->range_status = range_status ? range_status : guard->range_status;
    guard->slice_range_status = slice_range_status ? slice_range_status : guard->slice_range_status;
    guard->backend_status = backend_status ? backend_status : guard->backend_status;
    guard->backend_op_status = backend_op_status ? backend_op_status : guard->backend_op_status;
    guard->dispatch_attempted = dispatch_attempted;
    guard->reference_read_attempted = reference_read_attempted;
    guard->output_allocation_attempted = output_allocation_attempted;
    guard->cleanup_attempted = cleanup_attempted;
    guard->cleanup_status = cleanup_status ? cleanup_status : guard->cleanup_status;
    guard->output_bytes_planned = output_bytes_planned;
    guard->output_bytes_allocated = output_bytes_allocated;
    guard->reference_bytes_planned = reference_bytes_planned;
}

// Purpose: Apply selected-embedding executor facts without reconstructing execution policy.
static void graph_guard_apply_partial(yvex_cli_graph_guard_report *guard,
                                      const yvex_partial_graph_result *result,
                                      int success)
{
    graph_guard_apply(guard, result->graph_integrity_guard, result->graph_execution_phase,
                      result->shape_status, result->range_status, result->slice_range_status,
                      result->backend_status, result->backend_op_status, result->dispatch_attempted,
                      result->reference_read_attempted, result->output_allocation_attempted,
                      result->cleanup_attempted, result->cleanup_status, result->output_bytes_planned,
                      result->output_bytes_allocated, result->reference_bytes_planned, success);
}

// Purpose: Apply selected embedding-plus-normalization executor facts without changing them.
static void graph_guard_apply_segment(yvex_cli_graph_guard_report *guard,
                                      const yvex_segment_graph_result *result,
                                      int success)
{
    graph_guard_apply(guard, result->graph_integrity_guard, result->graph_execution_phase,
                      result->shape_status, result->range_status, result->slice_range_status,
                      result->backend_status, result->backend_op_status, result->dispatch_attempted,
                      result->reference_read_attempted, result->output_allocation_attempted,
                      result->cleanup_attempted, result->cleanup_status, result->output_bytes_planned,
                      result->output_bytes_allocated, result->reference_bytes_planned, success);
}

// Purpose: Project explicit token admission while preserving bounds and selection evidence.
static void graph_token_append(yvex_graph_report *report,
                               const yvex_token_input *input,
                               const char *status,
                               const char *bounds_status,
                               unsigned long long selected_index,
                               unsigned int selected_token,
                               int selected_seen)
{
    (void)graph_report_appendf(report, "token_input_status: %s\n", status);
    (void)graph_report_appendf(report, "token_input_kind: %s\n",
                               input ? yvex_token_input_kind_name(input->kind) : "unknown");
    (void)graph_report_appendf(report, "token_count: %llu\n", input ? input->token_count : 0ull);
    (void)graph_report_appendf(report, "selected_token_index: %llu\n", selected_index);
    if (selected_seen) {
        (void)graph_report_appendf(report, "selected_token_id: %u\n", selected_token);
    } else {
        (void)graph_report_appendf(report, "selected_token_id: unavailable\n");
    }
    (void)graph_report_appendf(report, "token_bounds_status: %s\n", bounds_status);
}

// Purpose: Project a committed selected-embedding execution and its bounded reference evidence.
// Inputs: a completed runtime result retained by the selected run and a writable report.
// Effects: appends typed execution facts and sample values without retaining payload data.
// Failure: report allocation failure cannot change the already completed runtime transaction.
// Boundary: bounded embedding execution is not complete graph or generation support.
static void graph_partial_append(yvex_graph_report *report,
                                 selected_graph_run *run)
{
    const yvex_partial_graph_result *result = &run->partial_result;
    unsigned long long i;

    graph_guard_apply_partial(&run->guard, result, 1);
    graph_guard_append(report, &run->guard);
    (void)graph_report_appendf(report, "real_partial_graph_executed: true\n");
    (void)graph_report_appendf(report, "partial_graph_kind: %s\n", result->segment_name);
    (void)graph_report_appendf(report, "partial_backend: %s\n", result->backend_name);
    (void)graph_report_appendf(report, "partial_weight: %s\n", result->weight_name);
    (void)graph_report_appendf(report, "partial_weight_dtype: %s\n", result->weight_dtype);
    (void)graph_report_appendf(report, "partial_token: %u\n", result->token_id);
    (void)graph_report_appendf(report, "partial_node_count: %llu\n", result->node_count);
    (void)graph_report_appendf(report, "partial_output_dtype: %s\n", result->output_dtype);
    (void)graph_report_appendf(report, "partial_output_count: %llu\n", result->output_count);
    (void)graph_report_appendf(report, "partial_output_bytes: %llu\n", result->output_bytes);
    (void)graph_report_appendf(report, "partial_output_checksum: %llu\n", result->output_checksum);
    (void)graph_report_appendf(report, "partial_reference_checksum: %llu\n",
                               result->reference_checksum);
    (void)graph_report_appendf(report, "partial_max_abs_diff: %.9g\n", result->max_abs_diff);
    (void)graph_report_appendf(report, "partial_output_sample_count: %llu\n",
                               result->output_value_count);
    (void)graph_report_appendf(report, "partial_output_sample_values:");
    for (i = 0; i < result->output_value_count; ++i) {
        (void)graph_report_appendf(report, "%s%.9g", i == 0 ? " " : ",",
                                   (double)result->output_values[i]);
    }
    (void)graph_report_appendf(report, "\nexecution_ready: false\n");
    (void)graph_report_appendf(report, "graph_execution_ready: false\n");
    (void)graph_report_appendf(report, "prefill_ready: false\nlogits_ready: false\n");
    (void)graph_report_appendf(report, "generation: unsupported\n");
    (void)graph_report_appendf(report, "status: real-partial-graph-executed\n");
}

// Purpose: Project a committed embedding-plus-normalization segment and reference evidence.
// Inputs: a completed runtime segment result retained by the selected run and writable report.
// Effects: appends lifecycle, memory, parity, and bounded sample facts.
// Failure: report allocation failure cannot alter or promote the runtime result.
// Boundary: the two-operation segment is selected evidence, not transformer execution.
static void graph_segment_append(yvex_graph_report *report,
                                 selected_graph_run *run)
{
    const yvex_segment_graph_result *result = &run->segment_result;
    unsigned long long i;

    graph_guard_apply_segment(&run->guard, result, 1);
    graph_guard_append(report, &run->guard);
    (void)graph_report_appendf(report, "segment_graph_executed: true\n");
    (void)graph_report_appendf(report, "segment_backend: %s\n", result->backend_name);
    (void)graph_report_appendf(report, "segment_name: %s\n", result->segment_name);
    (void)graph_report_appendf(report, "segment_ops: %llu\n", result->segment_ops);
    (void)graph_report_appendf(report, "segment_op_0: embed\nsegment_op_1: rms_norm\n");
    (void)graph_report_appendf(report, "partial_token: %u\n", result->token_id);
    (void)graph_report_appendf(report, "token_tensor: %s\n", result->token_tensor_name);
    (void)graph_report_appendf(report, "token_tensor_dtype: %s\n", result->token_tensor_dtype);
    (void)graph_report_appendf(report, "rmsnorm_tensor: %s\n", result->rmsnorm_tensor_name);
    (void)graph_report_appendf(report, "rmsnorm_tensor_dtype: %s\n",
                               result->rmsnorm_tensor_dtype);
    (void)graph_report_appendf(report, "hidden_size: %llu\nvocab_size: %llu\n",
                               result->hidden_size, result->vocab_size);
    (void)graph_report_appendf(report, "rmsnorm_epsilon_key: %s\n", result->rmsnorm_epsilon_key);
    (void)graph_report_appendf(report, "rmsnorm_epsilon: %.9g\n", result->rmsnorm_epsilon);
    (void)graph_report_appendf(report, "segment_memory_plan: explicit\n");
    (void)graph_report_appendf(report, "segment_intermediate_count: %llu\n",
                               result->segment_intermediate_count);
    (void)graph_report_appendf(report, "segment_intermediate_bytes: %llu\n",
                               result->segment_intermediate_bytes);
    (void)graph_report_appendf(report, "segment_output_count: %llu\n", result->segment_output_count);
    (void)graph_report_appendf(report, "segment_output_bytes: %llu\n", result->segment_output_bytes);
    (void)graph_report_appendf(report, "segment_scratch_bytes: %llu\n", result->segment_scratch_bytes);
    (void)graph_report_appendf(report, "segment_reference_bytes: %llu\n",
                               result->segment_reference_bytes);
    (void)graph_report_appendf(report, "segment_output_checksum: %llu\n", result->output_checksum);
    (void)graph_report_appendf(report, "segment_reference_checksum: %llu\n",
                               result->reference_checksum);
    (void)graph_report_appendf(report, "segment_max_abs_diff: %.9g\n", result->max_abs_diff);
    (void)graph_report_appendf(report, "%s_reference_max_abs_diff: %.9g\n",
                               strcmp(result->backend_name, "cuda") == 0 ? "cuda" : "cpu",
                               result->max_abs_diff);
    if (strcmp(result->backend_name, "cuda") == 0) {
        (void)graph_report_appendf(report, "segment_cuda_parity: pass\n");
    }
    (void)graph_report_appendf(report, "segment_output_sample_count: %llu\n",
                               result->output_value_count);
    (void)graph_report_appendf(report, "segment_output_sample_values:");
    for (i = 0; i < result->output_value_count; ++i) {
        (void)graph_report_appendf(report, "%s%.9g", i == 0 ? " " : ",",
                                   (double)result->output_values[i]);
    }
    (void)graph_report_appendf(report, "\nexecution_ready: false\n");
    (void)graph_report_appendf(report, "graph_execution_ready: false\n");
    (void)graph_report_appendf(report, "prefill_ready: false\nlogits_ready: false\n");
    (void)graph_report_appendf(report, "generation: unsupported\n");
    (void)graph_report_appendf(report, "status: real-segment-graph-executed\n");
}

// Purpose: Initialize one selected-artifact report run with no borrowed state retained.
// Inputs: an immutable request whose strings remain borrowed for the call duration.
// Effects: clears owned state and derives only the requested mode and token options.
// Failure: invalid request facts are handled before any runtime resource is acquired.
// Boundary: initialization performs no artifact access or backend admission.
static void selected_run_init(selected_graph_run *run,
                              const yvex_graph_report_request *request)
{
    memset(run, 0, sizeof(*run));
    run->segment = request->action == YVEX_GRAPH_ACTION_EXECUTE_SEGMENT;
    run->tokens_supplied = request->tokens_text && request->tokens_text[0];
    run->partial_options.token_id = request->partial_token_seen
                                        ? (unsigned int)request->partial_token
                                        : 0u;
    run->segment_options.token_id = run->partial_options.token_id;
    run->segment_options.segment_name = request->segment ? request->segment : "embedding-rmsnorm";
}

// Purpose: Parse and bind explicit token input to both selected execution modes.
// Inputs: resolved model reference, explicit token text, and selected token index.
// Effects: fills run-owned token facts and updates both mode options on exact success.
// Failure: parse, vocabulary, bounds, or selection errors leave runtime unopened.
// Boundary: token admission does not execute a graph or infer model capability.
static int selected_token_prepare(selected_graph_run *run,
                                  const yvex_graph_report_request *request,
                                  yvex_error *err)
{
    int rc = yvex_token_input_parse_explicit(request->tokens_text, &run->tokens, err);

    if (rc == YVEX_OK) {
        rc = yvex_model_context_vocab_size(run->model.path, &run->vocab_size, err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_token_input_validate_bounds(&run->tokens, run->vocab_size, err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_token_input_select(&run->tokens, request->token_index,
                                     &run->selected_token, err);
    }
    if (rc == YVEX_OK) {
        run->partial_options.token_id = run->selected_token;
        run->segment_options.token_id = run->selected_token;
    }
    return rc;
}

// Purpose: Publish an identity refusal while keeping backend dispatch and allocation untouched.
// Inputs: immutable failed identity evidence and a caller-owned report.
// Effects: appends drift facts and initializes a fail-closed guard projection.
// Failure: report allocation failure cannot turn the identity refusal into success.
// Boundary: no model context, payload range, engine, or backend is opened here.
static void selected_identity_refusal(yvex_graph_report *report,
                                      selected_graph_run *run)
{
    const char *status = strcmp(run->identity.identity_status, "missing") == 0
                             ? "models-identity-missing"
                         : strcmp(run->identity.metadata_status, "missing") == 0
                             ? "models-metadata-missing"
                         : strcmp(run->identity.identity_status, "pass") == 0
                             ? "models-metadata-drift"
                             : "models-identity-fail";
    (void)graph_report_appendf(report, "artifact_identity: check\nsurface: graph\n");
    (void)graph_report_appendf(report, "alias: %s\n", run->model.alias ? run->model.alias : "");
    (void)graph_report_appendf(report, "path: %s\n", run->model.path ? run->model.path : "");
    (void)graph_report_appendf(report, "registered_sha256: %s\n",
                               run->model.sha256 && run->model.sha256[0]
                                   ? run->model.sha256
                                   : "absent");
    (void)graph_report_appendf(report, "current_sha256: %s\n",
                               run->identity.current_sha256[0]
                                   ? run->identity.current_sha256
                                   : "unavailable");
    (void)graph_report_appendf(report, "digest_status: %s\n", run->identity.digest_status);
    (void)graph_report_appendf(report, "identity_status: %s\n", run->identity.identity_status);
    (void)graph_report_appendf(report, "metadata_status: %s\n", run->identity.metadata_status);
    for (unsigned int issue = 0; issue < run->identity.metadata_drift.issue_count; ++issue) {
        (void)graph_report_appendf(report, "metadata_issue_%u_code: %s\n", issue,
                                   run->identity.metadata_drift.issues[issue].code);
    }
    (void)graph_report_appendf(report, "reason: %s\n", run->identity.reason);
    (void)graph_report_appendf(report, "status: %s\n", status);
    memset(&run->guard, 0, sizeof(run->guard));
    run->guard.guard_status = "fail";
    run->guard.phase = "preflight";
    run->guard.graph_kind = run->segment ? "selected-embedding-rmsnorm"
                                         : "selected-embedding-partial";
    run->guard.identity_status = "fail";
    run->guard.metadata_status = "fail";
    run->guard.cleanup_status = "not-needed";
    graph_guard_append(report, &run->guard);
    (void)graph_report_appendf(report, "status: graph-integrity-fail\n");
}

// Purpose: Admit source identity, token selection, ranges, metadata, and backend before opening runtime.
// Inputs: selected request, caller-owned run/report storage, and typed error output.
// Effects: owns the resolved model reference and records exact preflight facts on success.
// Failure: any identity, token, metadata, range, or backend refusal leaves dispatch untouched.
// Boundary: this stage may inspect admitted artifact structure but cannot execute graph operations.
static int selected_run_admit(selected_graph_run *run,
                              const yvex_graph_report_request *request,
                              yvex_graph_report *report,
                              yvex_error *err)
{
    int rc = yvex_model_ref_resolve(&run->model, request->model, NULL, err);

    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_model_ref_identity_validate(&run->model, &run->identity, err);
    if (rc != YVEX_OK) {
        selected_identity_refusal(report, run);
        return rc;
    }
    if (run->tokens_supplied) {
        rc = selected_token_prepare(run, request, err);
        if (rc != YVEX_OK) {
            memset(&run->guard, 0, sizeof(run->guard));
            run->guard.guard_status = "fail";
            run->guard.phase = "preflight";
            run->guard.graph_kind = run->segment ? "selected-embedding-rmsnorm"
                                                 : "selected-embedding-partial";
            run->guard.identity_status = "pass";
            run->guard.metadata_status = "pass";
            run->guard.slice_range_status = "fail";
            run->guard.cleanup_status = "not-needed";
            graph_token_append(report, &run->tokens, "fail",
                               run->tokens.token_bounds_checked ? "fail" : "not-checked",
                               request->token_index, run->selected_token, 0);
            graph_guard_append(report, &run->guard);
            (void)graph_report_appendf(report, "status: graph-integrity-fail\n");
            return rc;
        }
    }
    rc = yvex_graph_preflight(&run->model, request->backend, 0, run->segment,
                              run->segment ? run->segment_options.token_id
                                           : run->partial_options.token_id,
                              &run->guard, err);
    if (rc != YVEX_OK) {
        graph_guard_append(report, &run->guard);
        (void)graph_report_appendf(report, "status: graph-integrity-fail\n");
        return rc;
    }
    if (run->tokens_supplied) {
        graph_token_append(report, &run->tokens, "pass", "pass", request->token_index,
                           run->selected_token, 1);
        (void)graph_report_appendf(report, "vocab_size: %llu\n", run->vocab_size);
    }
    return YVEX_OK;
}

// Purpose: Open the admitted runtime and execute exactly one selected bounded graph mode.
// Inputs: an admitted run, immutable request, report sink, and typed error output.
// Effects: owns one engine until cleanup and executes either partial or segment, never fixtures.
// Failure: executor facts drive a fail-closed report; caller cleanup owns the engine afterward.
// Boundary: execution stays bounded to selected embedding or embedding-plus-normalization evidence.
static int selected_run_execute(selected_graph_run *run,
                                const yvex_graph_report_request *request,
                                yvex_graph_report *report,
                                yvex_error *err)
{
    int rc;

    run->engine_options.model_path = run->model.path;
    run->engine_options.load_tokenizer = 0;
    run->engine_options.build_descriptor = 1;
    run->engine_options.build_default_graph = 1;
    run->engine_options.attach_weights = 1;
    run->engine_options.backend_name = request->backend;
    run->engine_options.require_all_weights = 1;
    rc = yvex_engine_open(&run->engine, &run->engine_options, err);
    if (rc == YVEX_OK && run->segment) {
        rc = yvex_engine_execute_segment_graph(run->engine, &run->segment_options,
                                               &run->segment_result, err);
    } else if (rc == YVEX_OK) {
        rc = yvex_engine_execute_partial_graph(run->engine, &run->partial_options,
                                               &run->partial_result, err);
    }
    if (rc != YVEX_OK) {
        if (run->segment) {
            graph_guard_apply_segment(&run->guard, &run->segment_result, 0);
        } else {
            graph_guard_apply_partial(&run->guard, &run->partial_result, 0);
        }
        graph_guard_append(report, &run->guard);
        (void)graph_report_appendf(report, "status: %s\n",
                                   run->guard.cleanup_attempted
                                       ? "graph-failed-cleaned"
                                       : "graph-integrity-fail");
        return rc;
    }
    if (run->segment) {
        graph_segment_append(report, run);
    } else {
        graph_partial_append(report, run);
    }
    return YVEX_OK;
}

// Purpose: Close all selected-report resources after success or any admitted failure.
static void selected_run_clear(selected_graph_run *run)
{
    if (run->engine) {
        yvex_engine_close(run->engine);
    }
    yvex_model_ref_clear(&run->model);
    memset(run, 0, sizeof(*run));
}

// Purpose: Execute the retained selected-artifact graph boundary without reviving test fixtures.
// Inputs: validated partial/segment request, caller-owned report, and typed error output.
// Effects: resolves, admits, executes, reports, and deterministically releases one bounded run.
// Failure: every stage returns typed status and cleanup occurs before the result escapes.
// Boundary: only selected-artifact partial and segment modes are accepted.
static int graph_selected_report_build(const yvex_graph_report_request *request,
                                       yvex_graph_report *report,
                                       yvex_error *err)
{
    selected_graph_run run;
    int rc;

    selected_run_init(&run, request);
    if (run.tokens_supplied && request->partial_token_seen) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "graph_report",
                       "tokens cannot be combined with explicit graph token");
        return YVEX_ERR_INVALID_ARG;
    }
    rc = selected_run_admit(&run, request, report, err);
    if (rc == YVEX_OK) {
        rc = selected_run_execute(&run, request, report, err);
    }
    selected_run_clear(&run);
    report->exit_code = yvex_graph_exit_for_status(rc);
    return rc;
}

/* Purpose: retain selected-artifact execution and refuse retired production proof fixtures.
 * Inputs: typed request plus writable report and error facts.
 * Effects: executes only admitted partial/segment requests; all other actions publish refusal.
 * Failure: typed errors preserve cleanup and never publish higher graph capability.
 * Boundary: focused fixtures and independent judges are test-owned evidence. */
int yvex_graph_primitive_report_build(const yvex_graph_report_request *request,
                                      yvex_graph_report *report,
                                      yvex_error *err)
{
    if (!request || !report) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "graph_primitive_report",
                       "request and report are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(report, 0, sizeof(*report));
    report->kind = YVEX_GRAPH_REPORT_KIND_PRIMITIVE;
    report->execution_ready = 0;
    report->boundary = "selected execution is not complete graph execution";
    if (request->action == YVEX_GRAPH_ACTION_EXECUTE_PARTIAL ||
        request->action == YVEX_GRAPH_ACTION_EXECUTE_SEGMENT) {
        return graph_selected_report_build(request, report, err);
    }
    report->status = "graph-proof-retired";
    report->reason = "production-fixtures-are-test-owned";
    report->exit_code = yvex_graph_exit_for_status(YVEX_ERR_UNSUPPORTED);
    (void)graph_report_appendf(report, "graph_integrity_guard: refused\n");
    (void)graph_report_appendf(report, "graph_execution_phase: admission\n");
    (void)graph_report_appendf(report, "execution_ready: false\n");
    (void)graph_report_appendf(report, "attention_execution_supported: %s\n",
        yvex_attention_execute_supported(NULL) ? "true" : "false");
    (void)graph_report_appendf(report, "generation_ready: false\n");
    (void)graph_report_appendf(report, "reason: %s\nstatus: %s\n",
                               report->reason, report->status);
    yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "graph_primitive_report",
                   "production graph fixtures were retired to focused tests");
    return YVEX_ERR_UNSUPPORTED;
}

// Purpose: Implement the graph-local exit for status semantic operation.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
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

// Purpose: Project typed report shape to file facts into the caller-owned report sink.
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

// Purpose: Project typed report edges to file facts into the caller-owned report sink.
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

// Purpose: Project typed report shape to report facts into the caller-owned report sink.
static int report_shape_to_report(yvex_graph_report *report,
                                  const unsigned long long *dims,
                                  unsigned int rank)
{
    unsigned int i;
    int rc;

    rc = graph_report_appendf(report, "[");
    for (i = 0; rc == YVEX_OK && i < rank; ++i) {
        rc = graph_report_appendf(report, "%s%llu",
                                       i > 0 ? "," : "",
                                       dims[i]);
    }
    if (rc == YVEX_OK) {
        rc = graph_report_appendf(report, "]");
    }
    return rc;
}

// Purpose: Project typed report edges to report facts into the caller-owned report sink.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
static int report_edges_to_report(yvex_graph_report *report,
                                  const unsigned int *ids,
                                  unsigned int count)
{
    unsigned int i;
    int rc;

    rc = graph_report_appendf(report, "[");
    if (!ids) {
        return rc == YVEX_OK ? graph_report_appendf(report, "]") : rc;
    }
    for (i = 0; rc == YVEX_OK && i < count; ++i) {
        rc = graph_report_appendf(report, "%s%u",
                                       i > 0 ? "," : "",
                                       ids[i]);
    }
    if (rc == YVEX_OK) {
        rc = graph_report_appendf(report, "]");
    }
    return rc;
}

// Purpose: Project typed dump facts into the caller-owned report sink.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
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

// Purpose: Project typed memory plan dump facts into the caller-owned report sink.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
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

// Purpose: Project typed plan dump facts into the caller-owned report sink.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
int yvex_plan_dump(const yvex_plan *plan, FILE *fp, yvex_error *err)
{
    if (!plan || !fp || !plan->graph || !plan->memory) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_plan_dump", "complete plan and fp are required");
        return YVEX_ERR_INVALID_ARG;
    }

    (void)graph_file_writef(
        fp, "plan status: %s\n",
        yvex_memory_plan_status_name(yvex_memory_plan_status_of(plan->memory)));
    (void)graph_file_writef(fp, "backend: %s\n", plan->backend_name ? plan->backend_name : "");
    (void)graph_file_writef(fp, "backend_status: %s\n", plan->backend_status ? plan->backend_status : "unknown");
    if (plan->backend_status && strcmp(plan->backend_status, "available") == 0) {
        (void)graph_file_writef(fp, "backend_capabilities:\n");
        (void)graph_file_writef(fp, "  tensor_alloc: %s\n", plan->backend_tensor_alloc ? "yes" : "no");
        (void)graph_file_writef(fp, "  tensor_read_write: %s\n",
                                plan->backend_tensor_read_write ? "yes" : "no");
        (void)graph_file_writef(fp, "  op_embed: %s\n", plan->backend_op_embed ? "yes" : "no");
        (void)graph_file_writef(fp, "  op_matmul: %s\n", plan->backend_op_matmul ? "yes" : "no");
        (void)graph_file_writef(fp, "  op_mlp: %s\n", plan->backend_op_mlp ? "yes" : "no");
        (void)graph_file_writef(fp, "  op_rms_norm: %s\n", plan->backend_op_rms_norm ? "yes" : "no");
        (void)graph_file_writef(fp, "  op_rope: %s\n", plan->backend_op_rope ? "yes" : "no");
        (void)graph_file_writef(fp, "  op_attention: %s\n", plan->backend_op_attention ? "yes" : "no");
    }
    (void)graph_file_writef(fp, "architecture: %s\n",
                            plan->graph->architecture ? plan->graph->architecture : "unknown");
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
        (void)graph_file_writef(
            fp, "reason: graph partial; missing output_norm, output_head; "
                "backend lacks full graph ops\n");
    } else if (plan->graph->status == YVEX_GRAPH_STATUS_BUILT) {
        (void)graph_file_writef(fp, "reason: session execution not implemented\n");
    } else {
        (void)graph_file_writef(fp, "reason: graph unsupported; backend lacks full graph ops\n");
    }
    (void)graph_file_writef(fp, "status: plan-only\n");

    yvex_error_clear(err);
    return YVEX_OK;
}

// Purpose: Project typed report build facts into the caller-owned report sink.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
int yvex_graph_report_build(const yvex_graph_report_request *request,
                            yvex_graph_report *report,
                            yvex_error *err)
{
    yvex_model_context ctx;
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

    rc = yvex_model_context_open(request->model, &ctx, err);
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
        rc = graph_report_appendf(report, "graph status: %s\n", yvex_graph_status_name(graph->status));
        if (rc == YVEX_OK)
            rc = graph_report_appendf(report, "architecture: %s\n",
                                      graph->architecture ? graph->architecture : "unknown");
        if (rc == YVEX_OK)
            rc = graph_report_appendf(report, "model_name: %s\n",
                                      graph->model_name ? graph->model_name : "");
        if (rc == YVEX_OK) rc = graph_report_appendf(report, "values: %llu\n", graph->value_count);
        if (rc == YVEX_OK) rc = graph_report_appendf(report, "ops: %llu\n", graph->op_count);
        if (rc == YVEX_OK) rc = graph_report_appendf(report, "missing_required: %llu\n\n", graph->missing_count);
        for (i = 0; rc == YVEX_OK && i < graph->value_count; ++i) {
            const yvex_graph_value_info *value = &graph->values[i];
            rc = graph_report_appendf(report,
                                           "value %u %s kind=%s shape=",
                                           value->id,
                                           value->name ? value->name : "",
                                           yvex_value_kind_name(value->kind));
            if (rc == YVEX_OK) {
                rc = report_shape_to_report(report, value->dims, value->rank);
            }
            if (rc == YVEX_OK) {
                rc = graph_report_appendf(
                    report,
                    " dtype=%s residency=%s",
                    yvex_dtype_name(value->dtype),
                    yvex_residency_name(value->residency));
            }
            if (rc == YVEX_OK && value->source_tensor_name &&
                value->source_tensor_name[0] != '\0') {
                rc = graph_report_appendf(report, " source=%s",
                                               value->source_tensor_name);
            }
            if (rc == YVEX_OK) {
                rc = graph_report_appendf(report, "\n");
            }
        }
        if (rc == YVEX_OK &&
            (graph->value_count > 0 || graph->op_count > 0 ||
             graph->missing_count > 0)) {
            rc = graph_report_appendf(report, "\n");
        }
        for (i = 0; rc == YVEX_OK && i < graph->op_count; ++i) {
            const yvex_graph_op_info *op = &graph->ops[i];
            const yvex_graph_op_edges *edges = yvex_graph_op_edges_at(graph, i);
            rc = graph_report_appendf(report,
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
                rc = graph_report_appendf(report, " outputs=");
            }
            if (rc == YVEX_OK) {
                rc = report_edges_to_report(report,
                                            edges ? edges->output_ids : NULL,
                                            op->output_count);
            }
            if (rc == YVEX_OK && op->reason && op->reason[0] != '\0') {
                rc = graph_report_appendf(report, " reason=%s",
                                               op->reason);
            }
            if (rc == YVEX_OK) {
                rc = graph_report_appendf(report, "\n");
            }
        }
        if (rc == YVEX_OK && graph->op_count > 0 && graph->missing_count > 0) {
            rc = graph_report_appendf(report, "\n");
        }
        for (i = 0; rc == YVEX_OK && i < graph->missing_count; ++i) {
            const yvex_graph_missing_required *missing = &graph->missing[i];
            rc = graph_report_appendf(
                report,
                "missing %s reason=\"%s\"\n",
                missing->role_name
                    ? missing->role_name
                    : yvex_tensor_role_name(missing->role),
                missing->reason ? missing->reason : "");
        }
        if (rc == YVEX_OK) {
            rc = graph_report_appendf(report,
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
    yvex_model_context_close(&ctx);
    return rc;
}
