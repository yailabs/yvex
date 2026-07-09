/*
 * yvex_tensor_collection_surface.h - tensor-collection command-family surface.
 * Owner: src/cli/model_artifacts
 * Owns: tensor-collection surface declarations for existing CLI behavior.
 * Does not own: runtime implementation, graph execution, backend algorithms,
 * artifact emission, eval, benchmark, or release claims.
 * Invariants: CLI-only and excluded from libyvex.a.
 * Boundary: tensor-collection output remains diagnostic/report-only.
 */
#ifndef YVEX_TENSOR_COLLECTION_SURFACE_H
#define YVEX_TENSOR_COLLECTION_SURFACE_H

#include "yvex_fullmodel_surface.h"

int yvex_model_artifacts_surface_tensor_collection_command(int arg_count, char **args);
void yvex_model_artifacts_surface_tensor_collection_help(FILE *fp);

#endif
