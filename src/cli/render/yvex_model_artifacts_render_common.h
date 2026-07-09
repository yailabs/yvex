/*
 * yvex_model_artifacts_render_common.h - shared model-artifacts render helpers.
 *
 * Owner:
 *   src/cli/render
 *
 * Owns:
 *   shared declarations for the current model-artifacts CLI rendering helpers
 *   while the historical command-family output is moved out of surface files.
 *
 * Does not own:
 *   command routing, domain algorithms, artifact emission, runtime generation,
 *   eval, benchmark, or release claims.
 *
 * Invariants:
 *   render helpers are CLI-only and must not enter CORE_SRCS/libyvex.a.
 *
 * Boundary:
 *   rendering current diagnostic/report-only model-artifacts facts does not
 *   promote artifacts to generation-capable runtime support.
 */
#ifndef YVEX_MODEL_ARTIFACTS_RENDER_COMMON_H
#define YVEX_MODEL_ARTIFACTS_RENDER_COMMON_H

#include "yvex_model_artifacts_surface_common.h"

#endif
