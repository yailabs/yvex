/*
 * yvex_model_target_private.h - private model-target cell boundary.
 *
 * Owner:
 *   src/model/target
 *
 * Owns:
 *   private declarations shared by model-target catalog/report modules.
 *
 * Does not own:
 *   CLI argv parsing, command dispatch, rendering, stdout/stderr writing,
 *   runtime execution, generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   private model-target declarations stay under src/model/target and do not
 *   depend on CLI/operator headers.
 *
 * Boundary:
 *   model-target private helpers support report-only facts and do not create
 *   runtime, generation, benchmark, or release capability.
 */
#ifndef YVEX_MODEL_TARGET_PRIVATE_H
#define YVEX_MODEL_TARGET_PRIVATE_H

#include "yvex_model_target_report.h"

/*
 * yvex_model_target_internal_report_build()
 *
 * Purpose:
 *   execute the shared model-target report backend for a parsed request.
 *
 * Inputs:
 *   request borrows argc/argv; report receives owned report segments.
 *
 * Effects:
 *   builds report-only model-target output in owned memory and may write
 *   explicit local sidecar files through model-target sidecar paths.
 *
 * Failure:
 *   returns yvex errors for invalid arguments, allocation, or local IO.
 *
 * Boundary:
 *   this internal backend preserves existing report behavior and does not
 *   implement runtime execution, quantization, generation, eval, benchmark, or
 *   release readiness.
 */
int yvex_model_target_internal_report_build(
    const yvex_model_target_request *request,
    yvex_model_target_report *report,
    yvex_error *err);

int yvex_model_target_internal_help_report_build(
    yvex_model_target_report *report,
    yvex_error *err);

void yvex_model_target_internal_report_close(
    yvex_model_target_report *report);

int yvex_model_target_tensor_route_report_build(
    const yvex_model_target_request *request,
    yvex_model_target_report *report,
    yvex_error *err);

int yvex_model_target_quant_route_report_build(
    const yvex_model_target_request *request,
    yvex_model_target_report *report,
    yvex_error *err);

#endif
