/*
 * yvex_models_download_render.h - models download render owner.
 *
 * Owner: src/cli/render
 * Owns: models download render/control entrypoint declarations.
 * Does not own: provider account state, source trust, artifact emission,
 * runtime generation, eval, benchmark, or release claims.
 * Invariants: CLI-only and excluded from CORE_SRCS/libyvex.a.
 * Boundary: rendering download state does not make downloaded bytes trusted.
 */
#ifndef YVEX_MODELS_DOWNLOAD_RENDER_H
#define YVEX_MODELS_DOWNLOAD_RENDER_H

#include "yvex_models_download_surface.h"

int yvex_models_download_render_command(int arg_count, char **args);

#endif
