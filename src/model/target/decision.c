/* Owner: src/model/target
 * Owns: the sole v0.1.0 release-target selection, refusal facts, and current source-to-mapping closure and
 *   payload-streaming handoff facts.
 * Does not own: CLI parsing, command dispatch, rendering, target catalog storage, candidate report ownership,
 *   sidecar writing, runtime execution, generation, eval, benchmark, or release decisions.
 * Invariants: the release decision selects exactly one canonical target while typed architecture and model support
 *   remain separate gates.
 * Boundary: target-decision facts do not select a runtime-ready model and do not imply quantization, artifact
 *   emission, generation, benchmark, or release readiness.
 * Purpose: derive target decision facts without executing downstream capability.
 * Inputs: typed target requests and canonical catalog rows.
 * Effects: mutates only bounded report state.
 * Failure: invalid requests produce typed report refusals. */
#include <yvex/internal/model_target.h>

#include <string.h>

typedef struct {
    const char *id;
    const char *class_name;
    const char *status;
    const char *reason;
    const char *next;
} decision_candidate;

static const decision_candidate decision_candidates[] = {
    {YVEX_SOURCE_RELEASE_TARGET_ID, "release-source-target",
     "selected-mapping-specified",
     "sole v0.1.0 target; support remains blocked by payload trust and downstream gates",
     "V010.SOURCE.PAYLOAD.STREAM.0"},
};

static const char *const decision_tail_rows[] = {
    "release_qtype: unselected",
    "artifact_status: not-produced"
};

static const char *const decision_help_rows[] = {
    "does not download models, emit artifacts, materialize tensors, execute "
    "graph work, run prefill, decode, logits, sampling, generation, "
    "evaluation, or benchmarks",
    "DeepSeek-V4-Flash is the sole release target. Qwen, Gemma, selected "
    "slices, source pressure targets, external references, and fixtures are "
    "engineering evidence, not alternate release choices."
};

static const char *const decision_audit_prefix[] = {
    "target_decision: v0.1.0",
    "status: target-selected-mapping-specified",
    "decision_state: selected"
};

static const char *const decision_audit_status_rows[] = {
    "source_verification_status: complete",
    "architecture_ir_status: complete",
    "tensor_coverage_status: complete",
    "gguf_mapping_status: complete",
    "full_runtime_candidate_status: unsupported",
    "selected_runtime_slice_eligible: false",
    "source_only_eligible: false",
    "external_reference_eligible: false"
};

static const char *const decision_audit_suffix[] = {
    "qwen_engineering_scope: preserved-non-release",
    "gemma_engineering_scope: preserved-non-release",
    "selected_slice_scope: bounded-evidence-only",
    "next_required_rows: V010.SOURCE.PAYLOAD.STREAM.0"
};

static const char *const decision_normal_prefix[] = {
    "report: target-decision",
    "status: target-selected-mapping-specified"
};

static const char *const decision_normal_suffix[] = {
    "top_blocker: source payload trust",
    "next: V010.SOURCE.PAYLOAD.STREAM.0",
    "boundary: release target selected; artifact/runtime/generation unsupported; "
    "benchmark not measured"
};

/* Purpose: project the immutable bounded decision candidate count view. */
static unsigned long decision_candidate_count(void)
{
    return sizeof(decision_candidates) / sizeof(decision_candidates[0]);
}

/* Purpose: resolve one decision find through the canonical index. */
static const decision_candidate *decision_find(const char *id)
{
    unsigned long i;

    if (!id || !id[0]) return NULL;
    for (i = 0; i < decision_candidate_count(); ++i) {
        if (strcmp(decision_candidates[i].id, id) == 0) {
            return &decision_candidates[i];
        }
    }
    return NULL;
}

/* Purpose: apply the canonical decision common tail transformation and invariants. */
static void decision_common_tail(yvex_model_target_report *report)
{
    yvex_model_target_report_add_rows(
        report, decision_tail_rows,
        sizeof(decision_tail_rows) / sizeof(decision_tail_rows[0]));
    yvex_model_target_report_common_tail(report);
}

/* Purpose: project decision help from typed facts without capability drift. */
static int decision_help(yvex_model_target_report *report)
{
    yvex_model_target_report_add_rows(
        report, decision_help_rows,
        sizeof(decision_help_rows) / sizeof(decision_help_rows[0]));
    return YVEX_OK;
}

/* Purpose: release owned decision unsupported release resources in dependency order.
 * Inputs: typed facts are borrowed.
 * Effects: updates bounded report or plan state.
 * Failure: preserves typed refusal and cleanup.
 * Boundary: never promotes payload or runtime execution. */
static int decision_unsupported_release(const yvex_model_target_request *request,
                                        yvex_model_target_report *report)
{
    report->exit_code = 2;
    report->status = "unsupported-release";
    yvex_model_target_report_add_row(report, "target_decision: %s",
                                     request->release[0] ? request->release : "missing");
    yvex_model_target_report_add_row(report, "status: unsupported-release");
    decision_common_tail(report);
    return YVEX_OK;
}

/* Purpose: apply the canonical decision missing candidate transformation and invariants. */
static int decision_missing_candidate(const yvex_model_target_request *request,
                                      yvex_model_target_report *report)
{
    report->exit_code = 2;
    report->status = "missing-candidate";
    yvex_model_target_report_add_row(report, "status: missing-candidate");
    yvex_model_target_report_add_row(report, "candidate_requested: %s",
                                     request->candidate_kind);
    yvex_model_target_report_add_row(report, "runtime_claim: unsupported");
    return YVEX_OK;
}

