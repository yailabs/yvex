/*
 * yvex_model_class_profile.c - model-class profile report builder.
 *
 * Owner:
 *   src/model/target
 *
 * Owns:
 *   model-class profile report construction entry point.
 *
 * Does not own:
 *   CLI parsing, rendering, tensor payload loading, tensor role mapping,
 *   runtime execution, generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   model-class profile reports use metadata and safetensors header facts only.
 *
 * Boundary:
 *   model-class profiling is not tensor role mapping, runtime support,
 *   generation readiness, benchmark evidence, or release readiness.
 */
#include "yvex_model_class_profile.h"

#include "yvex_model_target_private.h"

static int model_class_profile_kind_allowed(
    const yvex_model_target_request *request)
{
    return request &&
           request->kind == YVEX_MODEL_TARGET_COMMAND_CLASS_PROFILE;
}

static int model_class_profile_reject(yvex_error *err)
{
    yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_class_profile",
                   "model class profile requires class-profile command kind");
    return YVEX_ERR_INVALID_ARG;
}

static void model_class_profile_prepare_report(
    yvex_model_target_report *report)
{
    if (!report) {
        return;
    }
    report->kind = YVEX_MODEL_TARGET_COMMAND_CLASS_PROFILE;
    report->status = "model-class-profile";
}

int yvex_model_class_profile_report_build(
    const yvex_model_target_request *request,
    yvex_model_target_report *report,
    yvex_error *err)
{
    if (!model_class_profile_kind_allowed(request)) {
        return model_class_profile_reject(err);
    }
    model_class_profile_prepare_report(report);
    return yvex_model_target_runner_report_build(request, report, err);
}

/*
 * Ownership note:
 *   Model-class source/config/tokenizer presence facts are routed through this
 *   module. Further decomposition of class profile structs should land here,
 *   not in the coordinator.
 *
 * Report boundary:
 *   Source sidecar presence and model-class metadata are intake facts. They do
 *   not prove materialization, graph execution, or generation support.
 *
 * Test boundary:
 *   Layout tests require this module to remain a real ownership file with an
 *   exported builder. Keeping the guard here prevents the coordinator from
 *   absorbing class-profile request routing again.
 *
 * Runtime boundary:
 *   Class profile facts do not execute graph, backend, decode, logits, or
 *   sampling paths.
 *   This module allocates no runtime state.
 */
