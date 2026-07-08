/*
 * yvex_model_target_candidates.c - model-target candidate report builder.
 *
 * Owner:
 *   src/model/target
 *
 * Owns:
 *   full-runtime candidate, dense candidate, and Qwen/Metal pressure report
 *   builder entry points.
 *
 * Does not own:
 *   CLI parsing, command dispatch, rendering, target catalog storage,
 *   sidecar writing, runtime execution, generation, eval, benchmark, or
 *   release decisions.
 *
 * Invariants:
 *   candidate reports remain blocked/report-only until promoted by separate
 *   implementation proof rows.
 *
 * Boundary:
 *   candidate reporting does not create runtime capability, quantization,
 *   artifact emission, generation, benchmark, or release readiness.
 */
#include "yvex_model_target_candidates.h"

#include "yvex_model_target_private.h"

static int model_target_candidate_kind_allowed(
    yvex_model_target_command_kind kind)
{
    return kind == YVEX_MODEL_TARGET_COMMAND_CANDIDATE ||
           kind == YVEX_MODEL_TARGET_COMMAND_DENSE_CANDIDATE ||
           kind == YVEX_MODEL_TARGET_COMMAND_QWEN_METAL;
}

static const char *model_target_candidate_kind_name(
    yvex_model_target_command_kind kind)
{
    if (kind == YVEX_MODEL_TARGET_COMMAND_DENSE_CANDIDATE) {
        return "dense-candidate";
    }
    if (kind == YVEX_MODEL_TARGET_COMMAND_QWEN_METAL) {
        return "qwen-metal";
    }
    return "candidate";
}

static int model_target_candidate_reject(yvex_error *err)
{
    yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_target_candidate",
                   "candidate report requires candidate command kind");
    return YVEX_ERR_INVALID_ARG;
}

static void model_target_candidate_prepare_report(
    const yvex_model_target_request *request,
    yvex_model_target_report *report)
{
    if (!request || !report) {
        return;
    }
    report->kind = request->kind;
    report->status = model_target_candidate_kind_name(request->kind);
}

int yvex_model_target_candidate_report_build(
    const yvex_model_target_request *request,
    yvex_model_target_report *report,
    yvex_error *err)
{
    if (!request || !model_target_candidate_kind_allowed(request->kind)) {
        return model_target_candidate_reject(err);
    }
    model_target_candidate_prepare_report(request, report);
    return yvex_model_target_internal_report_build(request, report, err);
}

/*
 * Ownership note:
 *   Candidate pressure facts remain in this module. The coordinator routes the
 *   command kind, but full-runtime, dense, and Qwen/Metal candidate report
 *   ownership is kept here so the CLI adapter does not regain target facts.
 */
