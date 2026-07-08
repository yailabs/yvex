/*
 * yvex_mapping_gate_report.h - tensor mapping gate report boundary.
 *
 * Owner: src/model/target
 * Owns: tensor mapping gate report declarations.
 * Does not own: CLI parsing, rendering, artifact emission, runtime, generation, benchmark, or release readiness.
 * Invariants: mapping gates aggregate report-only facts.
 * Boundary: mapping gate pass/fail is not quantization or runtime readiness.
 */
#ifndef YVEX_MAPPING_GATE_REPORT_H
#define YVEX_MAPPING_GATE_REPORT_H

#include "yvex_model_target_report.h"

int yvex_mapping_gate_report_build(
    const yvex_model_target_request *request,
    yvex_model_target_report *report,
    yvex_error *err);

#endif
