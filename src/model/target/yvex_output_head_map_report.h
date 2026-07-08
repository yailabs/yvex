/*
 * yvex_output_head_map_report.h - output-head map report boundary.
 *
 * Owner: src/model/target
 * Owns: output-head map report declarations.
 * Does not own: CLI parsing, rendering, logits execution, runtime, generation, benchmark, or release readiness.
 * Invariants: output-head facts are metadata/header-only.
 * Boundary: output-head mapping is not logits readiness.
 */
#ifndef YVEX_OUTPUT_HEAD_MAP_REPORT_H
#define YVEX_OUTPUT_HEAD_MAP_REPORT_H

#include "yvex_model_target_report.h"

int yvex_output_head_map_report_build(
    const yvex_model_target_request *request,
    yvex_model_target_report *report,
    yvex_error *err);

#endif
