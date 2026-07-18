/*
 * models.h - models namespace CLI surface.
 * Owner: src/cli/model_artifacts
 * Owns: models namespace surface declarations.
 * Does not own: download lifecycle, prepare/check gates, artifact reports,
 * runtime generation, or release claims.
 * Invariants: CLI-only and excluded from libyvex.a.
 * Boundary: registry command output is CLI surface behavior only.
 */
#ifndef YVEX_MODELS_SURFACE_H
#define YVEX_MODELS_SURFACE_H

#include "surface_common.h"

int yvex_model_artifacts_surface_models_command(int arg_count, char **args);
void yvex_model_artifacts_surface_models_help(FILE *fp);

#endif
