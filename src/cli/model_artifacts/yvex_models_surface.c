/*
 * yvex_models_surface.c - models namespace surface router.
 *
 * Owner: src/cli/model_artifacts
 * Owns: forwarding from the historical models surface symbol to the models
 * render owner.
 * Does not own: output formatting, argv parsing bodies, registry storage,
 * artifact emission, runtime generation, eval, benchmark, or release claims.
 * Invariants: CLI-only and excluded from CORE_SRCS/libyvex.a.
 * Boundary: routing existing model command behavior does not imply runtime
 * support.
 */
#include "yvex_models_surface.h"
#include "yvex_models_render.h"

int yvex_model_artifacts_surface_models_command(int arg_count, char **args)
{
    return yvex_models_render_command(arg_count, args);
}

void yvex_model_artifacts_surface_models_help(FILE *fp)
{
    yvex_models_render_help(fp);
}
