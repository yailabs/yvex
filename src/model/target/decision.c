/*
 * decision.c - target decision report builder.
 *
 * Owner:
 *   src/model/target
 *
 * Owns:
 *   the sole v0.1.0 release-target selection, refusal facts, and current
 *   source-to-mapping closure and payload-streaming handoff facts.
 *
 * Does not own:
 *   CLI parsing, command dispatch, rendering, target catalog storage,
 *   candidate report ownership, sidecar writing, runtime execution,
 *   generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   the release decision selects exactly one canonical target while typed
 *   architecture and model support remain separate gates.
 *
 * Boundary:
 *   target-decision facts do not select a runtime-ready model and do not imply
 *   quantization, artifact emission, generation, benchmark, or release
 *   readiness.
 */
#include "decision.h"

#include "catalog.h"

#include <string.h>

typedef struct {
    const char *id;
    const char *class_name;
    const char *status;
    const char *reason;
    const char *next;
} decision_candidate;

static const decision_candidate decision_candidates[] = {
    {YVEX_MODEL_RELEASE_TARGET_ID, "release-source-target",
     "selected-mapping-specified",
     "sole v0.1.0 target; support remains blocked by payload trust and downstream gates",
     "V010.SOURCE.PAYLOAD.STREAM.0"},
};

static unsigned long decision_candidate_count(void)
{
    return sizeof(decision_candidates) / sizeof(decision_candidates[0]);
}

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

static void decision_common_tail(yvex_model_target_report *report)
{
    yvex_model_target_report_add_row(report, "release_qtype: unselected");
    yvex_model_target_report_add_row(report, "artifact_status: not-produced");
    yvex_model_target_report_add_row(report, "runtime_claim: unsupported");
    yvex_model_target_report_add_row(report, "generation: unsupported-full-model");
    yvex_model_target_report_add_row(report, "benchmark_status: not-measured");
    yvex_model_target_report_add_row(report, "release_ready: false");
}

static int decision_help(yvex_model_target_report *report)
{
    yvex_model_target_report_add_row(report, "does not download models, emit artifacts, materialize tensors, execute graph work, run prefill, decode, logits, sampling, generation, evaluation, or benchmarks");
    yvex_model_target_report_add_row(report, "DeepSeek-V4-Flash is the sole release target. Qwen, Gemma, selected slices, source pressure targets, external references, and fixtures are engineering evidence, not alternate release choices.");
    return YVEX_OK;
}

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

static int decision_audit(const yvex_model_target_request *request,
                          yvex_model_target_report *report)
{
    unsigned long i;

    yvex_model_target_report_add_row(report, "target_decision: v0.1.0");
    yvex_model_target_report_add_row(report, "status: target-selected-mapping-specified");
    yvex_model_target_report_add_row(report, "decision_state: selected");
    yvex_model_target_report_add_row(report, "selected_target_id: %s",
                                     YVEX_MODEL_RELEASE_TARGET_ID);
    yvex_model_target_report_add_row(report, "upstream_repository: %s",
                                     yvex_model_target_release_identity()->upstream_repo_id);
    yvex_model_target_report_add_row(report, "source_verification_status: complete");
    yvex_model_target_report_add_row(report, "architecture_ir_status: complete");
    yvex_model_target_report_add_row(report, "tensor_coverage_status: complete");
    yvex_model_target_report_add_row(report, "gguf_mapping_status: complete");
    yvex_model_target_report_add_row(report, "full_runtime_candidate_status: unsupported");
    yvex_model_target_report_add_row(report, "selected_runtime_slice_eligible: false");
    yvex_model_target_report_add_row(report, "source_only_eligible: false");
    yvex_model_target_report_add_row(report, "external_reference_eligible: false");
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
    yvex_model_target_report_add_row(report, "qwen_engineering_scope: preserved-non-release");
    yvex_model_target_report_add_row(report, "gemma_engineering_scope: preserved-non-release");
    yvex_model_target_report_add_row(report, "selected_slice_scope: bounded-evidence-only");
    yvex_model_target_report_add_row(report,
                                     "next_required_rows: V010.SOURCE.PAYLOAD.STREAM.0");
    return YVEX_OK;
}

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
            "{\"status\":\"target-selected-mapping-specified\",\"release\":\"v0.1.0\",\"selected_target_id\":\"%s\",\"upstream_repository\":\"%s\",\"source_verification\":\"complete\",\"architecture_ir\":\"complete\",\"tensor_coverage\":\"complete\",\"gguf_mapping\":\"complete\",\"release_qtype\":null,\"artifact_status\":\"not-produced\",\"runtime\":\"unsupported\",\"generation\":\"unsupported\",\"evaluation\":\"not-run\",\"benchmark\":\"not-measured\",\"next\":\"V010.SOURCE.PAYLOAD.STREAM.0\"}",
            YVEX_MODEL_RELEASE_TARGET_ID,
            yvex_model_target_release_identity()->upstream_repo_id);
        return YVEX_OK;
    }
    if (request->mode == YVEX_MODEL_TARGET_OUTPUT_TABLE) {
        yvex_model_target_report_add_row(report, "REPORT  STATUS  SELECTED  ELIGIBLE  NEXT");
        yvex_model_target_report_add_row(report,
                                         "target-decision  selected-mapping-specified  %s  0  V010.SOURCE.PAYLOAD.STREAM.0",
                                         YVEX_MODEL_RELEASE_TARGET_ID);
        return YVEX_OK;
    }
    if (request->mode == YVEX_MODEL_TARGET_OUTPUT_AUDIT ||
        request->candidate_kind[0]) {
        return decision_audit(request, report);
    }
    yvex_model_target_report_add_row(report, "report: target-decision");
    yvex_model_target_report_add_row(report, "status: target-selected-mapping-specified");
    yvex_model_target_report_add_row(report, "selected: %s",
                                     YVEX_MODEL_RELEASE_TARGET_ID);
    yvex_model_target_report_add_row(report, "top_blocker: source payload trust");
    yvex_model_target_report_add_row(report,
                                     "next: V010.SOURCE.PAYLOAD.STREAM.0");
    yvex_model_target_report_add_row(report, "boundary: release target selected; artifact/runtime/generation unsupported; benchmark not measured");
    return YVEX_OK;
}
