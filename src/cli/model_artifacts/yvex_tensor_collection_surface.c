/*
 * yvex_tensor_collection_surface.c - tensor-collection surface router.
 *
 * Owner: src/cli/model_artifacts
 * Owns: forwarding from historical tensor-collection surface symbols to the
 * tensor-collection render owner.
 * Does not own: output formatting, argv parsing bodies, tensor collection
 * algorithms, backend execution, artifact emission, eval, benchmark, or release
 * claims.
 * Invariants: CLI-only and excluded from CORE_SRCS/libyvex.a.
 * Boundary: tensor-collection routing preserves diagnostic/report-only
 * behavior.
 */
#include "yvex_tensor_collection_surface.h"
#include "yvex_tensor_collection_render.h"

int yvex_model_artifacts_surface_tensor_collection_command(int arg_count, char **args)
{
    return yvex_tensor_collection_render_command(arg_count, args);
}

void yvex_model_artifacts_surface_tensor_collection_help(FILE *fp)
{
    yvex_tensor_collection_render_help(fp);
}