/* Purpose: publish decision emit candidate through the bounded output boundary. */
static void decision_emit_candidate(yvex_model_target_report *report,
                                    unsigned long index,
                                    const decision_candidate *candidate)
{
    yvex_model_target_report_add_row(report, "candidate.%lu.id: %s", index,
                                     candidate->id);
    yvex_model_target_report_add_row(report, "candidate.%lu.class: %s", index,
                                     candidate->class_name);
    yvex_model_target_report_add_row(report, "candidate.%lu.status: %s", index,
                                     candidate->status);
    yvex_model_target_report_add_row(report, "candidate.%lu.reason: %s", index,
                                     candidate->reason);
    yvex_model_target_report_add_row(report, "candidate.%lu.next: %s", index,
                                     candidate->next);
}

/* Purpose: project decision audit from typed facts without capability drift.
 * Inputs: typed facts are borrowed.
 * Effects: updates bounded report or plan state.
 * Failure: preserves typed refusal and cleanup.
 * Boundary: never promotes payload or runtime execution. */
static int decision_audit(const yvex_model_target_request *request,
                          yvex_model_target_report *report)
{
    unsigned long i;

    yvex_model_target_report_add_rows(
        report, decision_audit_prefix,
        sizeof(decision_audit_prefix) / sizeof(decision_audit_prefix[0]));
    yvex_model_target_report_add_row(report, "selected_target_id: %s",
                                     YVEX_SOURCE_RELEASE_TARGET_ID);
    yvex_model_target_report_add_row(report, "upstream_repository: %s",
                                     yvex_source_release_identity()->upstream_repo_id);
    yvex_model_target_report_add_rows(
        report, decision_audit_status_rows,
        sizeof(decision_audit_status_rows) / sizeof(decision_audit_status_rows[0]));
    decision_common_tail(report);
    if (request->candidate_kind[0]) {
        const decision_candidate *candidate = decision_find(request->candidate_kind);
        if (!candidate) {
            return decision_missing_candidate(request, report);
        }
        yvex_model_target_report_add_row(report, "candidate_count: 1");
        decision_emit_candidate(report, 0, candidate);
    } else {
        for (i = 0; i < decision_candidate_count(); ++i) {
            decision_emit_candidate(report, i, &decision_candidates[i]);
        }
    }
    yvex_model_target_report_add_rows(
        report, decision_audit_suffix,
        sizeof(decision_audit_suffix) / sizeof(decision_audit_suffix[0]));
    return YVEX_OK;
}

/* Purpose: construct bounded decision report build state from admitted inputs.
 * Inputs: typed facts are borrowed.
 * Effects: updates bounded report or plan state.
 * Failure: preserves typed refusal and cleanup.
 * Boundary: never promotes payload or runtime execution. */
int yvex_model_target_decision_report_build(
    const yvex_model_target_request *request,
    yvex_model_target_report *report,
    yvex_error *err)
{
    if (!request || !report ||
        request->kind != YVEX_MODEL_TARGET_COMMAND_DECISION) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_target_decision",
                       "target decision report requires decision command kind");
        return YVEX_ERR_INVALID_ARG;
    }
    report->kind = request->kind;
    report->mode = request->mode;
    report->help_requested = request->help_requested;
    if (request->help_requested) {
        return decision_help(report);
    }
    if (strcmp(request->release, "v0.1.0") != 0) {
        return decision_unsupported_release(request, report);
    }
    if (request->mode == YVEX_MODEL_TARGET_OUTPUT_JSON) {
        yvex_model_target_report_add_row(
            report,
            "{\"status\":\"target-selected-mapping-specified\","
            "\"release\":\"v0.1.0\",\"selected_target_id\":\"%s\","
            "\"upstream_repository\":\"%s\",\"source_verification\":\"complete\","
            "\"architecture_ir\":\"complete\",\"tensor_coverage\":\"complete\","
            "\"gguf_mapping\":\"complete\",\"release_qtype\":null,"
            "\"artifact_status\":\"not-produced\",\"runtime\":\"unsupported\","
            "\"generation\":\"unsupported\",\"evaluation\":\"not-run\","
            "\"benchmark\":\"not-measured\","
            "\"next\":\"V010.SOURCE.PAYLOAD.STREAM.0\"}",
            YVEX_SOURCE_RELEASE_TARGET_ID,
            yvex_source_release_identity()->upstream_repo_id);
        return YVEX_OK;
    }
    if (request->mode == YVEX_MODEL_TARGET_OUTPUT_TABLE) {
        yvex_model_target_report_add_row(report, "REPORT  STATUS  SELECTED  ELIGIBLE  NEXT");
        yvex_model_target_report_add_row(report,
                                         "target-decision  selected-mapping-specified  %s  0  "
                                         "V010.SOURCE.PAYLOAD.STREAM.0",
                                         YVEX_SOURCE_RELEASE_TARGET_ID);
        return YVEX_OK;
    }
    if (request->mode == YVEX_MODEL_TARGET_OUTPUT_AUDIT ||
        request->candidate_kind[0]) {
        return decision_audit(request, report);
    }
    yvex_model_target_report_add_rows(
        report, decision_normal_prefix,
        sizeof(decision_normal_prefix) / sizeof(decision_normal_prefix[0]));
    yvex_model_target_report_add_row(report, "selected: %s",
                                     YVEX_SOURCE_RELEASE_TARGET_ID);
    yvex_model_target_report_add_rows(
        report, decision_normal_suffix,
        sizeof(decision_normal_suffix) / sizeof(decision_normal_suffix[0]));
    return YVEX_OK;
}
