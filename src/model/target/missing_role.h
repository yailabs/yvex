/*
 * missing_role.h - missing-role report boundary.
 *
 * Owner: src/model/target
 * Owns: missing runtime-role report declarations.
 * Does not own: CLI parsing, rendering, runtime execution, generation, benchmark, or release readiness.
 * Invariants: missing-role reports describe blockers only.
 * Boundary: missing-role reporting is not runtime support.
 */
#ifndef YVEX_MISSING_ROLE_REPORT_H
#define YVEX_MISSING_ROLE_REPORT_H

#include "report.h"

int yvex_missing_role_report_build(
    const yvex_model_target_request *request,
    yvex_model_target_report *report,
    yvex_error *err);

#endif
