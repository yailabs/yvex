/*
 * yvex_attention_render.h - attention render owner.
 *
 * Owner: src/cli/render
 * Owns: attention render entrypoint declarations.
 * Does not own: attention runtime execution, graph execution, backend kernels,
 * artifact emission, eval, benchmark, or release claims.
 * Invariants: CLI-only and excluded from CORE_SRCS/libyvex.a.
 * Boundary: rendering attention diagnostics is not attention-backed runtime.
 */
#ifndef YVEX_ATTENTION_RENDER_H
#define YVEX_ATTENTION_RENDER_H

#include "yvex_attention_surface.h"

int yvex_attention_render_command(int arg_count, char **args);
void yvex_attention_render_help(FILE *fp);

#endif
