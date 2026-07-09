/*
 * yvex_fullmodel_render.h - fullmodel render owner.
 *
 * Owner: src/cli/render
 * Owns: fullmodel render entrypoint declarations.
 * Does not own: graph execution, materialization algorithms, runtime
 * generation, eval, benchmark, or release claims.
 * Invariants: CLI-only and excluded from CORE_SRCS/libyvex.a.
 * Boundary: rendering fullmodel diagnostics is not full-runtime support.
 */
#ifndef YVEX_FULLMODEL_RENDER_H
#define YVEX_FULLMODEL_RENDER_H

#include "yvex_fullmodel_surface.h"

int yvex_fullmodel_render_command(int arg_count, char **args);
void yvex_fullmodel_render_help(FILE *fp);

#endif
