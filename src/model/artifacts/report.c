/* Owner: src/model/artifacts
 * Owns: typed model-artifact report request/result helpers, shared registry metadata facts, model-ref-to-registry
 *   views, and report build dispatch.
 * Does not own: command argument parsing, command dispatch, renderer formatting, explicit registry file writing,
 *   artifact emission, runtime generation, eval, benchmark, or release decisions.
 * Invariants: report facts are typed values; this module does not own command grammar or pre-rendered command
 *   output buffers.
 * Boundary: model artifact reports are not artifact emission, runtime descriptors, generation readiness, benchmark
 *   evidence, or release readiness.
 * Purpose: project canonical artifact admission into typed model reports.
 * Inputs: immutable admission facts and caller-owned report outputs.
 * Effects: mutates only bounded report state.
 * Failure: invalid admission produces a typed refusal without capability promotion. */
#include <yvex/internal/model_artifact.h>

#include <stdio.h>
#include <string.h>

#include <yvex/artifact.h>
#include <yvex/registry.h>

/* Purpose: release owned artifact report clear resources in dependency order. */
static void artifact_report_clear(yvex_model_artifact_report *report)
{
    if (!report) return;
    memset(report, 0, sizeof(*report));
}

/* Purpose: project artifact report from admission from typed facts without capability drift.
 * Inputs: artifact facts and outputs are explicit.
 * Effects: mutates only declared artifact ownership.
 * Failure: releases partial ownership on refusal.
 * Boundary: does not promote runtime execution support. */

int yvex_model_artifact_report_from_admission(
    const yvex_complete_artifact_admission *admission,
    yvex_model_artifact_report *report,
    yvex_error *err)
{
    yvex_model_complete_artifact_gate_fact gate;
    int rc;

    if (!report) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_artifact.report",
                       "report output is required");
        return YVEX_ERR_INVALID_ARG;
    }
    artifact_report_clear(report);
    rc = yvex_model_artifact_gate_from_admission(admission, &gate, err);
    if (rc != YVEX_OK) {
        report->kind = YVEX_MODEL_ARTIFACT_REPORT_STATUS;
        report->status = "blocked";
        report->exit_code = 2;
        report->support_level = "none";
        report->execution_ready = 0;
        report->reason = "canonical complete-artifact admission is absent";
        report->boundary = "tensor proofs and structural GGUF files are not complete artifacts";
        report->next_row = "V010.ARTIFACT.MATERIALIZE.0";
        return rc;
    }
    report->kind = YVEX_MODEL_ARTIFACT_REPORT_STATUS;
    report->status = "complete-artifact-admitted";
    report->exit_code = 0;
    report->artifact_class = yvex_artifact_class_name(
        admission->artifact_class);
    report->qprofile = gate.profile_name;
    report->path = gate.artifact_path;
    report->sha256 = gate.artifact_identity;
    report->file_size = gate.file_bytes;
    report->format = "gguf";
    report->tensor_count = gate.tensor_count;
    report->support_level = "descriptor-only";
    report->execution_ready = 0;
    report->integrity_status = "pass";
    report->materialization_status = "not-started";
    report->backend_status = "not-tested";
    report->reason = "canonical complete-artifact admission passed";
    report->boundary = "complete artifact ready for materialization; runtime unsupported";
    report->next_row = "V010.ARTIFACT.MATERIALIZE.0";
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: construct bounded artifact report build state from admitted inputs.
 * Inputs: artifact facts and outputs are explicit.
 * Effects: mutates only declared artifact ownership.
 * Failure: releases partial ownership on refusal.
 * Boundary: does not promote runtime execution support. */
int yvex_model_artifact_report_build(const yvex_model_artifact_report_request *request,
                                     yvex_model_artifact_report *report,
                                     yvex_error *err)
{
    if (!request || !report) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_artifact_report",
                       "request and report are required");
        return YVEX_ERR_INVALID_ARG;
    }
    artifact_report_clear(report);
    report->kind = request->kind;
    report->status = "report-only";
    report->exit_code = 0;
    report->alias = request->artifact_alias;
    report->path = request->artifact_path;
    report->family = request->expected_family;
    report->qprofile = request->expected_qprofile;
    report->execution_ready = 0;
    report->reason = "typed model artifact report dispatch is available";
    report->boundary = "model artifact report is not artifact emission or runtime generation";
    report->next_row = "topology cleanup";
    yvex_error_clear(err);
    return YVEX_OK;
}
