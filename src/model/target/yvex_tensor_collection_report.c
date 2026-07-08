/*
 * yvex_tensor_collection_report.c - tensor collection report builder.
 *
 * Owner:
 *   src/model/target
 *
 * Owns:
 *   tensor collection report construction entry point.
 *
 * Does not own:
 *   CLI parsing, rendering, tensor payload loading, tensor role mapping,
 *   artifact emission, runtime execution, generation, eval, benchmark, or
 *   release decisions.
 *
 * Invariants:
 *   tensor collection reports summarize header inventory facts and do not read
 *   tensor payload bytes.
 *
 * Boundary:
 *   tensor collection is not tensor role mapping, artifact emission, runtime
 *   support, generation readiness, benchmark evidence, or release readiness.
 */
#include "yvex_tensor_collection_report.h"

#include "yvex_model_target_private.h"

static int tensor_collection_kind_allowed(
    const yvex_model_target_request *request)
{
    return request &&
           request->kind == YVEX_MODEL_TARGET_COMMAND_TENSOR_COLLECTION;
}

static int tensor_collection_reject(yvex_error *err)
{
    yvex_error_set(err, YVEX_ERR_INVALID_ARG, "tensor_collection_report",
                   "tensor collection report requires tensor-collection command kind");
    return YVEX_ERR_INVALID_ARG;
}

static void tensor_collection_prepare_report(
    yvex_model_target_report *report)
{
    if (!report) {
        return;
    }
    report->kind = YVEX_MODEL_TARGET_COMMAND_TENSOR_COLLECTION;
    report->status = "tensor-collection";
}

int yvex_tensor_collection_report_build(
    const yvex_model_target_request *request,
    yvex_model_target_report *report,
    yvex_error *err)
{
    if (!tensor_collection_kind_allowed(request)) {
        return tensor_collection_reject(err);
    }
    tensor_collection_prepare_report(report);
    return yvex_model_target_internal_report_build(request, report, err);
}

/*
 * Ownership note:
 *   Tensor collection reports count header inventory and layer completeness
 *   facts. This module is the future home for collection profile structs and
 *   per-layer completeness accounting; the coordinator must stay unaware of
 *   those details.
 *
 * Report boundary:
 *   Collection completeness remains header inventory. It is not tensor
 *   materialization, residency, graph execution, or generation support.
 *
 * Test boundary:
 *   Layout tests require this module to remain a real ownership file with an
 *   exported builder. Keeping the guard here prevents the coordinator from
 *   absorbing collection request routing again.
 *
 * Runtime boundary:
 *   Collection facts do not execute graph, backend, decode, logits, or sampling
 *   paths.
 */
