/*
 * yvex_model_artifacts_args.h - model artifact CLI argument parsing.
 *
 * Owner:
 *   src/cli/input
 *
 * Owns:
 *   typed argument result for model artifact report commands.
 *
 * Does not own:
 *   registry lookup, model gate checks, report building, rendering,
 *   stdout/stderr, artifact emission, runtime generation, eval, benchmark, or
 *   release decisions.
 *
 * Invariants:
 *   argv is converted into typed request fields before domain/report code runs.
 *
 * Boundary:
 *   parsing model artifact args is not artifact emission or runtime support.
 */
#ifndef YVEX_MODEL_ARTIFACTS_ARGS_H
#define YVEX_MODEL_ARTIFACTS_ARGS_H

#include "yvex_model_artifact_report.h"

typedef struct {
    yvex_model_artifact_report_request request;
    int help_requested;
} yvex_model_artifacts_args;

int yvex_model_artifacts_args_parse(int argc,
                                    char **argv,
                                    yvex_model_artifacts_args *out,
                                    yvex_error *err);

#endif
