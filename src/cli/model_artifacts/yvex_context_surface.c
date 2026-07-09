/*
 * yvex_context_surface.c - context surface router.
 *
 * Owner: src/cli/model_artifacts
 * Owns: forwarding from historical context surface symbols to the context
 * render owner.
 * Does not own: output formatting, argv parsing bodies, context runtime,
 * backend execution, artifact emission, eval, benchmark, or release claims.
 * Invariants: CLI-only and excluded from CORE_SRCS/libyvex.a.
 * Boundary: context routing preserves diagnostic/report-only behavior.
 */
#include "yvex_context_surface.h"
#include "yvex_context_render.h"

int yvex_model_artifacts_surface_context_command(int arg_count, char **args)
{
    return yvex_context_render_command(arg_count, args);
}

void yvex_model_artifacts_surface_context_help(FILE *fp)
{
    yvex_context_render_help(fp);
}
