/*
 * candidates.h - target candidate report boundary.
 *
 * Owner: src/model/target
 * Owns: candidate report declarations.
 * Does not own: CLI parsing, rendering, runtime, generation, benchmark, or release readiness.
 * Invariants: candidate facts remain blocked/report-only until promoted by proof.
 * Boundary: candidate reports do not create runtime capability.
 */
#ifndef YVEX_MODEL_TARGET_CANDIDATES_H
#define YVEX_MODEL_TARGET_CANDIDATES_H

#include "report.h"

int yvex_model_target_candidate_report_build(
    const yvex_model_target_request *request,
    yvex_model_target_report *report,
    yvex_error *err);

#endif
