/*
 * yvex_qtype_role_support_report.c - qtype role-support report builder.
 *
 * Owner:
 *   src/model/target
 *
 * Owns:
 *   qtype role-support report construction entry point and gate report routing.
 *
 * Does not own:
 *   CLI parsing, rendering, quantization execution, artifact emission, runtime
 *   execution, generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   role-support matrices are report-only facts and hand off incomplete
 *   quantization work to later rows.
 *
 * Boundary:
 *   qtype role-support reporting is not qtype support completion,
 *   quantization, artifact emission, runtime readiness, generation readiness,
 *   benchmark evidence, or release readiness.
 */
#include "yvex_qtype_role_support_report.h"

#include "yvex_model_target_private.h"

static int qtype_role_support_kind_allowed(
    const yvex_model_target_request *request)
{
    return request && request->kind == YVEX_MODEL_TARGET_COMMAND_QUANT_POLICY;
}

static int qtype_role_support_reject(yvex_error *err)
{
    yvex_error_set(err, YVEX_ERR_INVALID_ARG, "qtype_role_support_report",
                   "qtype role-support report requires quant-policy command kind");
    return YVEX_ERR_INVALID_ARG;
}

static void qtype_role_support_prepare_report(
    yvex_model_target_report *report)
{
    if (!report) {
        return;
    }
    report->kind = YVEX_MODEL_TARGET_COMMAND_QUANT_POLICY;
    report->status = "qtype-role-support";
}

int yvex_qtype_role_support_report_build(
    const yvex_model_target_request *request,
    yvex_model_target_report *report,
    yvex_error *err)
{
    if (!qtype_role_support_kind_allowed(request)) {
        return qtype_role_support_reject(err);
    }
    qtype_role_support_prepare_report(report);
    return yvex_model_target_runner_report_build(request, report, err);
}

/*
 * Ownership note:
 *   Role-support and gate requests for qtype planning route through this
 *   module. It reports existing support matrix facts only and hands incomplete
 *   quantization work to later rows. It does not perform quantization.
 *
 * Report boundary:
 *   Role support may describe planned storage/compute handling. It does not
 *   complete qtype support or emit quantized artifacts.
 *
 * Test boundary:
 *   Layout tests require this module to remain a real ownership file with an
 *   exported builder. Keeping the guard here prevents the coordinator from
 *   absorbing role-support request routing again.
 *
 * Runtime boundary:
 *   Role-support facts do not execute graph, backend, decode, logits, or
 *   sampling paths.
 */
