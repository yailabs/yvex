/*
 * yvex_models_prepare_render.h - models prepare/check render owner.
 *
 * Owner: src/cli/render
 * Owns: prepare/check render entrypoint declarations.
 * Does not own: model gate algorithms, materialization, artifact emission,
 * runtime generation, eval, benchmark, or release claims.
 * Invariants: CLI-only and excluded from CORE_SRCS/libyvex.a.
 * Boundary: rendering prepare/check diagnostics is not generation readiness.
 */
#ifndef YVEX_MODELS_PREPARE_RENDER_H
#define YVEX_MODELS_PREPARE_RENDER_H

#include "yvex_models_prepare_surface.h"

int yvex_models_prepare_render_command(int arg_count, char **args);
int yvex_models_check_render_command(int arg_count, char **args);

#endif
