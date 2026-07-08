/*
 * yvex_model_target_decision.h - target decision report boundary.
 *
 * Owner: src/model/target
 * Owns: target-decision report declarations.
 * Does not own: CLI parsing, rendering, runtime, generation, benchmark, or release readiness.
 * Invariants: decision facts remain report-only.
 * Boundary: target decisions do not select a runtime-ready model.
 */
#ifndef YVEX_MODEL_TARGET_DECISION_H
#define YVEX_MODEL_TARGET_DECISION_H

#include "yvex_model_target_report.h"

int yvex_model_target_decision_report_build(
    const yvex_model_target_request *request,
    yvex_model_target_report *report,
    yvex_error *err);

#endif
