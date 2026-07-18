/*
 * mapping_gate.h - tensor mapping gate report boundary.
 *
 * Owner: src/model/target
 * Owns: canonical DeepSeek mapping-plan and legacy bounded mapping-gate report declarations.
 * Does not own: CLI parsing, rendering, artifact emission, runtime, generation, benchmark, or release readiness.
 * Invariants: DeepSeek reports project the canonical map; legacy family gates remain bounded evidence.
 * Boundary: mapping completion is not payload conversion, artifact emission, quantization, or runtime readiness.
 */
#ifndef YVEX_MAPPING_GATE_REPORT_H
#define YVEX_MAPPING_GATE_REPORT_H

#include "report.h"

int yvex_mapping_gate_report_build(
    const yvex_model_target_request *request,
    yvex_model_target_report *report,
    yvex_error *err);

int yvex_model_mapping_report_deepseek(
    const yvex_model_target_request *request,
    yvex_model_target_report *report,
    yvex_error *err);

#endif
