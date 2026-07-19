/* Owner: src/cli/render
 * Owns: normal, table, audit, and help serialization for typed KV reports.
 * Does not own: KV report construction, input parsing, command dispatch, KV cache mutation, attention execution,
 *   decode, logits, sampling, generation, eval, benchmark, or release decisions.
 * Invariants: renderers write only through src/cli/io helpers and never mutate reports.
 * Boundary: rendering KV facts is not runtime KV support or generation readiness.
 * Purpose: provide normal, table, audit, and help serialization for typed KV reports.
 * Inputs: typed domain facts, requested output mode, and caller-owned render state.
 * Effects: formats admitted facts through CLI I/O without changing domain state.
 * Failure: formatting or I/O refusal cannot alter capability facts. */
#include "src/cli/render/private.h"

#include "src/cli/io/private.h"

#include <string.h>

/* Purpose: Render kv render str from typed facts (`kv_render_str`). */
static const char *kv_render_str(const char *value, const char *fallback)
{
    return value ? value : fallback;
}

#define KV_FIELD(K, F, T, D) {K, T, offsetof(yvex_kv_report, F), D}

static const yvex_render_field_spec kv_identity_fields[] = {
    KV_FIELD("status", status, YVEX_RENDER_FIELD_TEXT, "unknown"),
    KV_FIELD("model", model, YVEX_RENDER_FIELD_TEXT, "none"),
    KV_FIELD("model_resolved_path", model_resolved_path,
             YVEX_RENDER_FIELD_TEXT, "not-resolved"),
    KV_FIELD("target_id", target_id, YVEX_RENDER_FIELD_TEXT, "candidate-GGUF-path"),
    KV_FIELD("target_class", target_class, YVEX_RENDER_FIELD_TEXT, "unknown"),
    KV_FIELD("backend", backend, YVEX_RENDER_FIELD_TEXT, "cpu"),
    KV_FIELD("family", family, YVEX_RENDER_FIELD_TEXT, "unknown"),
    KV_FIELD("family_detected", family_detected, YVEX_RENDER_FIELD_TEXT, "unknown"),
    KV_FIELD("family_requested", family_requested, YVEX_RENDER_FIELD_TEXT, "auto"),
    KV_FIELD("family_runtime_status", family_runtime_status,
             YVEX_RENDER_FIELD_TEXT, "unknown"),
    KV_FIELD("attention_class_status", attention_class_status,
             YVEX_RENDER_FIELD_TEXT, "unknown"),
    KV_FIELD("kv_class_status", kv_class_status, YVEX_RENDER_FIELD_TEXT, "report-only"),
    KV_FIELD("report_options.include_attention", include_attention,
             YVEX_RENDER_FIELD_BOOL, NULL),
    KV_FIELD("report_options.include_context", include_context,
             YVEX_RENDER_FIELD_BOOL, NULL),
    KV_FIELD("report_options.include_residency", include_residency,
             YVEX_RENDER_FIELD_BOOL, NULL),
    KV_FIELD("report_options.include_blockers", include_blockers,
             YVEX_RENDER_FIELD_BOOL, NULL),
    KV_FIELD("kv_stage", kv_stage, YVEX_RENDER_FIELD_TEXT, "report-only"),
    KV_FIELD("kv_support_status", kv_support_status,
             YVEX_RENDER_FIELD_TEXT, "report-only"),
    KV_FIELD("runtime_claim", runtime_claim, YVEX_RENDER_FIELD_TEXT, "unsupported"),
    KV_FIELD("generation_ready", generation_ready, YVEX_RENDER_FIELD_BOOL, NULL),
    KV_FIELD("generation", generation, YVEX_RENDER_FIELD_TEXT, "unsupported-full-model"),
    KV_FIELD("benchmark_status", benchmark_status, YVEX_RENDER_FIELD_TEXT, "not-measured"),
    KV_FIELD("diagnostic_kv_available", diagnostic_kv_available,
             YVEX_RENDER_FIELD_BOOL, NULL),
    KV_FIELD("diagnostic_kv_boundary", diagnostic_kv_boundary,
             YVEX_RENDER_FIELD_TEXT, "segment-summary/minimal diagnostic KV")
};

static const yvex_render_field_spec kv_identity_readiness_fields[] = {
    KV_FIELD("real_attention_kv_write_ready", real_attention_kv_write_ready,
             YVEX_RENDER_FIELD_BOOL, NULL),
    KV_FIELD("real_attention_kv_read_ready", real_attention_kv_read_ready,
             YVEX_RENDER_FIELD_BOOL, NULL),
    KV_FIELD("decode_kv_consumer_ready", decode_kv_consumer_ready,
             YVEX_RENDER_FIELD_BOOL, NULL)
};

