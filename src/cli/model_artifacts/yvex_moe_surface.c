/*
 * yvex_moe_surface.c - MoE surface router.
 *
 * Owner: src/cli/model_artifacts
 * Owns: forwarding from historical MoE surface symbols to the MoE render
 * owner.
 * Does not own: output formatting, argv parsing bodies, MoE runtime, backend
 * execution, artifact emission, eval, benchmark, or release claims.
 * Invariants: CLI-only and excluded from CORE_SRCS/libyvex.a.
 * Boundary: MoE routing preserves diagnostic/report-only behavior.
 */
#include "yvex_moe_surface.h"
#include "yvex_moe_render.h"

int yvex_model_artifacts_surface_moe_command(int arg_count, char **args)
{
    return yvex_moe_render_command(arg_count, args);
}

void yvex_model_artifacts_surface_moe_help(FILE *fp)
{
    yvex_moe_render_help(fp);
}
