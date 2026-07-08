/*
 * yvex_tensor_naming_report.h - tensor naming report boundary.
 *
 * Owner: src/model/target
 * Owns: tensor naming report declarations.
 * Does not own: CLI parsing, rendering, tensor payload loading, runtime, generation, benchmark, or release readiness.
 * Invariants: naming reports are lexical/header-only until mapped by explicit rows.
 * Boundary: tensor naming is not artifact emission or runtime readiness.
 */
#ifndef YVEX_TENSOR_NAMING_REPORT_H
#define YVEX_TENSOR_NAMING_REPORT_H

#include "yvex_model_target_report.h"

int yvex_tensor_naming_report_build(
    const yvex_model_target_request *request,
    yvex_model_target_report *report,
    yvex_error *err);

#endif
