/*
 * yvex_tensor_collection_render.h - tensor-collection render owner.
 *
 * Owner: src/cli/render
 * Owns: tensor-collection render entrypoint declarations.
 * Does not own: tensor role algorithms, backend execution, artifact emission,
 * eval, benchmark, or release claims.
 * Invariants: CLI-only and excluded from CORE_SRCS/libyvex.a.
 * Boundary: rendering tensor-collection diagnostics is not graph/runtime
 * support.
 */
#ifndef YVEX_TENSOR_COLLECTION_RENDER_H
#define YVEX_TENSOR_COLLECTION_RENDER_H

#include "yvex_tensor_collection_surface.h"

int yvex_tensor_collection_render_command(int arg_count, char **args);
void yvex_tensor_collection_render_help(FILE *fp);

#endif
