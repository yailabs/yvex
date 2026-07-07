/*
 * yvex_kv_render.c - typed KV report rendering.
 *
 * Owner:
 *   src/cli/render
 *
 * Owns:
 *   normal, table, audit, and help serialization for typed KV reports.
 *
 * Does not own:
 *   KV report construction, input parsing, command dispatch, KV cache
 *   mutation, attention execution, decode, logits, sampling, generation,
 *   eval, benchmark, or release decisions.
 *
 * Invariants:
 *   renderers write only through src/cli/io helpers and never mutate reports.
 *
 * Boundary:
 *   rendering KV facts is not runtime KV support or generation readiness.
 */
#include "yvex_kv_render.h"

#include "yvex_cli_out.h"
#include "yvex_cli_table.h"

#include <string.h>

static const char *kv_render_str(const char *value, const char *fallback)
{
    return value ? value : fallback;
}

static void yvex_kv_render_phases(FILE *fp, const yvex_kv_report *report)
{
    unsigned long i;

    if (!report) {
        return;
    }
    for (i = 0u; i < report->phase_count; ++i) {
        yvex_cli_out_writef(fp, "kv_phase.%lu.name: %s\n",
                            i,
                            kv_render_str(report->phases[i].name, ""));
        yvex_cli_out_writef(fp, "kv_phase.%lu.status: %s\n",
                            i,
                            kv_render_str(report->phases[i].status, ""));
    }
}

static void yvex_kv_render_blockers(FILE *fp, const yvex_kv_report *report)
{
    unsigned long i;

    if (!report) {
        return;
    }
    if (report->blocker_count == 0u) {
        yvex_cli_out_kv_str(fp, "kv_blockers",
                            "real attention-backed KV writes unsupported; "
                            "decode KV consumer unsupported; KV capacity "
                            "estimator pending; context class report pending; "
                            "full transformer prefill unsupported");
        return;
    }
    yvex_cli_out_writef(fp, "kv_blockers:");
    for (i = 0u; i < report->blocker_count; ++i) {
        yvex_cli_out_writef(fp, "%s %s %s",
                            i == 0u ? "" : ";",
                            kv_render_str(report->blockers[i].name, "blocker"),
                            kv_render_str(report->blockers[i].status, "blocked"));
    }
    yvex_cli_out_writef(fp, "\n");
}

static void yvex_kv_render_report_normal(FILE *fp,
                                         const yvex_kv_report *report)
{
    const char *status = report ? report->kv_class_status : "unknown";

    if (report && report->status &&
        (strcmp(report->status, "fail") == 0 ||
         strcmp(report->status, "unsupported") == 0)) {
        status = report->status;
    }

    yvex_cli_out_kv_str(fp, "report", "kv");
    yvex_cli_out_kv_str(fp, "model",
                        report ? kv_render_str(report->model, "") : "");
    yvex_cli_out_kv_str(fp, "family",
                        report ? kv_render_str(report->family, "unknown") :
                            "unknown");
    yvex_cli_out_kv_str(fp, "status", kv_render_str(status, "unknown"));
    yvex_cli_out_kv_str(fp, "top_blocker",
                        report ? kv_render_str(report->top_blocker,
                                               "real attention-backed KV unsupported") :
                            "real attention-backed KV unsupported");
    yvex_cli_out_kv_str(fp, "next", "V010.KV.*");
    yvex_cli_out_kv_str(fp, "boundary",
                        "report-only, no runtime execution");
}

