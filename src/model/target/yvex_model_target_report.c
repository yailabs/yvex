/*
 * yvex_model_target_report.c - model-target report coordinator.
 *
 * Owner:
 *   src/model/target
 *
 * Owns:
 *   unified model-target report dispatch, request routing, and shared report
 *   cleanup entry points.
 *
 * Does not own:
 *   target catalogs, target decisions, candidate facts, class profiles, tensor
 *   collection, tensor naming, output-head maps, tokenizer maps, missing-role
 *   facts, mapping gates, qtype facts, sidecar file writing, CLI parsing,
 *   command dispatch, rendering, stdout/stderr byte emission, runtime
 *   execution, generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   the coordinator routes typed requests to specialized model-target modules;
 *   it does not render, open operator streams, or contain report-specific
 *   static catalogs.
 *
 * Boundary:
 *   model-target reports are report-only facts. This coordinator does not
 *   implement quantization, artifact emission, runtime execution, generation,
 *   eval, benchmark, throughput, or release readiness.
 */
#include "yvex_model_target_report.h"

#include "yvex_model_class_profile.h"
#include "yvex_model_target_candidates.h"
#include "yvex_model_target_catalog.h"
#include "yvex_model_target_decision.h"
#include "yvex_model_target_private.h"
#include "yvex_tensor_collection_report.h"

int yvex_model_target_report_build(const yvex_model_target_request *request,
                                   yvex_model_target_report *report,
                                   yvex_error *err)
{
    if (!request || !report || !request->argv || request->argc <= 0) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_target_report",
                       "request, argc, argv, and report are required");
        return YVEX_ERR_INVALID_ARG;
    }

    switch (request->kind) {
    case YVEX_MODEL_TARGET_COMMAND_HELP:
        return yvex_model_target_help_report_build(report, err);
    case YVEX_MODEL_TARGET_COMMAND_CLASSES:
    case YVEX_MODEL_TARGET_COMMAND_LIST:
    case YVEX_MODEL_TARGET_COMMAND_INSPECT:
        return yvex_model_target_catalog_report_build(request, report, err);
    case YVEX_MODEL_TARGET_COMMAND_DECISION:
        return yvex_model_target_decision_report_build(request, report, err);
    case YVEX_MODEL_TARGET_COMMAND_CANDIDATE:
    case YVEX_MODEL_TARGET_COMMAND_DENSE_CANDIDATE:
    case YVEX_MODEL_TARGET_COMMAND_QWEN_METAL:
        return yvex_model_target_candidate_report_build(request, report, err);
    case YVEX_MODEL_TARGET_COMMAND_CLASS_PROFILE:
        return yvex_model_class_profile_report_build(request, report, err);
    case YVEX_MODEL_TARGET_COMMAND_TENSOR_COLLECTION:
        return yvex_tensor_collection_report_build(request, report, err);
    case YVEX_MODEL_TARGET_COMMAND_TENSOR_MAP:
        return yvex_model_target_tensor_route_report_build(request, report, err);
    case YVEX_MODEL_TARGET_COMMAND_TOKENIZER_MAP:
        return yvex_model_target_tensor_route_report_build(request, report, err);
    case YVEX_MODEL_TARGET_COMMAND_MISSING_ROLES:
        return yvex_model_target_tensor_route_report_build(request, report, err);
    case YVEX_MODEL_TARGET_COMMAND_QUANT_POLICY:
        return yvex_model_target_quant_route_report_build(request, report, err);
    case YVEX_MODEL_TARGET_COMMAND_UNKNOWN:
    default:
        return yvex_model_target_catalog_report_build(request, report, err);
    }
}

int yvex_model_target_help_report_build(yvex_model_target_report *report,
                                        yvex_error *err)
{
    return yvex_model_target_catalog_help_report_build(report, err);
}

void yvex_model_target_report_close(yvex_model_target_report *report)
{
    if (!report) {
        return;
    }
    yvex_model_target_runner_report_close(report);
}
