/*
 * yvex_tokenizer_map_report.c - tokenizer map report builder.
 *
 * Owner:
 *   src/model/target
 *
 * Owns:
 *   tokenizer map report construction entry point.
 *
 * Does not own:
 *   CLI parsing, rendering, tokenizer runtime, generation, eval, benchmark, or
 *   release decisions.
 *
 * Invariants:
 *   tokenizer map reports inspect metadata sidecars only.
 *
 * Boundary:
 *   tokenizer mapping is not tokenizer runtime support, generation readiness,
 *   benchmark evidence, or release readiness.
 */
#include "yvex_tokenizer_map_report.h"

#include "yvex_model_target_private.h"

static int tokenizer_map_kind_allowed(
    const yvex_model_target_request *request)
{
    return request &&
           (request->kind == YVEX_MODEL_TARGET_COMMAND_TENSOR_MAP ||
            request->kind == YVEX_MODEL_TARGET_COMMAND_TOKENIZER_MAP);
}

static int tokenizer_map_reject(yvex_error *err)
{
    yvex_error_set(err, YVEX_ERR_INVALID_ARG, "tokenizer_map_report",
                   "tokenizer map report requires tokenizer map command kind");
    return YVEX_ERR_INVALID_ARG;
}

static void tokenizer_map_prepare_report(yvex_model_target_report *report)
{
    if (!report) {
        return;
    }
    report->kind = YVEX_MODEL_TARGET_COMMAND_TENSOR_MAP;
    report->status = "tokenizer-map";
}

int yvex_tokenizer_map_report_build(
    const yvex_model_target_request *request,
    yvex_model_target_report *report,
    yvex_error *err)
{
    if (!tokenizer_map_kind_allowed(request)) {
        return tokenizer_map_reject(err);
    }
    tokenizer_map_prepare_report(report);
    return yvex_model_target_internal_report_build(request, report, err);
}

/*
 * Ownership note:
 *   Tokenizer map reports inspect metadata sidecars only. The coordinator does
 *   not know tokenizer-map details; it routes tensor-map tokenizer requests to
 *   this module. Future sidecar parsing and typed tokenizer profile fields
 *   should remain here.
 *
 * Report boundary:
 *   Tokenizer mapping is metadata accounting. It is not tokenizer runtime
 *   encoding/decoding support for full generation.
 *
 * Test boundary:
 *   Layout tests require this module to remain a real ownership file with an
 *   exported builder. Keeping the guard here prevents the coordinator from
 *   absorbing tokenizer-map request routing again.
 *
 * Runtime boundary:
 *   Tokenizer map facts do not execute tokenizer runtime, graph, logits, or
 *   sampling paths.
 */