static void yvex_kv_render_ownership(FILE *fp,
                                     const yvex_kv_report *report)
{
    const yvex_kv_summary *summary = report ? &report->ownership_summary : NULL;
    unsigned long long i;

    yvex_cli_out_line(fp, "kv: ownership");
    yvex_cli_out_kv_bool(fp, "kv_created", report && report->kv_created);
    yvex_cli_out_kv_bool(fp, "session_owned",
                         report && report->session_owned);
    yvex_cli_out_kv_u64(fp, "layers", summary ? summary->layer_count : 0ull);
    yvex_cli_out_kv_u64(fp, "heads", summary ? summary->kv_head_count : 0ull);
    yvex_cli_out_kv_u64(fp, "head_dim", summary ? summary->head_dim : 0ull);
    yvex_cli_out_kv_u64(fp, "capacity",
                        summary ? summary->context_length : 0ull);
    yvex_cli_out_kv_str(fp, "dtype",
                        summary ? kv_render_str(summary->dtype, "F32") : "F32");
    yvex_cli_out_kv_u64(fp, "values_per_position",
                        summary ? summary->values_per_position : 0ull);
    yvex_cli_out_kv_u64(fp, "bytes_per_position",
                        summary ? summary->bytes_per_position : 0ull);
    yvex_cli_out_kv_u64(fp, "planned_bytes",
                        summary ? summary->bytes : 0ull);
    yvex_cli_out_kv_u64(fp, "allocated_bytes",
                        summary ? summary->allocated_bytes : 0ull);
    yvex_cli_out_kv_u64(fp, "append_count",
                        summary ? summary->append_count : 0ull);
    yvex_cli_out_kv_u64(fp, "read_count",
                        summary ? summary->read_count : 0ull);
    yvex_cli_out_kv_u64(fp, "written_positions",
                        summary ? summary->written_positions : 0ull);
    yvex_cli_out_kv_u64(fp, "last_appended_position",
                        report ? report->last_appended_position : 0ull);
    if (report && report->read_requested) {
        yvex_cli_out_kv_u64(fp, "read_position", report->read_position);
        yvex_cli_out_kv_u64(fp, "read_value_count", report->read_value_count);
        yvex_cli_out_kv_u64(fp, "read_checksum", report->read_checksum);
        yvex_cli_out_writef(fp, "read_sample_values:");
        for (i = 0ull; i < report->read_sample_count; ++i) {
            yvex_cli_out_writef(fp, "%s%.9g",
                                i == 0ull ? " " : ",",
                                (double)report->read_sample_values[i]);
        }
        yvex_cli_out_writef(fp, "\n");
    } else {
        yvex_cli_out_kv_str(fp, "read_position", "not-requested");
        yvex_cli_out_kv_u64(fp, "read_value_count", 0ull);
        yvex_cli_out_kv_u64(fp, "read_checksum", 0ull);
    }
    yvex_cli_out_kv_str(fp, "overflow_status",
                        summary ? kv_render_str(summary->overflow_status,
                                                "not-overflowed") :
                            "not-overflowed");
    yvex_cli_out_kv_bool(fp, "cleanup_attempted",
                         report && report->cleanup_attempted);
    yvex_cli_out_kv_str(fp, "cleanup_status",
                        report ? kv_render_str(report->cleanup_status, "pass") :
                            "pass");
    yvex_cli_out_kv_bool(fp, "decode_ready", 0);
    yvex_cli_out_kv_bool(fp, "logits_ready", 0);
    yvex_cli_out_kv_bool(fp, "generation_ready", 0);
    yvex_cli_out_kv_str(fp, "generation",
                        report ? kv_render_str(report->generation, "unsupported") :
                            "unsupported");
    yvex_cli_out_kv_str(fp, "status",
                        report ? kv_render_str(report->status, "kv-owned") :
                            "kv-owned");
}

