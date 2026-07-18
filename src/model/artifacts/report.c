/*
 * report.c - model artifact report coordination and shared facts.
 *
 * Owner:
 *   src/model/artifacts
 *
 * Owns:
 *   typed model-artifact report request/result helpers, shared registry metadata
 *   facts, model-ref-to-registry views, and report build dispatch.
 *
 * Does not own:
 *   command argument parsing, command dispatch, renderer formatting, explicit registry
 *   file writing, artifact emission, runtime generation, eval, benchmark, or
 *   release decisions.
 *
 * Invariants:
 *   report facts are typed values; this module does not own command grammar or
 *   pre-rendered command output buffers.
 *
 * Boundary:
 *   model artifact reports are not artifact emission, runtime descriptors,
 *   generation readiness, benchmark evidence, or release readiness.
 */
#include "report.h"

#include <stdio.h>
#include <string.h>

#include <yvex/artifact_identity.h>
#include <yvex/model_registry.h>
#include <yvex/model_ref.h>
#include <yvex/api.h>

#include "gate.h"

static void artifact_report_clear(yvex_model_artifact_report *report)
{
    if (!report) return;
    memset(report, 0, sizeof(*report));
}

/*
 * Builds a typed machine-readable inventory view from canonical admission.
 * The result borrows admission strings and performs no parsing, IO, or support
 * inference from paths, names, or independent booleans.
 */
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

/* Contract: normalizes a typed report request without IO or capability promotion. */
static int artifact_report_build_kind(
    const yvex_model_artifact_report_request *request,
    yvex_model_artifact_report_kind kind,
    const char *operation,
    yvex_model_artifact_report *report,
    yvex_error *err)
{
    yvex_model_artifact_report_request normalized;

    if (!request || !report) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, operation,
                       "request and report are required");
        return YVEX_ERR_INVALID_ARG;
    }
    normalized = *request;
    normalized.kind = kind;
    return yvex_model_artifact_report_build(&normalized, report, err);
}

int yvex_model_artifact_check_report_build(
    const yvex_model_artifact_report_request *request,
    yvex_model_artifact_report *report,
    yvex_error *err)
{
    return artifact_report_build_kind(
        request, YVEX_MODEL_ARTIFACT_REPORT_CHECK,
        "model_artifact_check_report", report, err);
}

int yvex_model_artifact_list_report_build(
    const yvex_model_artifact_report_request *request,
    yvex_model_artifact_report *report,
    yvex_error *err)
{
    return artifact_report_build_kind(
        request, YVEX_MODEL_ARTIFACT_REPORT_LIST,
        "model_artifact_list_report", report, err);
}

int yvex_model_artifact_status_report_build(
    const yvex_model_artifact_report_request *request,
    yvex_model_artifact_report *report,
    yvex_error *err)
{
    return artifact_report_build_kind(
        request, YVEX_MODEL_ARTIFACT_REPORT_STATUS,
        "model_artifact_status_report", report, err);
}
