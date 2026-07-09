/*
 * yvex_fullmodel_surface.c - fullmodel surface router.
 *
 * Owner: src/cli/model_artifacts
 * Owns: forwarding from historical fullmodel surface symbols to the fullmodel
 * render owner.
 * Does not own: output formatting, argv parsing bodies, tensor inventory
 * algorithms, materialization, runtime generation, eval, benchmark, or release
 * claims.
 * Invariants: CLI-only and excluded from CORE_SRCS/libyvex.a.
 * Boundary: fullmodel routing preserves report-only behavior.
 */
#include "yvex_fullmodel_surface.h"
#include "yvex_fullmodel_render.h"

int yvex_model_artifacts_surface_fullmodel_command(int arg_count, char **args)
{
    return yvex_fullmodel_render_command(arg_count, args);
}

void yvex_model_artifacts_surface_fullmodel_help(FILE *fp)
{
    yvex_fullmodel_render_help(fp);
}
