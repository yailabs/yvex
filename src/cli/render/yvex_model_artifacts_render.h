/*
 * yvex_model_artifacts_render.h - model artifact typed report renderer.
 *
 * Owner:
 *   src/cli/render
 *
 * Owns:
 *   model artifact normal/table/audit/help rendering declarations.
 *
 * Does not own:
 *   report building, registry lookup, model gate checks, backend calls,
 *   artifact emission, runtime generation, eval, benchmark, or release
 *   decisions.
 *
 * Invariants:
 *   renderer accepts typed reports and writes through CLI IO helpers.
 *
 * Boundary:
 *   rendering model artifact facts is not artifact emission or runtime support.
 */
#ifndef YVEX_MODEL_ARTIFACTS_RENDER_H
#define YVEX_MODEL_ARTIFACTS_RENDER_H

#include <stdio.h>
#include "yvex_model_artifact_report.h"

int yvex_model_artifacts_render(FILE *fp,
                                yvex_model_artifact_render_mode mode,
                                const yvex_model_artifact_report *report);
int yvex_model_artifacts_render_help(FILE *fp);

#endif
