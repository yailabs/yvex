/*
 * yvex_model_artifact_check_report.c - typed model artifact check report.
 *
 * Owner:
 *   src/model/artifacts
 *
 * Owns:
 *   check report construction entrypoint for model artifacts.
 *
 * Does not own:
 *   command argument parsing, command dispatch, renderer formatting, stdout/stderr,
 *   explicit file writing, artifact emission, runtime generation, eval,
 *   benchmark, or release decisions.
 *
 * Invariants:
 *   this module normalizes the request kind and delegates common typed fact
 *   initialization to the model artifact report coordinator.
 *
 * Boundary:
 *   check report facts are not artifact emission, runtime descriptors,
 *   generation readiness, benchmark evidence, or release readiness.
 */
#include "yvex_model_artifact_check_report.h"

#include <string.h>

int yvex_model_artifact_check_report_build(const yvex_model_artifact_report_request *request,
           yvex_model_artifact_report *report,
           yvex_error *err)
{
    yvex_model_artifact_report_request normalized;

    if (!request || !report) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_artifact_check_report",
                       "request and report are required");
        return YVEX_ERR_INVALID_ARG;
    }

    normalized = *request;
    normalized.kind = YVEX_MODEL_ARTIFACT_REPORT_CHECK;
    return yvex_model_artifact_report_build(&normalized, report, err);
}
