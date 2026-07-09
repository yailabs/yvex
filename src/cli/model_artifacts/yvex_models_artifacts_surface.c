/*
 * yvex_models_artifacts_surface.c - models artifacts surface router.
 *
 * Owner: src/cli/model_artifacts
 * Owns: forwarding from the historical models artifacts surface symbol to the
 * render owner.
 * Does not own: output formatting, argv parsing bodies, artifact inspection
 * algorithms, artifact emission, runtime generation, eval, benchmark, or
 * release claims.
 * Invariants: CLI-only and excluded from CORE_SRCS/libyvex.a.
 * Boundary: routing current artifact listing/status behavior does not make
 * artifacts generation-capable.
 */
#include "yvex_models_artifacts_surface.h"
#include "yvex_models_artifacts_render.h"

int yvex_models_artifacts_surface_command(int arg_count, char **args)
{
    return yvex_models_artifacts_render_command(arg_count, args);
}