static void yvex_kv_render_audit_report(FILE *fp,
                                        const yvex_kv_report *report)
{
    yvex_cli_out_line(fp, "kv: report");
    yvex_cli_out_kv_str(fp, "status",
                        report ? kv_render_str(report->status, "unknown") :
                            "unknown");
    yvex_cli_out_kv_str(fp, "model",
                        report ? kv_render_str(report->model, "none") : "none");
    yvex_cli_out_kv_str(fp, "model_resolved_path",
                        report ? kv_render_str(report->model_resolved_path,
                                               "not-resolved") :
                            "not-resolved");
    yvex_cli_out_kv_str(fp, "target_id",
                        report ? kv_render_str(report->target_id,
                                               "candidate-GGUF-path") :
                            "candidate-GGUF-path");
    yvex_cli_out_kv_str(fp, "target_class",
                        report ? kv_render_str(report->target_class, "unknown") :
                            "unknown");
    yvex_cli_out_kv_str(fp, "backend",
                        report ? kv_render_str(report->backend, "cpu") : "cpu");
    yvex_cli_out_kv_str(fp, "family",
                        report ? kv_render_str(report->family, "unknown") :
                            "unknown");
    yvex_cli_out_kv_str(fp, "family_detected",
                        report ? kv_render_str(report->family_detected,
                                               "unknown") :
                            "unknown");
    yvex_cli_out_kv_str(fp, "family_requested",
                        report ? kv_render_str(report->family_requested, "auto") :
                            "auto");
    yvex_cli_out_kv_str(fp, "family_runtime_status",
                        report ? kv_render_str(report->family_runtime_status,
                                               "unknown") :
                            "unknown");
    yvex_cli_out_kv_str(fp, "attention_class_status",
                        report ? kv_render_str(report->attention_class_status,
                                               "unknown") :
                            "unknown");
    yvex_cli_out_kv_str(fp, "kv_class_status",
                        report ? kv_render_str(report->kv_class_status,
                                               "report-only") :
                            "report-only");
    yvex_cli_out_kv_bool(fp, "report_options.include_attention",
                         report && report->include_attention);
    yvex_cli_out_kv_bool(fp, "report_options.include_context",
                         report && report->include_context);
    yvex_cli_out_kv_bool(fp, "report_options.include_residency",
                         report && report->include_residency);
    yvex_cli_out_kv_bool(fp, "report_options.include_blockers",
                         report && report->include_blockers);
    yvex_cli_out_kv_str(fp, "kv_stage",
                        report ? kv_render_str(report->kv_stage, "report-only") :
                            "report-only");
    yvex_cli_out_kv_str(fp, "kv_support_status",
                        report ? kv_render_str(report->kv_support_status,
                                               "report-only") :
                            "report-only");
    yvex_cli_out_kv_str(fp, "runtime_claim",
                        report ? kv_render_str(report->runtime_claim,
                                               "unsupported") :
                            "unsupported");
    yvex_cli_out_kv_bool(fp, "generation_ready",
                         report && report->generation_ready);
    yvex_cli_out_kv_str(fp, "generation",
                        report ? kv_render_str(report->generation,
                                               "unsupported-full-model") :
                            "unsupported-full-model");
    yvex_cli_out_kv_str(fp, "benchmark_status",
                        report ? kv_render_str(report->benchmark_status,
                                               "not-measured") :
                            "not-measured");
    yvex_cli_out_kv_bool(fp, "diagnostic_kv_available",
                         report && report->diagnostic_kv_available);
    yvex_cli_out_kv_str(fp, "diagnostic_kv_boundary",
                        report ? kv_render_str(report->diagnostic_kv_boundary,
                                               "segment-summary/minimal diagnostic KV") :
                            "segment-summary/minimal diagnostic KV");
    yvex_cli_out_kv_str(fp, "real_attention_kv",
                        report && report->real_attention_kv ?
                            "supported" : "unsupported");
    yvex_cli_out_kv_bool(fp, "real_attention_kv_write_ready",
                         report && report->real_attention_kv_write_ready);
    yvex_cli_out_kv_bool(fp, "real_attention_kv_read_ready",
                         report && report->real_attention_kv_read_ready);
    yvex_cli_out_kv_bool(fp, "decode_kv_consumer_ready",
                         report && report->decode_kv_consumer_ready);
    yvex_cli_out_kv_bool(fp, "kv_required", report && report->kv_required);
    yvex_cli_out_kv_str(fp, "kv_source",
                        report ? kv_render_str(report->kv_source,
                                               "attention-qkv-requirements") :
                            "attention-qkv-requirements");
    yvex_cli_out_kv_str(fp, "kv_layout",
                        report ? kv_render_str(report->kv_layout, "planned") :
                            "planned");
    yvex_cli_out_kv_str(fp, "kv_layout_status",
                        report ? kv_render_str(report->kv_layout_status,
                                               "planned") :
                            "planned");
    yvex_cli_out_kv_str(fp, "kv_dtype",
                        report ? kv_render_str(report->kv_dtype, "planned") :
                            "planned");
    yvex_cli_out_kv_str(fp, "kv_dtype_status",
                        report ? kv_render_str(report->kv_dtype_status,
                                               "planned") :
                            "planned");
    yvex_cli_out_kv_str(fp, "kv_layers",
                        report ? kv_render_str(report->kv_layers, "unknown") :
                            "unknown");
    yvex_cli_out_kv_str(fp, "kv_layers_status",
                        report ? kv_render_str(report->kv_layers_status,
                                               "planned") :
                            "planned");
    yvex_cli_out_kv_str(fp, "kv_heads",
                        report ? kv_render_str(report->kv_heads, "unknown") :
                            "unknown");
    yvex_cli_out_kv_str(fp, "kv_heads_status",
                        report ? kv_render_str(report->kv_heads_status,
                                               "planned") :
                            "planned");
    yvex_cli_out_kv_str(fp, "kv_head_dim",
                        report ? kv_render_str(report->kv_head_dim, "unknown") :
                            "unknown");
    yvex_cli_out_kv_str(fp, "kv_head_dim_status",
                        report ? kv_render_str(report->kv_head_dim_status,
                                               "planned") :
                            "planned");
    yvex_cli_out_kv_str(fp, "kv_positions",
                        report ? kv_render_str(report->kv_positions,
                                               "context-dependent") :
                            "context-dependent");
    yvex_cli_out_kv_str(fp, "kv_capacity",
                        report ? kv_render_str(report->kv_capacity, "planned") :
                            "planned");
    yvex_cli_out_kv_str(fp, "kv_capacity_status",
                        report ? kv_render_str(report->kv_capacity_status,
                                               "planned") :
                            "planned");
    yvex_cli_out_kv_str(fp, "kv_indexing",
                        report ? kv_render_str(report->kv_indexing,
                                               "layer-head-position-token-order") :
                            "layer-head-position-token-order");
    yvex_cli_out_kv_str(fp, "kv_residency_class",
                        report ? kv_render_str(report->kv_residency_class,
                                               "planned") :
                            "planned");
    yvex_cli_out_kv_str(fp, "kv_residency_status",
                        report ? kv_render_str(report->kv_residency_status,
                                               "planned") :
                            "planned");
    yvex_cli_out_kv_str(fp, "context_required",
                        report ? kv_render_str(report->context_required, "true") :
                            "true");
    yvex_cli_out_kv_str(fp, "context_length_source",
                        report ? kv_render_str(report->context_length_source,
                                               "planned") :
                            "planned");
    if (report && report->max_context_seen) {
        yvex_cli_out_kv_u64(fp, "max_context", report->max_context);
    } else {
        yvex_cli_out_kv_str(fp, "max_context", "unknown");
    }
    yvex_cli_out_kv_str(fp, "attention_dependency_status",
                        report ? kv_render_str(report->attention_dependency_status,
                                               "blocked-missing-qkv") :
                            "blocked-missing-qkv");
    yvex_cli_out_kv_bool(fp, "attention_q_required",
                         report && report->attention_q_required);
    yvex_cli_out_kv_bool(fp, "attention_k_required",
                         report && report->attention_k_required);
    yvex_cli_out_kv_bool(fp, "attention_v_required",
                         report && report->attention_v_required);
    yvex_cli_out_kv_str(fp, "attention_q_status",
                        report ? kv_render_str(report->attention_q_status,
                                               "missing") :
                            "missing");
    yvex_cli_out_kv_str(fp, "attention_k_status",
                        report ? kv_render_str(report->attention_k_status,
                                               "missing") :
                            "missing");
    yvex_cli_out_kv_str(fp, "attention_v_status",
                        report ? kv_render_str(report->attention_v_status,
                                               "missing") :
                            "missing");
    yvex_cli_out_kv_str(fp, "attention_o_status",
                        report ? kv_render_str(report->attention_o_status,
                                               "missing") :
                            "missing");
    yvex_cli_out_kv_str(fp, "tensor_inventory_status",
                        report ? kv_render_str(report->tensor_inventory_status,
                                               "not-performed") :
                            "not-performed");
    if (report && report->source_artifact_class &&
        report->source_artifact_class[0] != '\0') {
        yvex_cli_out_kv_str(fp, "source_artifact_class",
                            report->source_artifact_class);
    }
    if (report && report->target_artifact_class &&
        report->target_artifact_class[0] != '\0') {
        yvex_cli_out_kv_str(fp, "target_artifact_class",
                            report->target_artifact_class);
    }
    yvex_cli_out_kv_u64(fp, "tensor_count",
                        report ? report->tensor_count : 0ull);
    yvex_cli_out_kv_u64(fp, "tensor_bytes",
                        report ? report->tensor_bytes : 0ull);
    yvex_cli_out_kv_str(fp, "role.token_embedding.status",
                        report ? kv_render_str(report->role_token_embedding_status,
                                               "missing") :
                            "missing");
    yvex_cli_out_kv_str(fp, "role.attention_norm.status",
                        report ? kv_render_str(report->role_attention_norm_status,
                                               "missing") :
                            "missing");
    yvex_cli_out_kv_str(fp, "role.q_projection.status",
                        report ? kv_render_str(report->role_q_projection_status,
                                               "missing") :
                            "missing");
    yvex_cli_out_kv_str(fp, "role.k_projection.status",
                        report ? kv_render_str(report->role_k_projection_status,
                                               "missing") :
                            "missing");
    yvex_cli_out_kv_str(fp, "role.v_projection.status",
                        report ? kv_render_str(report->role_v_projection_status,
                                               "missing") :
                            "missing");
    yvex_cli_out_kv_str(fp, "role.o_projection.status",
                        report ? kv_render_str(report->role_o_projection_status,
                                               "missing") :
                            "missing");
    yvex_cli_out_kv_str(fp, "role.output_head.status",
                        report ? kv_render_str(report->role_output_head_status,
                                               "missing") :
                            "missing");
    yvex_cli_out_kv_str(fp, "prefill_kv_write_ready",
                        report ? kv_render_str(report->prefill_kv_write_ready,
                                               "false") :
                            "false");
    yvex_cli_out_kv_str(fp, "decode_kv_read_ready",
                        report ? kv_render_str(report->decode_kv_read_ready,
                                               "false") :
                            "false");
    yvex_cli_out_kv_str(fp, "qkv_role_coverage",
                        report ? kv_render_str(report->qkv_role_coverage,
                                               "missing") :
                            "missing");
    yvex_cli_out_kv_bool(fp, "backend_allocation_attempted",
                         report && report->backend_allocation_attempted);
    yvex_cli_out_kv_bool(fp, "full_kv_allocation_proof",
                         report && report->full_kv_allocation_proof);
    yvex_cli_out_kv_bool(fp, "cuda_full_kv_allocation_proof",
                         report && report->cuda_full_kv_allocation_proof);
    yvex_cli_out_kv_bool(fp, "paged_kv_implementation",
                         report && report->paged_kv_implementation);
    yvex_cli_out_kv_bool(fp, "chunked_kv_runtime_implementation",
                         report && report->chunked_kv_runtime_implementation);
    yvex_cli_out_kv_bool(fp, "ssd_backed_kv",
                         report && report->ssd_backed_kv);
    yvex_cli_out_kv_bool(fp, "quantized_kv_runtime",
                         report && report->quantized_kv_runtime);
    yvex_cli_out_kv_bool(fp, "full_transformer_prefill_ready",
                         report && report->full_transformer_prefill_ready);
    yvex_cli_out_kv_bool(fp, "decode_ready",
                         report && report->decode_ready);
    yvex_cli_out_kv_bool(fp, "logits_ready",
                         report && report->logits_ready);
    yvex_cli_out_kv_bool(fp, "sampling_ready",
                         report && report->sampling_ready);
    yvex_cli_out_kv_bool(fp, "runtime_execution_ready",
                         report && report->runtime_execution_ready);
    yvex_kv_render_blockers(fp, report);
    yvex_cli_out_kv_str(fp, "next_required_rows",
                        report ? kv_render_str(report->next_required_rows, "") :
                            "");
    yvex_cli_out_kv_bool(fp, "cleanup_attempted",
                         report && report->cleanup_attempted);
    yvex_cli_out_kv_str(fp, "cleanup_status",
                        report ? kv_render_str(report->cleanup_status, "pass") :
                            "pass");
    yvex_kv_render_phases(fp, report);
}