static const yvex_render_field_spec kv_layout_fields[] = {
    KV_FIELD("kv_required", kv_required, YVEX_RENDER_FIELD_BOOL, NULL),
    KV_FIELD("kv_source", kv_source, YVEX_RENDER_FIELD_TEXT, "attention-qkv-requirements"),
    KV_FIELD("kv_layout", kv_layout, YVEX_RENDER_FIELD_TEXT, "planned"),
    KV_FIELD("kv_layout_status", kv_layout_status, YVEX_RENDER_FIELD_TEXT, "planned"),
    KV_FIELD("kv_dtype", kv_dtype, YVEX_RENDER_FIELD_TEXT, "planned"),
    KV_FIELD("kv_dtype_status", kv_dtype_status, YVEX_RENDER_FIELD_TEXT, "planned"),
    KV_FIELD("kv_layers", kv_layers, YVEX_RENDER_FIELD_TEXT, "unknown"),
    KV_FIELD("kv_layers_status", kv_layers_status, YVEX_RENDER_FIELD_TEXT, "planned"),
    KV_FIELD("kv_heads", kv_heads, YVEX_RENDER_FIELD_TEXT, "unknown"),
    KV_FIELD("kv_heads_status", kv_heads_status, YVEX_RENDER_FIELD_TEXT, "planned"),
    KV_FIELD("kv_head_dim", kv_head_dim, YVEX_RENDER_FIELD_TEXT, "unknown"),
    KV_FIELD("kv_head_dim_status", kv_head_dim_status, YVEX_RENDER_FIELD_TEXT, "planned"),
    KV_FIELD("kv_positions", kv_positions, YVEX_RENDER_FIELD_TEXT, "context-dependent"),
    KV_FIELD("kv_capacity", kv_capacity, YVEX_RENDER_FIELD_TEXT, "planned"),
    KV_FIELD("kv_capacity_status", kv_capacity_status, YVEX_RENDER_FIELD_TEXT, "planned"),
    KV_FIELD("kv_indexing", kv_indexing, YVEX_RENDER_FIELD_TEXT,
             "layer-head-position-token-order"),
    KV_FIELD("kv_residency_class", kv_residency_class,
             YVEX_RENDER_FIELD_TEXT, "planned"),
    KV_FIELD("kv_residency_status", kv_residency_status,
             YVEX_RENDER_FIELD_TEXT, "planned"),
    KV_FIELD("context_required", context_required, YVEX_RENDER_FIELD_TEXT, "true"),
    KV_FIELD("context_length_source", context_length_source,
             YVEX_RENDER_FIELD_TEXT, "planned")
};

static const yvex_render_field_spec kv_attention_fields[] = {
    KV_FIELD("attention_dependency_status", attention_dependency_status,
             YVEX_RENDER_FIELD_TEXT, "blocked-missing-qkv"),
    KV_FIELD("attention_q_required", attention_q_required, YVEX_RENDER_FIELD_BOOL, NULL),
    KV_FIELD("attention_k_required", attention_k_required, YVEX_RENDER_FIELD_BOOL, NULL),
    KV_FIELD("attention_v_required", attention_v_required, YVEX_RENDER_FIELD_BOOL, NULL),
    KV_FIELD("attention_q_status", attention_q_status, YVEX_RENDER_FIELD_TEXT, "missing"),
    KV_FIELD("attention_k_status", attention_k_status, YVEX_RENDER_FIELD_TEXT, "missing"),
    KV_FIELD("attention_v_status", attention_v_status, YVEX_RENDER_FIELD_TEXT, "missing"),
    KV_FIELD("attention_o_status", attention_o_status, YVEX_RENDER_FIELD_TEXT, "missing"),
    KV_FIELD("tensor_inventory_status", tensor_inventory_status,
             YVEX_RENDER_FIELD_TEXT, "not-performed")
};

