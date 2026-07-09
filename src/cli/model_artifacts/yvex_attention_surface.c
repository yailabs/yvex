/*
 * yvex_attention_surface.c - attention surface router.
 *
 * Owner: src/cli/model_artifacts
 * Owns: forwarding from historical attention surface symbols to the attention
 * render owner.
 * Does not own: output formatting, argv parsing bodies, attention runtime,
 * backend execution, artifact emission, eval, benchmark, or release claims.
 * Invariants: CLI-only and excluded from CORE_SRCS/libyvex.a.
 * Boundary: attention routing preserves diagnostic/report-only behavior.
 */
#include "yvex_attention_surface.h"
#include "yvex_attention_render.h"

int yvex_model_artifacts_surface_attention_command(int arg_count, char **args)
{
    return yvex_attention_render_command(arg_count, args);
}

void yvex_model_artifacts_surface_attention_help(FILE *fp)
{
    yvex_attention_render_help(fp);
}
