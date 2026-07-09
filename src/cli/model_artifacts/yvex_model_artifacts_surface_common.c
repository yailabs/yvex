/*
 * yvex_model_artifacts_surface_common.c - model-artifacts surface common unit.
 *
 * Owner:
 *   src/cli/model_artifacts
 *
 * Owns:
 *   the CLI-only surface common compilation unit after shared formatting was
 *   moved to src/cli/render.
 *
 * Does not own:
 *   output formatting, argv parsing bodies, domain algorithms, artifact
 *   emission, runtime generation, eval, benchmark, or release claims.
 *
 * Invariants:
 *   this file emits no operator output and stays outside CORE_SRCS/libyvex.a.
 *
 * Boundary:
 *   surface common declarations preserve legacy command-family linkage only.
 */
#include "yvex_model_artifacts_surface_common.h"