static const yvex_render_field_spec kv_admission_fields[] = {
    KV_FIELD("tensor_count", tensor_count, YVEX_RENDER_FIELD_U64, NULL),
    KV_FIELD("tensor_bytes", tensor_bytes, YVEX_RENDER_FIELD_U64, NULL),
    KV_FIELD("role.token_embedding.status", role_token_embedding_status,
             YVEX_RENDER_FIELD_TEXT, "missing"),
    KV_FIELD("role.attention_norm.status", role_attention_norm_status,
             YVEX_RENDER_FIELD_TEXT, "missing"),
    KV_FIELD("role.q_projection.status", role_q_projection_status,
             YVEX_RENDER_FIELD_TEXT, "missing"),
    KV_FIELD("role.k_projection.status", role_k_projection_status,
             YVEX_RENDER_FIELD_TEXT, "missing"),
    KV_FIELD("role.v_projection.status", role_v_projection_status,
             YVEX_RENDER_FIELD_TEXT, "missing"),
    KV_FIELD("role.o_projection.status", role_o_projection_status,
             YVEX_RENDER_FIELD_TEXT, "missing"),
    KV_FIELD("role.output_head.status", role_output_head_status,
             YVEX_RENDER_FIELD_TEXT, "missing"),
    KV_FIELD("prefill_kv_write_ready", prefill_kv_write_ready,
             YVEX_RENDER_FIELD_TEXT, "false"),
    KV_FIELD("decode_kv_read_ready", decode_kv_read_ready,
             YVEX_RENDER_FIELD_TEXT, "false"),
    KV_FIELD("qkv_role_coverage", qkv_role_coverage, YVEX_RENDER_FIELD_TEXT, "missing"),
    KV_FIELD("backend_allocation_attempted", backend_allocation_attempted,
             YVEX_RENDER_FIELD_BOOL, NULL),
    KV_FIELD("full_kv_allocation_proof", full_kv_allocation_proof,
             YVEX_RENDER_FIELD_BOOL, NULL),
    KV_FIELD("cuda_full_kv_allocation_proof", cuda_full_kv_allocation_proof,
             YVEX_RENDER_FIELD_BOOL, NULL),
    KV_FIELD("paged_kv_implementation", paged_kv_implementation,
             YVEX_RENDER_FIELD_BOOL, NULL),
    KV_FIELD("chunked_kv_runtime_implementation", chunked_kv_runtime_implementation,
             YVEX_RENDER_FIELD_BOOL, NULL),
    KV_FIELD("ssd_backed_kv", ssd_backed_kv, YVEX_RENDER_FIELD_BOOL, NULL),
    KV_FIELD("quantized_kv_runtime", quantized_kv_runtime,
             YVEX_RENDER_FIELD_BOOL, NULL),
    KV_FIELD("full_transformer_prefill_ready", full_transformer_prefill_ready,
             YVEX_RENDER_FIELD_BOOL, NULL),
    KV_FIELD("decode_ready", decode_ready, YVEX_RENDER_FIELD_BOOL, NULL),
    KV_FIELD("logits_ready", logits_ready, YVEX_RENDER_FIELD_BOOL, NULL),
    KV_FIELD("sampling_ready", sampling_ready, YVEX_RENDER_FIELD_BOOL, NULL),
    KV_FIELD("runtime_execution_ready", runtime_execution_ready,
             YVEX_RENDER_FIELD_BOOL, NULL)
};

static const yvex_render_field_spec kv_admission_tail_fields[] = {
    KV_FIELD("next_required_rows", next_required_rows, YVEX_RENDER_FIELD_TEXT, ""),
    KV_FIELD("cleanup_attempted", cleanup_attempted, YVEX_RENDER_FIELD_BOOL, NULL),
    KV_FIELD("cleanup_status", cleanup_status, YVEX_RENDER_FIELD_TEXT, "pass")
};

#undef KV_FIELD

/* Purpose: Render render phases from typed facts (`render_phases`). */
static void render_phases(FILE *fp, const yvex_kv_report *report)
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

