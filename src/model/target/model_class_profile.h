/*
 * model_class_profile.h - model-class profile report boundary.
 *
 * Owner: src/model/target
 * Owns: model-class profile declarations.
 * Does not own: CLI parsing, rendering, tensor payload loading, runtime, generation, benchmark, or release readiness.
 * Invariants: model-class profiles use metadata/header facts only.
 * Boundary: model-class profiling is not role mapping or runtime support.
 */
#ifndef YVEX_MODEL_CLASS_PROFILE_H
#define YVEX_MODEL_CLASS_PROFILE_H

#include "report.h"

struct yvex_source_verification;

int yvex_model_class_profile_report_build(
    const yvex_model_target_request *request,
    yvex_model_target_report *report,
    yvex_error *err);

int yvex_model_class_profile_deepseek_from_verification(
    const yvex_model_target_request *request,
    const struct yvex_source_verification *verification,
    yvex_model_target_report *report,
    yvex_error *err);

#endif
