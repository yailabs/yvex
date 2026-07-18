/*
 * context.h - context command-family surface.
 * Owner: src/cli/model_artifacts
 * Owns: context surface declarations for existing CLI behavior.
 * Does not own: runtime implementation, graph execution, backend algorithms,
 * artifact emission, eval, benchmark, or release claims.
 * Invariants: CLI-only and excluded from libyvex.a.
 * Boundary: context output remains diagnostic/report-only.
 */
#ifndef YVEX_CONTEXT_SURFACE_H
#define YVEX_CONTEXT_SURFACE_H

#include "fullmodel.h"

int yvex_model_artifacts_surface_context_command(int arg_count, char **args);
void yvex_model_artifacts_surface_context_help(FILE *fp);

#endif