/* Purpose: Render render blockers from typed facts (`render_blockers`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static void render_blockers(FILE *fp, const yvex_kv_report *report)
{
    unsigned long i;

    if (!report) {
        return;
    }
    if (report->blocker_count == 0u) {
        yvex_cli_out_kv_str(fp, "kv_blockers",
                            "real attention-backed KV writes unsupported; decode KV consumer unsupported; "
                                "KV capacity estimator pending; context class report pending; full transformer "
                                "prefill unsupported");
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

/* Purpose: Render render report normal from typed facts (`render_report_normal`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static void render_report_normal(FILE *fp,
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

/* Purpose: Render render ownership from typed facts (`render_ownership`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static void render_ownership(FILE *fp,
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

/* Render audit identity, family, and capability-stage facts. */
/* Purpose: Render render audit identity from typed facts (`render_audit_identity`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static void render_audit_identity(FILE *fp, const yvex_kv_report *report)
{
    const yvex_kv_report empty = {0};
    const yvex_kv_report *facts = report ? report : &empty;

    yvex_cli_out_line(fp, "kv: report");
    render_object_fields(fp, facts, kv_identity_fields,
                         sizeof(kv_identity_fields) / sizeof(kv_identity_fields[0]));
    yvex_cli_out_kv_str(fp, "real_attention_kv",
                        facts->real_attention_kv ? "supported" : "unsupported");
    render_object_fields(fp, facts, kv_identity_readiness_fields,
                         sizeof(kv_identity_readiness_fields) /
                             sizeof(kv_identity_readiness_fields[0]));
}

/* Render immutable KV geometry and attention dependency facts. */
/* Purpose: Render render audit layout from typed facts (`render_audit_layout`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static void render_audit_layout(FILE *fp, const yvex_kv_report *report)
{
    const yvex_kv_report empty = {0};
    const yvex_kv_report *facts = report ? report : &empty;

    render_object_fields(fp, facts, kv_layout_fields,
                         sizeof(kv_layout_fields) / sizeof(kv_layout_fields[0]));
    if (facts->max_context_seen) {
        yvex_cli_out_kv_u64(fp, "max_context", facts->max_context);
    } else {
        yvex_cli_out_kv_str(fp, "max_context", "unknown");
    }
    render_object_fields(fp, facts, kv_attention_fields,
                         sizeof(kv_attention_fields) / sizeof(kv_attention_fields[0]));
}

/* Render tensor admission, downstream readiness, and cleanup facts. */
/* Purpose: Render render audit admission from typed facts (`render_audit_admission`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static void render_audit_admission(FILE *fp, const yvex_kv_report *report)
{
    const yvex_kv_report empty = {0};
    const yvex_kv_report *facts = report ? report : &empty;

    if (facts->source_artifact_class && facts->source_artifact_class[0] != '\0') {
        yvex_cli_out_kv_str(fp, "source_artifact_class", facts->source_artifact_class);
    }
    if (facts->target_artifact_class && facts->target_artifact_class[0] != '\0') {
        yvex_cli_out_kv_str(fp, "target_artifact_class", facts->target_artifact_class);
    }
    render_object_fields(fp, facts, kv_admission_fields,
                         sizeof(kv_admission_fields) / sizeof(kv_admission_fields[0]));
    render_blockers(fp, report);
    render_object_fields(fp, facts, kv_admission_tail_fields,
                         sizeof(kv_admission_tail_fields) / sizeof(kv_admission_tail_fields[0]));
    render_phases(fp, report);
}

/* Purpose: Render render audit report from typed facts (`render_audit_report`). */
static void render_audit_report(FILE *fp, const yvex_kv_report *report)
{
    render_audit_identity(fp, report);
    render_audit_layout(fp, report);
    render_audit_admission(fp, report);
}

/* Purpose: render compact KV normal output from typed report facts.
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_kv_render_normal(FILE *fp, const yvex_kv_report *report)
{
    if (report && report->kind == YVEX_KV_REQUEST_OWNERSHIP) {
        render_ownership(fp, report);
    } else {
        render_report_normal(fp, report);
    }
    return 0;
}

/* Purpose: render a compact table-shaped KV summary from typed facts.
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_kv_render_table(FILE *fp, const yvex_kv_report *report)
{
    if (report && report->kind == YVEX_KV_REQUEST_OWNERSHIP) {
        render_ownership(fp, report);
        return 0;
    }
    render_report_normal(fp, report);
    return 0;
}

/* Purpose: render full KV audit facts from a typed report.
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_kv_render_audit(FILE *fp, const yvex_kv_report *report)
{
    if (report && report->kind == YVEX_KV_REQUEST_OWNERSHIP) {
        render_ownership(fp, report);
    } else {
        render_audit_report(fp, report);
    }
    return 0;
}

/* Purpose: dispatch typed KV report rendering by requested output mode.
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
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

/* Purpose: render KV command help text through CLI writer helpers.
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_kv_render_help(FILE *fp)
{
    yvex_cli_out_writef(
        fp,
        "usage: yvex kv report --model FILE_OR_ALIAS [--family auto|deepseek|glm|qwen|llama] [--backend "
            "cpu|cuda] [--audit | --output normal|table|audit] [options]\nusage: yvex kv --layers N --heads N -"
            "-head-dim N --capacity N [--append-demo] [--read-position N]\n\nkv report:\n  KV cache class and "
            "requirements report over model/family facts.\n  Reports layout, dtype, layer/head/position "
            "indexing, capacity, context dependency, residency class, attention dependency, and blockers.\n  "
            "This is a report-only boundary: it does not allocate full runtime KV, write real attention-backed "
            "KV, execute decode, generate, evaluate, benchmark, or report throughput.\n  Existing diagnostic "
            "KV is segment-summary/minimal only and is not DeepSeek KV, real attention KV, full transformer KV,"
            " or decode-ready KV.\n  Options: --include-attention --include-context --include-residency --"
            "include-blockers --registry FILE\n  Default report output is compact. Use --audit for full "
            "diagnostic fields.\n\nminimal KV diagnostic:\n  --append-demo allocates a minimal F32 session-"
            "owned KV store and reports lifecycle/bounds facts.\n  It remains diagnostic/minimal and does not "
            "run attention, decode, logits, sampling, generation, or prefill.\n");
    return 0;
}
