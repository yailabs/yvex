/*
 * yvex_models_download_surface.c - models download surface router.
 *
 * Owner: src/cli/model_artifacts
 * Owns: forwarding from the historical models download surface symbol to the
 * download render/control owner.
 * Does not own: output formatting, argv parsing bodies, provider process
 * control, file writing, artifact emission, runtime generation, eval,
 * benchmark, or release claims.
 * Invariants: CLI-only and excluded from CORE_SRCS/libyvex.a.
 * Boundary: download routing does not make downloaded bytes trusted runtime
 * artifacts.
 */
#include "yvex_models_download_surface.h"
#include "yvex_models_download_render.h"

int yvex_models_download_surface_command(int arg_count, char **args)
{
    return yvex_models_download_render_command(arg_count, args);
}
