/*
 * yvex_model_artifact_status_report.h - typed model artifact status report.
 *
 * Owner:
 *   src/model/artifacts
 *
 * Owns:
 *   status report builder declaration for model artifacts.
 *
 * Does not own:
 *   CLI parsing, command dispatch, rendering, stdout/stderr, file writing,
 *   artifact emission, runtime generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   builders return typed facts through yvex_model_artifact_report.
 *
 * Boundary:
 *   status reports are not runtime or release readiness evidence.
 */
#ifndef YVEX_MODEL_ARTIFACT_STATUS_REPORT_H
#define YVEX_MODEL_ARTIFACT_STATUS_REPORT_H

#include "yvex_model_artifact_report.h"

int yvex_model_artifact_status_report_build(const yvex_model_artifact_report_request *request,
           yvex_model_artifact_report *report,
           yvex_error *err);

#endif
