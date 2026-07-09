/*
 * yvex_models_artifacts_render.h - models artifacts render owner.
 *
 * Owner: src/cli/render
 * Owns: models artifacts render entrypoint declarations.
 * Does not own: artifact identity algorithms, artifact emission, runtime
 * generation, eval, benchmark, or release claims.
 * Invariants: CLI-only and excluded from CORE_SRCS/libyvex.a.
 * Boundary: rendering artifact list/status facts is not artifact production.
 */
#ifndef YVEX_MODELS_ARTIFACTS_RENDER_H
#define YVEX_MODELS_ARTIFACTS_RENDER_H

#include "yvex_models_artifacts_surface.h"

int yvex_models_artifacts_render_command(int arg_count, char **args);

#endif
