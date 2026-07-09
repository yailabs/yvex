/*
 * yvex_models_prepare_surface.c - models prepare/check surface router.
 *
 * Owner: src/cli/model_artifacts
 * Owns: forwarding from historical prepare/check surface symbols to the
 * prepare render owner.
 * Does not own: output formatting, argv parsing bodies, model gates,
 * materialization algorithms, artifact emission, runtime generation, eval,
 * benchmark, or release claims.
 * Invariants: CLI-only and excluded from CORE_SRCS/libyvex.a.
 * Boundary: prepare/check routing preserves diagnostic behavior only.
 */
#include "yvex_models_prepare_surface.h"
#include "yvex_models_prepare_render.h"

int yvex_models_prepare_surface_command(int arg_count, char **args)
{
    return yvex_models_prepare_render_command(arg_count, args);
}

int yvex_models_check_surface_command(int arg_count, char **args)
{
    return yvex_models_check_render_command(arg_count, args);
}
