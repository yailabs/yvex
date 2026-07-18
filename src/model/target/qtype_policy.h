/*
 * qtype_policy.h - qtype policy report boundary.
 *
 * Owner: src/model/target
 * Owns: qtype policy report declarations.
 * Does not own: CLI parsing, rendering, quantization execution, artifact emission, runtime, generation, benchmark, or release readiness.
 * Invariants: qtype policy facts are planning/report-only.
 * Boundary: qtype policy reporting is not quantization.
 */
#ifndef YVEX_QTYPE_POLICY_REPORT_H
#define YVEX_QTYPE_POLICY_REPORT_H

#include "report.h"

int yvex_qtype_policy_report_build(
    const yvex_model_target_request *request,
    yvex_model_target_report *report,
    yvex_error *err);

#endif
