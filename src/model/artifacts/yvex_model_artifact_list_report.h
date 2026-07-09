/*
 * yvex_model_artifact_list_report.h - typed model artifact list report.
 *
 * Owner:
 *   src/model/artifacts
 *
 * Owns:
 *   list report builder declaration for model artifacts.
 *
 * Does not own:
 *   CLI parsing, command dispatch, rendering, stdout/stderr, file writing,
 *   artifact emission, runtime generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   builders return typed facts through yvex_model_artifact_report.
 *
 * Boundary:
 *   list reports are not runtime or release readiness evidence.
 */
#ifndef YVEX_MODEL_ARTIFACT_LIST_REPORT_H
#define YVEX_MODEL_ARTIFACT_LIST_REPORT_H

#include "yvex_model_artifact_report.h"

int yvex_model_artifact_list_report_build(const yvex_model_artifact_report_request *request,
           yvex_model_artifact_report *report,
           yvex_error *err);

#endif
