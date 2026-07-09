/*
 * yvex_moe_surface.h - MoE command-family surface.
 * Owner: src/cli/model_artifacts
 * Owns: MoE surface declarations for existing CLI behavior.
 * Does not own: runtime implementation, graph execution, backend algorithms,
 * artifact emission, eval, benchmark, or release claims.
 * Invariants: CLI-only and excluded from libyvex.a.
 * Boundary: MoE output remains diagnostic/report-only.
 */
#ifndef YVEX_MOE_SURFACE_H
#define YVEX_MOE_SURFACE_H

#include "yvex_fullmodel_surface.h"

int yvex_model_artifacts_surface_moe_command(int arg_count, char **args);
void yvex_model_artifacts_surface_moe_help(FILE *fp);

#endif
