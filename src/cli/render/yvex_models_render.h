/*
 * yvex_models_render.h - models namespace render owner.
 *
 * Owner: src/cli/render
 * Owns: models namespace render entrypoint declarations.
 * Does not own: model registry storage, artifact emission, runtime generation,
 * eval, benchmark, or release claims.
 * Invariants: CLI-only and excluded from CORE_SRCS/libyvex.a.
 * Boundary: rendering registry facts does not imply runtime support.
 */
#ifndef YVEX_MODELS_RENDER_H
#define YVEX_MODELS_RENDER_H

#include "yvex_models_surface.h"

int yvex_models_render_command(int arg_count, char **args);
void yvex_models_render_help(FILE *fp);

#endif
