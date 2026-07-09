/*
 * yvex_moe_render.h - MoE render owner.
 *
 * Owner: src/cli/render
 * Owns: MoE render entrypoint declarations.
 * Does not own: MoE runtime execution, graph execution, artifact emission,
 * eval, benchmark, or release claims.
 * Invariants: CLI-only and excluded from CORE_SRCS/libyvex.a.
 * Boundary: rendering MoE diagnostics is not MoE runtime support.
 */
#ifndef YVEX_MOE_RENDER_H
#define YVEX_MOE_RENDER_H

#include "yvex_moe_surface.h"

int yvex_moe_render_command(int arg_count, char **args);
void yvex_moe_render_help(FILE *fp);

#endif