/*
 * yvex_kv_render_normal()
 *
 * Purpose:
 *   render compact KV normal output from typed report facts.
 *
 * Inputs:
 *   fp is a borrowed stream and report is a borrowed typed report.
 *
 * Effects:
 *   writes compact normal text through CLI writer helpers.
 *
 * Failure:
 *   stream write failures are left to the caller's stream state.
 *
 * Boundary:
 *   normal output is report text only and does not imply runtime support.
 */
int yvex_kv_render_normal(FILE *fp, const yvex_kv_report *report)
{
    if (report && report->kind == YVEX_KV_REQUEST_OWNERSHIP) {
        yvex_kv_render_ownership(fp, report);
    } else {
        yvex_kv_render_report_normal(fp, report);
    }
    return 0;
}

/*
 * yvex_kv_render_table()
 *
 * Purpose:
 *   render a compact table-shaped KV summary from typed facts.
 *
 * Inputs:
 *   fp is a borrowed stream and report is a borrowed typed report.
 *
 * Effects:
 *   writes table rows through CLI table helpers only.
 *
 * Failure:
 *   stream write failures are left to the caller's stream state.
 *
 * Boundary:
 *   table layout is not additional KV capability.
 */
int yvex_kv_render_table(FILE *fp, const yvex_kv_report *report)
{
    if (report && report->kind == YVEX_KV_REQUEST_OWNERSHIP) {
        yvex_kv_render_ownership(fp, report);
        return 0;
    }
    yvex_kv_render_report_normal(fp, report);
    return 0;
}

