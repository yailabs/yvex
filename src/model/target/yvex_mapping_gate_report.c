/*
 * yvex_mapping_gate_report.c - tensor mapping gate report builder.
 *
 * Owner:
 *   src/model/target
 *
 * Owns:
 *   tensor mapping gate report construction entry point.
 *
 * Does not own:
 *   CLI parsing, rendering, artifact emission, quantization execution,
 *   runtime execution, generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   mapping gates aggregate report-only facts and hand off to later rows.
 *
 * Boundary:
 *   mapping gate status is not quantization, artifact emission, runtime
 *   readiness, generation readiness, benchmark evidence, or release readiness.
 */
#include "yvex_mapping_gate_report.h"

#include "yvex_model_target_private.h"

static int mapping_gate_kind_allowed(
    const yvex_model_target_request *request)
{
    return request && request->kind == YVEX_MODEL_TARGET_COMMAND_TENSOR_MAP;
}

static int mapping_gate_reject(yvex_error *err)
{
    yvex_error_set(err, YVEX_ERR_INVALID_ARG, "mapping_gate_report",
                   "mapping gate report requires tensor-map command kind");
    return YVEX_ERR_INVALID_ARG;
}

static void mapping_gate_prepare_report(yvex_model_target_report *report)
{
    if (!report) {
        return;
    }
    report->kind = YVEX_MODEL_TARGET_COMMAND_TENSOR_MAP;
    report->status = "mapping-gate";
}

int yvex_mapping_gate_report_build(
    const yvex_model_target_request *request,
    yvex_model_target_report *report,
    yvex_error *err)
{
    if (!mapping_gate_kind_allowed(request)) {
        return mapping_gate_reject(err);
    }
    mapping_gate_prepare_report(report);
    return yvex_model_target_internal_report_build(request, report, err);
}

/*
 * Ownership note:
 *   The mapping-gate module is intentionally the only public entry point for
 *   mapping gate reports. The shared backend preserves the existing output
 *   contract for this row, while the coordinator only routes the typed request.
 *   Future rows that replace the shared backend should move gate profile
 *   construction here without changing command syntax.
 *
 * Report boundary:
 *   The gate may say a mapping is blocked, incomplete, or ready for the next
 *   report row. It may not say quantization ran, a GGUF artifact exists, or a
 *   runtime can execute the model.
 *
 * Test boundary:
 *   Layout tests require this module to remain a real ownership file with an
 *   exported builder. Keeping the guard here prevents the coordinator from
 *   absorbing gate-specific request routing again.
 *
 * Runtime boundary:
 *   Gate facts do not execute graph, backend, decode, logits, or sampling
 *   paths.
 */
