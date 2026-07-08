/*
 * yvex_model_target_decision.c - target decision report builder.
 *
 * Owner:
 *   src/model/target
 *
 * Owns:
 *   target-decision report routing and target-decision report construction
 *   entry point.
 *
 * Does not own:
 *   CLI parsing, command dispatch, rendering, target catalog storage,
 *   candidate report ownership, sidecar writing, runtime execution,
 *   generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   target-decision reports preserve existing v0.1.0 decision semantics and
 *   remain report-only.
 *
 * Boundary:
 *   target-decision facts do not select a runtime-ready model and do not imply
 *   quantization, artifact emission, generation, benchmark, or release
 *   readiness.
 */
#include "yvex_model_target_decision.h"

#include "yvex_model_target_private.h"

static int model_target_decision_kind_allowed(
    const yvex_model_target_request *request)
{
    return request &&
           request->kind == YVEX_MODEL_TARGET_COMMAND_DECISION;
}

static int model_target_decision_reject(yvex_error *err)
{
    yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_target_decision",
                   "target decision report requires decision command kind");
    return YVEX_ERR_INVALID_ARG;
}

static void model_target_decision_prepare_report(
    yvex_model_target_report *report)
{
    if (!report) {
        return;
    }
    report->kind = YVEX_MODEL_TARGET_COMMAND_DECISION;
    report->status = "target-decision";
}

/*
 * yvex_model_target_decision_report_build()
 *
 * Purpose:
 *   build the target-decision report through the model-target decision module.
 *
 * Inputs:
 *   request borrows parsed argv; report receives owned report segments.
 *
 * Effects:
 *   delegates to the shared model-target report backend for existing decision
 *   behavior; it does not render or write operator output.
 *
 * Failure:
 *   returns invalid-arg for non-decision request kinds or backend errors.
 *
 * Boundary:
 *   decision reporting is report-only and does not create runtime support,
 *   quantization, artifact emission, generation, eval, benchmark, or release
 *   readiness.
 */
int yvex_model_target_decision_report_build(
    const yvex_model_target_request *request,
    yvex_model_target_report *report,
    yvex_error *err)
{
    if (!model_target_decision_kind_allowed(request)) {
        return model_target_decision_reject(err);
    }
    model_target_decision_prepare_report(report);
    return yvex_model_target_runner_report_build(request, report, err);
}
