/*
 * yvex_tensor_naming_report.c - tensor naming report builder.
 *
 * Owner:
 *   src/model/target
 *
 * Owns:
 *   tensor naming report construction entry point.
 *
 * Does not own:
 *   CLI parsing, rendering, tensor payload loading, artifact emission, runtime
 *   execution, generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   naming reports use native tensor names and header metadata only; lexical
 *   naming facts do not become runtime role mapping unless a mapping row owns
 *   that promotion.
 *
 * Boundary:
 *   tensor naming is not artifact emission, runtime support, generation
 *   readiness, benchmark evidence, or release readiness.
 */
#include "yvex_tensor_naming_report.h"

#include "yvex_mapping_gate_report.h"
#include "yvex_missing_role_report.h"
#include "yvex_model_target_private.h"
#include "yvex_output_head_map_report.h"
#include "yvex_tokenizer_map_report.h"

#include <string.h>

static int tensor_route_has_flag(const yvex_model_target_request *request,
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

static const char *tensor_route_value_after(
    const yvex_model_target_request *request,
    const char *flag)
{
    int i;

    if (!request || !flag) {
        return NULL;
    }
    for (i = 0; i + 1 < request->argc; ++i) {
        if (request->argv[i] && strcmp(request->argv[i], flag) == 0) {
            return request->argv[i + 1];
        }
    }
    return NULL;
}

static int tensor_naming_kind_allowed(
    const yvex_model_target_request *request)
{
    return request && request->kind == YVEX_MODEL_TARGET_COMMAND_TENSOR_MAP;
}

static int tensor_naming_reject(yvex_error *err)
{
    yvex_error_set(err, YVEX_ERR_INVALID_ARG, "tensor_naming_report",
                   "tensor naming report requires tensor-map command kind");
    return YVEX_ERR_INVALID_ARG;
}

static void tensor_naming_prepare_report(yvex_model_target_report *report)
{
    if (!report) {
        return;
    }
    report->kind = YVEX_MODEL_TARGET_COMMAND_TENSOR_MAP;
    report->status = "tensor-map";
}

int yvex_tensor_naming_report_build(
    const yvex_model_target_request *request,
    yvex_model_target_report *report,
    yvex_error *err)
{
    if (!tensor_naming_kind_allowed(request)) {
        return tensor_naming_reject(err);
    }
    tensor_naming_prepare_report(report);
    return yvex_model_target_internal_report_build(request, report, err);
}

int yvex_model_target_tensor_route_report_build(
    const yvex_model_target_request *request,
    yvex_model_target_report *report,
    yvex_error *err)
{
    const char *role;

    if (tensor_route_has_flag(request, "--gate")) {
        return yvex_mapping_gate_report_build(request, report, err);
    }
    role = tensor_route_value_after(request, "--role");
    if (role && strcmp(role, "output-head") == 0) {
        return yvex_output_head_map_report_build(request, report, err);
    }
    if (role && strcmp(role, "tokenizer") == 0) {
        return yvex_tokenizer_map_report_build(request, report, err);
    }
    if (role && strcmp(role, "missing-roles") == 0) {
        return yvex_missing_role_report_build(request, report, err);
    }
    if (request && request->kind == YVEX_MODEL_TARGET_COMMAND_TOKENIZER_MAP) {
        return yvex_tokenizer_map_report_build(request, report, err);
    }
    if (request && request->kind == YVEX_MODEL_TARGET_COMMAND_MISSING_ROLES) {
        return yvex_missing_role_report_build(request, report, err);
    }
    return yvex_tensor_naming_report_build(request, report, err);
}
