/*
 * artifacts.h - models artifacts list/status surface.
 * Owner: src/cli/model_artifacts
 * Owns: models artifacts surface declarations for current list/status output.
 * Does not own: artifact identity, model gates, runtime generation, artifact
 * emission, eval, benchmark, or release claims.
 * Invariants: CLI-only and excluded from libyvex.a.
 * Boundary: discovered artifacts are report-only facts.
 */
#ifndef YVEX_MODELS_ARTIFACTS_SURFACE_H
#define YVEX_MODELS_ARTIFACTS_SURFACE_H

#include "surface_common.h"

#define YVEX_MODELS_ARTIFACT_ROWS_CAP 256u

int yvex_models_artifacts_surface_command(int arg_count, char **args);

#endif
