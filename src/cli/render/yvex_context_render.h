/*
 * yvex_context_render.h - context render owner.
 *
 * Owner: src/cli/render
 * Owns: context render entrypoint declarations.
 * Does not own: runtime context execution, graph execution, artifact emission,
 * eval, benchmark, or release claims.
 * Invariants: CLI-only and excluded from CORE_SRCS/libyvex.a.
 * Boundary: rendering context diagnostics is not runtime context support.
 */
#ifndef YVEX_CONTEXT_RENDER_H
#define YVEX_CONTEXT_RENDER_H

#include "yvex_context_surface.h"

int yvex_context_render_command(int arg_count, char **args);
void yvex_context_render_help(FILE *fp);

#endif
