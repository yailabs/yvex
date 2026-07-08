/*
 * yvex_qtype_policy_report.c - qtype policy report builder.
 *
 * Owner:
 *   src/model/target
 *
 * Owns:
 *   qtype policy report construction entry point.
 *
 * Does not own:
 *   CLI parsing, rendering, quantization execution, artifact emission, runtime
 *   execution, generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   qtype policy facts are planning/report-only and do not perform
 *   quantization.
 *
 * Boundary:
 *   qtype policy reporting is not quantization, artifact emission, runtime
 *   support, generation readiness, benchmark evidence, or release readiness.
 */
#include "yvex_qtype_policy_report.h"

#include "yvex_model_target_private.h"
#include "yvex_qtype_role_support_report.h"

#include <string.h>

static int quant_route_has_flag(const yvex_model_target_request *request,
                                const char *flag)
{
    int i;

    if (!request || !flag) {
        return 0;
    }
    for (i = 0; i < request->argc; ++i) {
        if (request->argv[i] && strcmp(request->argv[i], flag) == 0) {
            return 1;
        }
    }
    return 0;
}

static int qtype_policy_kind_allowed(
    const yvex_model_target_request *request)
{
    return request && request->kind == YVEX_MODEL_TARGET_COMMAND_QUANT_POLICY;
}

static int qtype_policy_reject(yvex_error *err)
{
    yvex_error_set(err, YVEX_ERR_INVALID_ARG, "qtype_policy_report",
                   "qtype policy report requires quant-policy command kind");
    return YVEX_ERR_INVALID_ARG;
}

static void qtype_policy_prepare_report(yvex_model_target_report *report)
{
    if (!report) {
        return;
    }
    report->kind = YVEX_MODEL_TARGET_COMMAND_QUANT_POLICY;
    report->status = "qtype-policy";
}

int yvex_qtype_policy_report_build(
    const yvex_model_target_request *request,
    yvex_model_target_report *report,
    yvex_error *err)
{
    if (!qtype_policy_kind_allowed(request)) {
        return qtype_policy_reject(err);
    }
    qtype_policy_prepare_report(report);
    return yvex_model_target_runner_report_build(request, report, err);
}

int yvex_model_target_quant_route_report_build(
    const yvex_model_target_request *request,
    yvex_model_target_report *report,
    yvex_error *err)
{
    if (quant_route_has_flag(request, "--role-support") ||
        quant_route_has_flag(request, "--roles") ||
        quant_route_has_flag(request, "--gate")) {
        return yvex_qtype_role_support_report_build(request, report, err);
    }
    return yvex_qtype_policy_report_build(request, report, err);
}
