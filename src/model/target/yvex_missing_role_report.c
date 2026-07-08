/*
 * yvex_missing_role_report.c - missing-role report builder.
 *
 * Owner:
 *   src/model/target
 *
 * Owns:
 *   missing runtime-role blocker report construction entry point.
 *
 * Does not own:
 *   CLI parsing, rendering, runtime execution, generation, eval, benchmark, or
 *   release decisions.
 *
 * Invariants:
 *   missing-role reports describe blockers only and do not promote a target to
 *   runtime readiness.
 *
 * Boundary:
 *   missing-role reporting is not runtime support, generation readiness,
 *   benchmark evidence, or release readiness.
 */
#include "yvex_missing_role_report.h"

#include "yvex_model_target_private.h"

static int missing_role_kind_allowed(
    const yvex_model_target_request *request)
{
    return request &&
           (request->kind == YVEX_MODEL_TARGET_COMMAND_TENSOR_MAP ||
            request->kind == YVEX_MODEL_TARGET_COMMAND_MISSING_ROLES);
}

static int missing_role_reject(yvex_error *err)
{
    yvex_error_set(err, YVEX_ERR_INVALID_ARG, "missing_role_report",
                   "missing-role report requires tensor-map command kind");
    return YVEX_ERR_INVALID_ARG;
}

static void missing_role_prepare_report(yvex_model_target_report *report)
{
    if (!report) {
        return;
    }
    report->kind = YVEX_MODEL_TARGET_COMMAND_TENSOR_MAP;
    report->status = "missing-role-report";
}

int yvex_missing_role_report_build(
    const yvex_model_target_request *request,
    yvex_model_target_report *report,
    yvex_error *err)
{
    if (!missing_role_kind_allowed(request)) {
        return missing_role_reject(err);
    }
    missing_role_prepare_report(report);
    return yvex_model_target_runner_report_build(request, report, err);
}

/*
 * Ownership note:
 *   Missing-role blocker facts are owned by this module. This row preserves the
 *   existing output path through the shared backend, but request routing and
 *   status ownership no longer live in the coordinator or CLI adapter.
 *
 * Report boundary:
 *   Missing-role reports identify blockers only. They never turn lexical tensor
 *   names into runtime-ready weights.
 *
 * Test boundary:
 *   Layout tests require this module to remain a real ownership file with an
 *   exported builder. Keeping the guard here prevents the coordinator from
 *   absorbing missing-role request routing again.
 *
 * Runtime boundary:
 *   Blocker facts do not execute graph, backend, decode, logits, or sampling
 *   paths.
 */
