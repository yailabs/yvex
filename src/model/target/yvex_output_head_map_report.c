/*
 * yvex_output_head_map_report.c - output-head map report builder.
 *
 * Owner:
 *   src/model/target
 *
 * Owns:
 *   output-head map report construction entry point.
 *
 * Does not own:
 *   CLI parsing, rendering, logits execution, runtime execution, generation,
 *   eval, benchmark, or release decisions.
 *
 * Invariants:
 *   output-head map reports use metadata/header facts and do not prove logits
 *   readiness.
 *
 * Boundary:
 *   output-head mapping is not logits readiness, runtime support, generation
 *   readiness, benchmark evidence, or release readiness.
 */
#include "yvex_output_head_map_report.h"

#include "yvex_model_target_private.h"

static int output_head_kind_allowed(
    const yvex_model_target_request *request)
{
    return request && request->kind == YVEX_MODEL_TARGET_COMMAND_TENSOR_MAP;
}

static int output_head_reject(yvex_error *err)
{
    yvex_error_set(err, YVEX_ERR_INVALID_ARG, "output_head_map_report",
                   "output-head map report requires tensor-map command kind");
    return YVEX_ERR_INVALID_ARG;
}

static void output_head_prepare_report(yvex_model_target_report *report)
{
    if (!report) {
        return;
    }
    report->kind = YVEX_MODEL_TARGET_COMMAND_TENSOR_MAP;
    report->status = "output-head-map";
}

int yvex_output_head_map_report_build(
    const yvex_model_target_request *request,
    yvex_model_target_report *report,
    yvex_error *err)
{
    if (!output_head_kind_allowed(request)) {
        return output_head_reject(err);
    }
    output_head_prepare_report(report);
    return yvex_model_target_runner_report_build(request, report, err);
}

/*
 * Ownership note:
 *   The output-head module is the public owner for output-head map report
 *   construction. The shared backend is retained only to preserve behavior in
 *   this extraction row. Future typed profile construction for final-norm,
 *   embedding, and output-head relation facts belongs in this file.
 *
 * Report boundary:
 *   Output-head presence is metadata evidence. It does not prove logits can be
 *   computed, sampled, or generated over a supported-family runtime path.
 *
 * Test boundary:
 *   Layout tests require this module to remain a real ownership file with an
 *   exported builder. Keeping the guard here prevents the coordinator from
 *   absorbing output-head request routing again.
 *
 * Runtime boundary:
 *   The report may identify candidate tensors, but it never executes output
 *   projection, logits, or sampling code.
 *   It also does not mark decode or generation ready.
 */