/*
 * yvex_kv_render_audit()
 *
 * Purpose:
 *   render full KV audit facts from a typed report.
 *
 * Inputs:
 *   fp is a borrowed stream and report is a borrowed typed report.
 *
 * Effects:
 *   writes audit text through CLI writer helpers only.
 *
 * Failure:
 *   stream write failures are left to the caller's stream state.
 *
 * Boundary:
 *   audit output carries evidence but does not create runtime support.
 */
int yvex_kv_render_audit(FILE *fp, const yvex_kv_report *report)
{
    if (report && report->kind == YVEX_KV_REQUEST_OWNERSHIP) {
        yvex_kv_render_ownership(fp, report);
    } else {
        yvex_kv_render_audit_report(fp, report);
    }
    return 0;
}

/*
 * yvex_kv_render()
 *
 * Purpose:
 *   dispatch typed KV report rendering by requested output mode.
 *
 * Inputs:
 *   fp is a borrowed stream; mode is parsed CLI output mode; report is typed
 *   report data.
 *
 * Effects:
 *   writes normal, table, or audit text through renderer functions.
 *
 * Failure:
 *   invalid mode falls back to normal rendering.
 *
 * Boundary:
 *   render dispatch is not command execution.
 */
int yvex_kv_render(FILE *fp,
                   yvex_kv_report_mode mode,
                   const yvex_kv_report *report)
{
    switch (mode) {
    case YVEX_KV_REPORT_MODE_TABLE:
        return yvex_kv_render_table(fp, report);
    case YVEX_KV_REPORT_MODE_AUDIT:
        return yvex_kv_render_audit(fp, report);
    case YVEX_KV_REPORT_MODE_NORMAL:
    default:
        return yvex_kv_render_normal(fp, report);
    }
}

