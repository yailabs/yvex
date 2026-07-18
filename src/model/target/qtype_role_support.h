/*
 * qtype_role_support.h - qtype role-support report boundary.
 *
 * Owner: src/model/target
 * Owns: qtype role-support report declarations.
 * Does not own: CLI parsing, rendering, quantization execution, runtime, generation, benchmark, or release readiness.
 * Invariants: role-support matrices are report-only support facts.
 * Boundary: qtype role-support reporting is not qtype support completion.
 */
#ifndef YVEX_QTYPE_ROLE_SUPPORT_REPORT_H
#define YVEX_QTYPE_ROLE_SUPPORT_REPORT_H

#include "report.h"

int yvex_qtype_role_support_report_build(
    const yvex_model_target_request *request,
    yvex_model_target_report *report,
    yvex_error *err);

#endif