/*
 * yvex_kv_render_help()
 *
 * Purpose:
 *   render KV command help text through CLI writer helpers.
 *
 * Inputs:
 *   fp is a borrowed output stream.
 *
 * Effects:
 *   writes help text only and does not inspect model or KV state.
 *
 * Failure:
 *   stream write failures are left to the caller's stream state.
 *
 * Boundary:
 *   help text is not proof of lower runtime capability.
 */
int yvex_kv_render_help(FILE *fp)
{
    yvex_cli_out_writef(
        fp,
        "usage: yvex kv report --model FILE_OR_ALIAS "
        "[--family auto|deepseek|glm|qwen|llama] [--backend cpu|cuda] "
        "[--audit | --output normal|table|audit] [options]\n"
        "usage: yvex kv --layers N --heads N --head-dim N --capacity N "
        "[--append-demo] [--read-position N]\n\n"
        "kv report:\n"
        "  KV cache class and requirements report over model/family facts.\n"
        "  Reports layout, dtype, layer/head/position indexing, capacity, "
        "context dependency, residency class, attention dependency, and blockers.\n"
        "  This is a report-only boundary: it does not allocate full runtime KV, "
        "write real attention-backed KV, execute decode, generate, evaluate, "
        "benchmark, or report throughput.\n"
        "  Existing diagnostic KV is segment-summary/minimal only and is not "
        "DeepSeek KV, real attention KV, full transformer KV, or decode-ready KV.\n"
        "  Options: --include-attention --include-context --include-residency "
        "--include-blockers --registry FILE\n"
        "  Default report output is compact. Use --audit for full diagnostic fields.\n\n"
        "minimal KV diagnostic:\n"
        "  --append-demo allocates a minimal F32 session-owned KV store and "
        "reports lifecycle/bounds facts.\n"
        "  It remains diagnostic/minimal and does not run attention, decode, "
        "logits, sampling, generation, or prefill.\n");
    return 0;
}
